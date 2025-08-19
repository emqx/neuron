/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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
#include <memory.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "neuron.h"

#include "modbus_s.h"

zlog_category_t *neuron           = NULL;
neu_events_t    *events           = NULL;
neu_event_io_t  *tcp_server_event = NULL;
neu_conn_t      *conn             = NULL;
int64_t          global_timestamp = 0;
bool             exiting          = false;

static void start_listen(void *data, int fd);
static void stop_listen(void *data, int fd);
static void connected(void *data, int fd);
static void disconnected(void *data, int fd);
static int  new_client(enum neu_event_io_type type, int fd, void *usr_data);
static int  recv_msg(enum neu_event_io_type type, int fd, void *usr_data);
static bool mode_tcp = true;

struct client_event {
    neu_event_io_t *client;
    int             fd;
    void           *user_data;
};

struct client_event c_events[10] = { 0 };

struct cycle_buf {
    uint16_t len;
    uint8_t  buf[4096];
};

static int add_client(int fd, neu_event_io_t *client, void *user_data)
{
    for (uint32_t i = 0; i < sizeof(c_events) / sizeof(struct client_event);
         i++) {
        if (c_events[i].fd == 0) {
            c_events[i].client    = client;
            c_events[i].fd        = fd;
            c_events[i].user_data = user_data;
            return 0;
        }
    }

    return -1;
}

static neu_event_io_t *del_client(int fd)
{
    for (uint32_t i = 0; i < sizeof(c_events) / sizeof(struct client_event);
         i++) {
        if (c_events[i].fd == fd) {
            c_events[i].fd = 0;
            return c_events[i].client;
        }
    }

    return NULL;
}

static void sig_handler(int sig)
{
    exiting = true;
    (void) sig;
    for (uint32_t i = 0; i < sizeof(c_events) / sizeof(struct client_event);
         i++) {
        if (c_events[i].fd > 0) {
            neu_event_del_io(events, c_events[i].client);
        }
    }

    nlog_warn("recv sig: %d\n", sig);
    neu_conn_destory(conn);
    neu_event_close(events);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        printf("./modbus_simulator rtu/tcp port ip_v4/ip_v6\n");
        return -1;
    }

    if (strncmp(argv[1], "tcp", strlen("tcp")) == 0) {
        mode_tcp = true;
    } else if (strncmp(argv[1], "rtu", strlen("rtu")) == 0) {
        mode_tcp = false;
    } else {
        printf("./modbus_simulator rtu/tcp port\n");
        return -1;
    }

    int port = atoi(argv[2]);
    if (port <= 1024) {
        printf("port <= 1024\n");
        return -1;
    }

    bool mode_ipv6 = false;
    if (strncmp(argv[3], "ip_v6", strlen("ip_v6")) == 0) {
        mode_ipv6 = true;
    } else if (strncmp(argv[3], "ip_v4", strlen("ip_v4")) == 0) {
        mode_ipv6 = false;
    } else {
        printf("./modbus_simulator rtu/tcp port ip_v4/ip_v6\n");
        return -1;
    }

    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");

    neu_conn_param_t param = {
        .log                            = neuron,
        .type                           = NEU_CONN_TCP_SERVER,
        .params.tcp_server.ip           = mode_ipv6 ? "::" : "0.0.0.0",
        .params.tcp_server.port         = (uint16_t) port,
        .params.tcp_server.timeout      = 0,
        .params.tcp_server.max_link     = 10,
        .params.tcp_server.start_listen = start_listen,
        .params.tcp_server.stop_listen  = stop_listen,
    };

    events = neu_event_new("modbus_simulator");
    conn   = neu_conn_new(&param, NULL, connected, disconnected);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGABRT, sig_handler);

    while (1) {
        sleep(10);
    }

    return 0;
}

static void start_listen(void *data, int fd)
{
    neu_event_io_param_t io_event = {
        .fd       = fd,
        .usr_data = data,
        .cb       = new_client,
    };

    tcp_server_event = neu_event_add_io(events, io_event);
    nlog_info("start listen....\n");
}

static void stop_listen(void *data, int fd)
{
    (void) data;
    (void) fd;

    neu_event_del_io(events, tcp_server_event);
    nlog_info("stop listen....fd: %d\n", fd);
}

static void connected(void *data, int fd)
{
    (void) data;
    (void) fd;
    return;
}

static void disconnected(void *data, int fd)
{
    (void) data;
    (void) fd;

    return;
}

static int new_client(enum neu_event_io_type type, int fd, void *usr_data)
{
    switch (type) {
    case NEU_EVENT_IO_READ: {
        int client_fd = neu_conn_tcp_server_accept(conn);
        if (client_fd > 0) {
            struct cycle_buf    *buf = calloc(1, sizeof(struct cycle_buf));
            neu_event_io_param_t io  = {
                 .fd       = client_fd,
                 .usr_data = (void *) buf,
                 .cb       = recv_msg,
            };

            neu_event_io_t *client = neu_event_add_io(events, io);

            add_client(fd, client, buf);
            nlog_info("accept new client: fd: %d\n", client_fd);
        }

        break;
    }

    case NEU_EVENT_IO_CLOSED:
    case NEU_EVENT_IO_HUP: {
        neu_event_del_io(events, tcp_server_event);

        free(usr_data);
        close(fd);
        break;
    }
    }
    return 0;
}

static int recv_msg(enum neu_event_io_type type, int fd, void *usr_data)
{
    if (exiting) {
        return 0;
    }

    switch (type) {
    case NEU_EVENT_IO_READ: {
        struct cycle_buf *buf        = (struct cycle_buf *) usr_data;
        ssize_t           len        = 0;
        uint8_t           res[20000] = { 0 };
        int               res_len    = 0;

        len = neu_conn_tcp_server_recv(conn, fd, buf->buf + buf->len,
                                       sizeof(buf->buf) - buf->len);
        if (len > 0) {
            buf->len += len;
        }
        if (mode_tcp) {
            len = modbus_s_tcp_req(buf->buf, buf->len, res, sizeof(res),
                                   &res_len);
        } else {
            len = modbus_s_rtu_req(buf->buf, buf->len, res, sizeof(res),
                                   &res_len);
        }

        neu_msleep(2);

        if (len == -1) {
            neu_event_io_t *io = del_client(fd);

            if (io != NULL) {
                neu_event_del_io(events, io);
            }

            free(usr_data);
            neu_conn_tcp_server_close_client(conn, fd);
            printf("recv msg parse fail, close client: %d\n", fd);
        } else if (len == -2) {
            if (exiting) {
                return 0;
            }
            neu_conn_tcp_server_send(conn, fd, NULL, 0);
            printf("recv modbus adrress for retry_test, continue wait msg\n");
        } else if (len > 0) {
            if (exiting) {
                return 0;
            }
            memmove(buf->buf, buf->buf + len, buf->len - len);
            buf->len -= len;

            neu_conn_tcp_server_send(conn, fd, res, res_len);
        } else {
            printf("recv part modbus msg, continue wait msg\n");
        }

        break;
    }
    case NEU_EVENT_IO_CLOSED:
    case NEU_EVENT_IO_HUP: {
        neu_event_io_t *io = del_client(fd);

        if (io != NULL) {
            neu_event_del_io(events, io);
        }

        free(usr_data);
        neu_conn_tcp_server_close_client(conn, fd);
        break;
    }
    }

    return 0;
}