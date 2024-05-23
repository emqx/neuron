/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>

#include "connection/neu_connection_eth.h"
#include "event/event.h"
#include "utils/log.h"
#include "utils/utextend.h"

#ifndef ETH_P_LLDP
#define ETH_P_LLDP 0x88CC
#endif

typedef struct neu_conn_eth_sub {
    uint16_t protocol;
    char     mac[ETH_ALEN];
} callback_elem_key_t;

typedef struct {
    callback_elem_key_t key;

    neu_conn_eth_msg_callback callback;

    UT_hash_handle hh;
} callback_elem_t;

typedef struct {
    char device[NEU_NODE_NAME_LEN];

    neu_conn_eth_msg_callback callback;

    UT_hash_handle hh;
} lldp_callback_elem_t;

typedef struct interface_conn {
    char *        interface;
    neu_events_t *events;
    uint8_t       count;
    uint8_t       buf[1500];

    int     lldp_fd;
    int     profinet_fd;
    int     vlan_fd;
    uint8_t mac[ETH_ALEN];

    neu_event_io_t *profinet_event;
    neu_event_io_t *lldp_event;
    neu_event_io_t *vlan_event;

    lldp_callback_elem_t *lldp_callbacks;
    callback_elem_t *     callbacks;
    pthread_mutex_t       mtx;
} interface_conn_t;

static pthread_mutex_t mtx      = { 0 };
static bool            mtx_init = false;
#define interface_max 4
static interface_conn_t in_conns[interface_max];

struct neu_conn_eth {
    interface_conn_t *ic;
    void *            ctx;
};

static int get_mac(neu_conn_eth_t *conn, const char *interface);
static int init_socket(const char *interface, uint16_t protocol);
static int uninit_socket(int fd);

static int lldp_msg_cb(enum neu_event_io_type type, int fd, void *usr_data);
static int eth_msg_cb(enum neu_event_io_type type, int fd, void *usr_data);

static char *lldp_parse_port_id(uint16_t n_byte, uint8_t *bytes);

neu_conn_eth_t *neu_conn_eth_init(const char *interface, void *ctx)
{
    if (!mtx_init) {
        pthread_mutex_init(&mtx, NULL);
        mtx_init = true;
    }

    neu_conn_eth_t *conn_eth = calloc(1, sizeof(neu_conn_eth_t));
    int             i        = 0;
    bool            find     = false;

    pthread_mutex_lock(&mtx);

    for (i = 0; i < interface_max; i++) {
        if (in_conns[i].interface != NULL &&
            strcmp(in_conns[i].interface, interface) == 0) {
            in_conns[i].count += 1;
            conn_eth->ic = &in_conns[i];
            find         = true;
            break;
        }
    }
    if (!find) {
        for (i = 0; i < interface_max; i++) {
            if (in_conns[i].interface == NULL) {
                neu_event_io_param_t param = { 0 };

                in_conns[i].interface = strdup(interface);
                in_conns[i].events    = neu_event_new();
                in_conns[i].count += 1;

                in_conns[i].profinet_fd = init_socket(interface, 0x8892);
                in_conns[i].lldp_fd     = init_socket(interface, ETH_P_LLDP);
                in_conns[i].vlan_fd     = init_socket(interface, 0x8100);

                get_mac(conn_eth, interface);
                pthread_mutex_init(&in_conns[i].mtx, NULL);
                conn_eth->ic = &in_conns[i];

                param.fd       = in_conns[i].profinet_fd;
                param.usr_data = (void *) conn_eth;
                param.cb       = eth_msg_cb;

                in_conns[i].profinet_event =
                    neu_event_add_io(in_conns[i].events, param);

                param.fd = in_conns[i].lldp_fd;
                param.cb = lldp_msg_cb;

                in_conns[i].lldp_event =
                    neu_event_add_io(in_conns[i].events, param);

                param.fd = in_conns[i].vlan_fd;
                param.cb = eth_msg_cb;

                in_conns[i].vlan_event =
                    neu_event_add_io(in_conns[i].events, param);

                break;
            }
        }
    }

    pthread_mutex_unlock(&mtx);

    conn_eth->ctx = ctx;
    return conn_eth;
}

int neu_conn_eth_uninit(neu_conn_eth_t *conn)
{
    pthread_mutex_lock(&mtx);
    conn->ic->count -= 1;
    if (conn->ic->count == 0) {
        neu_event_del_io(conn->ic->events, conn->ic->profinet_event);
        neu_event_del_io(conn->ic->events, conn->ic->lldp_event);
        neu_event_del_io(conn->ic->events, conn->ic->vlan_event);

        neu_event_close(conn->ic->events);

        uninit_socket(conn->ic->profinet_fd);
        uninit_socket(conn->ic->lldp_fd);
        uninit_socket(conn->ic->vlan_fd);

        free(conn->ic->interface);
        pthread_mutex_destroy(&conn->ic->mtx);
    }
    pthread_mutex_unlock(&mtx);

    free(conn);

    return 0;
}

void neu_conn_eth_get_mac(neu_conn_eth_t *conn, uint8_t *mac)
{
    memcpy(mac, conn->ic->mac, ETH_ALEN);
}

int neu_conn_eth_check_interface(const char *interface)
{
    struct ifreq ifr = { 0 };
    int          fd  = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    int          ret = -1;

    strcpy(ifr.ifr_name, interface);
    ret = ioctl(fd, SIOCGIFHWADDR, &ifr);

    close(fd);
    return ret;
}

neu_conn_eth_sub_t *neu_conn_eth_register(neu_conn_eth_t *conn,
                                          uint16_t protocol, uint8_t mac[6],
                                          neu_conn_eth_msg_callback callback)
{
    callback_elem_key_t key  = { 0 };
    neu_conn_eth_sub_t *sub  = NULL;
    callback_elem_t *   elem = NULL;

    key.protocol = protocol;
    memcpy(key.mac, mac, ETH_ALEN);

    pthread_mutex_lock(&conn->ic->mtx);

    HASH_FIND(hh, conn->ic->callbacks, &key, sizeof(callback_elem_key_t), elem);
    if (elem == NULL) {
        elem = calloc(1, sizeof(callback_elem_t));
        sub  = calloc(1, sizeof(neu_conn_eth_sub_t));

        elem->key.protocol = protocol;
        memcpy(key.mac, mac, ETH_ALEN);
        elem->callback = callback;

        memcpy(sub->mac, mac, ETH_ALEN);
        sub->protocol = protocol;

        HASH_ADD(hh, conn->ic->callbacks, key, sizeof(callback_elem_key_t),
                 elem);
    }

    pthread_mutex_unlock(&conn->ic->mtx);

    return sub;
}

int neu_conn_eth_unregister(neu_conn_eth_t *conn, neu_conn_eth_sub_t *sub)
{
    callback_elem_key_t key  = { 0 };
    callback_elem_t *   elem = NULL;

    key.protocol = sub->protocol;
    memcpy(key.mac, sub->mac, ETH_ALEN);

    pthread_mutex_lock(&conn->ic->mtx);

    HASH_FIND(hh, conn->ic->callbacks, &key, sizeof(callback_elem_key_t), elem);
    if (elem != NULL) {
        HASH_DEL(conn->ic->callbacks, elem);
        free(elem);
    }

    pthread_mutex_unlock(&conn->ic->mtx);

    free(sub);

    return 0;
}

int neu_conn_eth_register_lldp(neu_conn_eth_t *conn, const char *device,
                               neu_conn_eth_msg_callback callback)
{
    lldp_callback_elem_t *elem = NULL;
    int                   ret  = 0;

    pthread_mutex_lock(&conn->ic->mtx);

    HASH_FIND_STR(conn->ic->lldp_callbacks, device, elem);
    if (elem != NULL) {
        ret = -1;
    } else {
        elem = calloc(1, sizeof(lldp_callback_elem_t));

        elem->callback = callback;
        strcpy(elem->device, device);

        HASH_ADD_STR(conn->ic->lldp_callbacks, device, elem);
    }

    pthread_mutex_unlock(&conn->ic->mtx);

    return ret;
}

int neu_conn_eth_unregister_lldp(neu_conn_eth_t *conn, const char *device)
{
    lldp_callback_elem_t *elem = NULL;

    pthread_mutex_lock(&conn->ic->mtx);
    HASH_FIND_STR(conn->ic->lldp_callbacks, device, elem);
    if (elem != NULL) {
        HASH_DEL(conn->ic->lldp_callbacks, elem);
        free(elem);
    }
    pthread_mutex_unlock(&conn->ic->mtx);

    return 0;
}

int neu_conn_eth_send(neu_conn_eth_t *conn, uint8_t dst_mac[6],
                      uint16_t protocol, uint16_t n_byte, uint8_t *bytes)
{
    int            fd   = 0;
    int            ret  = 0;
    struct ethhdr *ehdr = (struct ethhdr *) bytes;
    (void) dst_mac;

    switch (protocol) {
    case 0x8892:
        fd = conn->ic->profinet_fd;
        break;
    case ETH_P_LLDP:
        fd = conn->ic->lldp_fd;
        break;
    case 0x8100:
        fd = conn->ic->vlan_fd;
        break;
    default:
        assert(1 == 0);
        break;
    }

    memcpy(ehdr->h_source, conn->ic->mac, ETH_ALEN);

    ret = send(fd, bytes, n_byte, 0);
    return ret;
}

static int get_mac(neu_conn_eth_t *conn, const char *interface)
{
    struct ifreq ifr = { 0 };
    int          fd  = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    int          ret = -1;

    strcpy(ifr.ifr_name, interface);
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
        memcpy(conn->ic->mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
        ret = 0;
    }

    close(fd);
    return ret;
}

static int init_socket(const char *interface, uint16_t protocol)
{
    struct ifreq       ifr                     = { 0 };
    struct sockaddr_ll sll                     = { 0 };
    struct packet_mreq mreq                    = { 0 };
    const uint8_t      pn_mcast_addr[ETH_ALEN] = {
        0x01, 0x0e, 0xcf, 0x00, 0x00, 0x00
    };
    int fd  = socket(PF_PACKET, SOCK_RAW, htons(protocol));
    int ret = 0;

    assert(fd > 0);
    setsockopt(fd, SOL_SOCKET, SO_DONTROUTE, &ret, sizeof(ret));

    strcpy(ifr.ifr_name, interface);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) != 0) {
        close(fd);
        return -1;
    }

    sll.sll_family   = AF_PACKET;
    sll.sll_ifindex  = ifr.ifr_ifindex;
    sll.sll_protocol = htons(protocol);

    mreq.mr_ifindex = ifr.ifr_ifindex;
    mreq.mr_type    = PACKET_HOST | PACKET_MR_MULTICAST;
    mreq.mr_alen    = ETH_ALEN;
    memcpy(mreq.mr_address, pn_mcast_addr, ETH_ALEN);

    ifr.ifr_flags = 0;
    ioctl(fd, SIOCGIFFLAGS, &ifr);

    bind(fd, (struct sockaddr *) &sll, sizeof(sll));

    if (protocol == 0x8892) {
        ret = setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq,
                         sizeof(mreq));
        assert(ret == 0);
    }

    return fd;
}
static int uninit_socket(int fd)
{
    close(fd);

    return 0;
}

static int eth_msg_cb(enum neu_event_io_type type, int fd, void *usr_data)
{
    neu_conn_eth_t *conn = (neu_conn_eth_t *) usr_data;

    switch (type) {
    case NEU_EVENT_IO_READ: {
        callback_elem_key_t key  = { 0 };
        callback_elem_t *   elem = NULL;

        memset(conn->ic->buf, 0, sizeof(conn->ic->buf));
        int ret = recv(fd, conn->ic->buf, sizeof(conn->ic->buf), 0);

        if (ret < ETH_HLEN) {
            nlog_warn("eth recv length < %d msg", ETH_HLEN);
            return 0;
        }

        struct ethhdr *ehdr = (struct ethhdr *) conn->ic->buf;

        key.protocol = ntohs(ehdr->h_proto);
        memcpy(key.mac, ehdr->h_source, ETH_ALEN);

        HASH_FIND(hh, conn->ic->callbacks, &key, sizeof(key), elem);

        if (elem != NULL) {
            elem->callback(conn, conn->ctx, key.protocol, ret - ETH_HLEN,
                           conn->ic->buf + ETH_HLEN, ehdr->h_source);
        }

        break;
    }
    case NEU_EVENT_IO_CLOSED:
    case NEU_EVENT_IO_HUP:
        nlog_warn("eth conn eth error type: %d, fd: %d(%s)", type, fd,
                  conn->ic->interface);
        break;
    }

    return 0;
}

static int lldp_msg_cb(enum neu_event_io_type type, int fd, void *usr_data)
{
    neu_conn_eth_t *conn    = (neu_conn_eth_t *) usr_data;
    int             ret     = 0;
    char *          port_id = NULL;

    switch (type) {
    case NEU_EVENT_IO_READ:
        memset(conn->ic->buf, 0, sizeof(conn->ic->buf));
        ret     = recv(fd, conn->ic->buf, sizeof(conn->ic->buf), 0);
        port_id = lldp_parse_port_id(ret - sizeof(struct ethhdr),
                                     conn->ic->buf + sizeof(struct ethhdr));

        if (port_id != NULL) {
            lldp_callback_elem_t *elem = NULL;
            struct ethhdr *       ehdr = (struct ethhdr *) conn->ic->buf;

            HASH_FIND_STR(conn->ic->callbacks, port_id, elem);
            if (elem != NULL) {
                elem->callback(
                    conn, conn->ctx, ETH_P_LLDP, ret - sizeof(struct ethhdr),
                    conn->ic->buf + sizeof(struct ethhdr), ehdr->h_source);
            }
        }

        break;
    case NEU_EVENT_IO_CLOSED:
    case NEU_EVENT_IO_HUP:
        nlog_warn("eth conn lldp error type: %d, fd: %d(%s)", type, fd,
                  conn->ic->interface);
        break;
    }

    return 0;
}

struct lldp_tvl {
    uint16_t type : 7;
    uint16_t length : 9;
} __attribute__((packed));

#define END_OF_LLDPDU_TLV_TYPE 0x00
#define CHASSIS_ID_TLV_TYPE 0x01
#define PORT_ID_TLV_TYPE 0x02
#define TIME_TO_LIVE_TLV_TYPE 0x03

static char *lldp_parse_port_id(uint16_t n_byte, uint8_t *bytes)
{
    struct lldp_tvl *tvl = (struct lldp_tvl *) bytes;

    if (n_byte < sizeof(struct lldp_tvl) + tvl->length) {
        return NULL;
    }

    if (tvl->type == PORT_ID_TLV_TYPE) {
        static __thread char port_id[NEU_NODE_NAME_LEN] = { 0 };
        uint8_t *            data                       = (uint8_t *) &tvl[1];

        strncpy(port_id, (char *) data + 1, tvl->length - 1);
        return port_id;
    } else {
        return lldp_parse_port_id(
            n_byte - sizeof(struct lldp_tvl) - tvl->length,
            bytes + sizeof(struct lldp_tvl) + tvl->length);
    }
}