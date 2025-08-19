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
#include <errno.h>
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
#include "type.h"
#include "utils/log.h"
#include "utils/utextend.h"

#ifndef ETH_P_LLDP
#define ETH_P_LLDP 0x88CC
#endif

struct neu_conn_eth_sub {
    uint8_t mac[6];
};

typedef struct {
    char mac[20];

    neu_conn_eth_msg_callback callback;

    UT_hash_handle hh;
} callback_elem_t;

typedef struct {
    neu_conn_eth_msg_callback callback;

    UT_hash_handle hh;
} lldp_callback_elem_t;

typedef struct interface_conn {
    char *        interface;
    neu_events_t *events;
    uint8_t       count;

    int     profinet_fd;
    int     vlan_fd;
    uint8_t mac[ETH_ALEN];

    neu_event_io_t *profinet_event;
    neu_event_io_t *vlan_event;

    callback_elem_t *callbacks;
    pthread_mutex_t  mtx;
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
static int init_socket(const char *interface, uint16_t protocol,
                       uint8_t mac[6]);
static int uninit_socket(int fd);

static int     eth_msg_cb(enum neu_event_io_type type, int fd, void *usr_data);
static uint8_t pf_dcp_broadcast[ETH_ALEN] = {
    0x01, 0x0e, 0xcf, 0x00, 0x00, 0x01
};

int neu_conn_eth_size()
{
    int ret = 0;

    for (int i = 0; i < interface_max; i++) {
        ret += in_conns[i].count;
    }

    return ret;
}

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

                pthread_mutex_init(&in_conns[i].mtx, NULL);
                conn_eth->ic = &in_conns[i];

                in_conns[i].interface = strdup(interface);
                in_conns[i].events    = neu_event_new("eth_conn");
                in_conns[i].count += 1;

                get_mac(conn_eth, interface);

                in_conns[i].profinet_fd =
                    init_socket(interface, 0x8892, in_conns[i].mac);
                // in_conns[i].vlan_fd =
                // init_socket(interface, 0x8100, in_conns[i].mac);

                param.fd       = in_conns[i].profinet_fd;
                param.usr_data = (void *) conn_eth;
                param.cb       = eth_msg_cb;

                in_conns[i].profinet_event =
                    neu_event_add_io(in_conns[i].events, param);

                // param.fd = in_conns[i].vlan_fd;
                // param.cb = eth_msg_cb;

                // in_conns[i].vlan_event =
                // neu_event_add_io(in_conns[i].events, param);

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
        // neu_event_del_io(conn->ic->events, conn->ic->vlan_event);

        neu_event_close(conn->ic->events);

        uninit_socket(conn->ic->profinet_fd);
        // uninit_socket(conn->ic->vlan_fd);

        free(conn->ic->interface);
        conn->ic->interface = NULL;
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
    int          fd  = socket(PF_PACKET, SOCK_RAW, ETH_P_IPV6);
    int          ret = -1;

    if (fd <= 0) {
        return -1;
    }

    strcpy(ifr.ifr_name, interface);
    ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
    if (ret != 0) {
        return -1;
    }

    close(fd);
    return ret;
}

neu_conn_eth_sub_t *neu_conn_eth_register(neu_conn_eth_t *conn, uint8_t xmac[6],
                                          neu_conn_eth_msg_callback callback)
{
    char                tmp_mac[20] = { 0 };
    neu_conn_eth_sub_t *sub         = NULL;
    callback_elem_t *   elem        = NULL;

    snprintf(tmp_mac, sizeof(tmp_mac), "%X:%X:%X-%X:%X:%X", xmac[0], xmac[1],
             xmac[2], xmac[3], xmac[4], xmac[5]);

    pthread_mutex_lock(&conn->ic->mtx);

    HASH_FIND_STR(conn->ic->callbacks, tmp_mac, elem);
    if (elem == NULL) {
        elem = calloc(1, sizeof(callback_elem_t));
        sub  = calloc(1, sizeof(neu_conn_eth_sub_t));

        elem->callback = callback;
        memcpy(elem->mac, tmp_mac, sizeof(elem->mac));

        memcpy(sub->mac, xmac, ETH_ALEN);

        HASH_ADD_STR(conn->ic->callbacks, mac, elem);
    }

    pthread_mutex_unlock(&conn->ic->mtx);

    return sub;
}

int neu_conn_eth_unregister(neu_conn_eth_t *conn, neu_conn_eth_sub_t *sub)
{
    char             tmp_mac[20] = { 0 };
    callback_elem_t *elem        = NULL;

    snprintf(tmp_mac, sizeof(tmp_mac), "%X:%X:%X-%X:%X:%X", sub->mac[0],
             sub->mac[1], sub->mac[2], sub->mac[3], sub->mac[4], sub->mac[5]);

    pthread_mutex_lock(&conn->ic->mtx);

    HASH_FIND_STR(conn->ic->callbacks, tmp_mac, elem);
    if (elem != NULL) {
        HASH_DEL(conn->ic->callbacks, elem);
        free(elem);
    }

    pthread_mutex_unlock(&conn->ic->mtx);

    free(sub);

    return 0;
}

int neu_conn_eth_send(neu_conn_eth_t *conn, uint16_t protocol, uint16_t n_byte,
                      uint8_t *bytes)
{
    int fd  = 0;
    int ret = 0;

    switch (protocol) {
    case ETH_P_LLDP:
    case 0x8892:
        fd = conn->ic->profinet_fd;
        break;
    case 0x8100:
        fd = conn->ic->vlan_fd;
        break;
    default:
        assert(1 == 0);
        break;
    }

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

static int init_socket(const char *interface, uint16_t protocol, uint8_t mac[6])
{
    struct ifreq       ifr                     = { 0 };
    struct sockaddr_ll sll                     = { 0 };
    const uint8_t      pn_mcast_addr[ETH_ALEN] = {
        0x01, 0x0e, 0xcf, 0x00, 0x00, 0x00
    };
    int fd  = socket(PF_PACKET, SOCK_RAW, htons(protocol));
    int ret = 1;

    assert(fd > 0);
    setsockopt(fd, SOL_SOCKET, SO_DONTROUTE, &ret, sizeof(ret));

    (void) mac;
    (void) pn_mcast_addr;

    strcpy(ifr.ifr_name, interface);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) != 0) {
        close(fd);
        return -1;
    }

    sll.sll_family   = AF_PACKET;
    sll.sll_ifindex  = ifr.ifr_ifindex;
    sll.sll_protocol = htons(protocol);
    ret              = bind(fd, (struct sockaddr *) &sll, sizeof(sll));
    assert(ret == 0);

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
        callback_elem_t *elem      = NULL;
        uint8_t          buf[1500] = { 0 };
        int              ret       = recv(fd, buf, 1500, 0);

        if (ret < ETH_HLEN) {
            return 0;
        }

        struct ethhdr *ehdr = (struct ethhdr *) buf;
        ehdr->h_proto       = ntohs(ehdr->h_proto);

        if (ehdr->h_proto != 0x8100 && ehdr->h_proto != 0x8892) {
            return 0;
        }

        if (memcmp(ehdr->h_dest, pf_dcp_broadcast, ETH_ALEN) == 0) {
            callback_elem_t *tmp = NULL;

            HASH_ITER(hh, conn->ic->callbacks, elem, tmp)
            {
                elem->callback(conn, conn->ctx, ehdr->h_proto, ret - ETH_HLEN,
                               buf + ETH_HLEN, ehdr->h_source);
            }
        } else {
            if (memcmp(ehdr->h_dest, conn->ic->mac, ETH_ALEN) != 0) {
                return 0;
            }
            char tmp_mac[20] = { 0 };

            snprintf(tmp_mac, sizeof(tmp_mac), "%X:%X:%X-%X:%X:%X",
                     ehdr->h_source[0], ehdr->h_source[1], ehdr->h_source[2],
                     ehdr->h_source[3], ehdr->h_source[4], ehdr->h_source[5]);

            HASH_FIND_STR(conn->ic->callbacks, tmp_mac, elem);
            if (elem != NULL) {
                elem->callback(conn, conn->ctx, ehdr->h_proto, ret - ETH_HLEN,
                               buf + ETH_HLEN, ehdr->h_source);
            } else {
                memset(tmp_mac, 0, sizeof(tmp_mac));
                snprintf(tmp_mac, sizeof(tmp_mac), "0:0:0-0:0:0");
                HASH_FIND_STR(conn->ic->callbacks, tmp_mac, elem);
                if (elem != NULL) {
                    elem->callback(conn, conn->ctx, ehdr->h_proto,
                                   ret - ETH_HLEN, buf + ETH_HLEN,
                                   ehdr->h_source);
                }
            }
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
