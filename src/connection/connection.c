/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
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

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "connection/neu_connection.h"
#include "neu_log.h"

struct neu_conn_tcp_client {
    int                fd;
    struct sockaddr_in client;
};

struct neu_conn {
    neu_conn_param_t      param;
    void *                data;
    bool                  flag_connection;
    neu_conn_conncted     connected;
    neu_conn_disconnected disconnected;

    int fd;
};

static int  neu_conn_connect(neu_conn_t *conn);
static void neu_conn_free_param(neu_conn_t *conn);

neu_conn_t *neu_conn_new(neu_conn_param_t *param, void *data,
                         neu_conn_conncted     connected,
                         neu_conn_disconnected disconnected)
{
    neu_conn_t *conn = calloc(1, sizeof(neu_conn_t));

    switch (param->type) {
    case NEU_CONN_TCP_SERVER:
        conn->param.params.tcp_server.ip = strdup(param->params.tcp_server.ip);
        conn->param.params.tcp_server.port = param->params.tcp_server.port;
        conn->param.params.tcp_server.timeout =
            param->params.tcp_server.timeout;
        conn->param.params.tcp_server.max_link =
            param->params.tcp_server.max_link;
        break;
    case NEU_CONN_TCP_CLIENT:
        conn->param.params.tcp_client.ip = strdup(param->params.tcp_client.ip);
        conn->param.params.tcp_client.port = param->params.tcp_client.port;
        conn->param.params.tcp_client.timeout =
            param->params.tcp_client.timeout;
        break;
    case NEU_CONN_UDP:
        conn->param.params.udp.ip   = strdup(param->params.udp.ip);
        conn->param.params.udp.port = param->params.udp.port;
        break;
    case NEU_CONN_TTY_CLIENT:
        conn->param.params.tty_client.device =
            strdup(param->params.tty_client.device);
        conn->param.params.tty_client.data   = param->params.tty_client.data;
        conn->param.params.tty_client.stop   = param->params.tty_client.stop;
        conn->param.params.tty_client.baud   = param->params.tty_client.baud;
        conn->param.params.tty_client.parity = param->params.tty_client.parity;
        break;
    }

    conn->data         = data;
    conn->disconnected = disconnected;
    conn->connected    = connected;

    if (neu_conn_connect(conn) != 0) {
        neu_conn_free_param(conn);
        free(conn);
        return NULL;
    }

    return conn;
}

void neu_conn_destory(neu_conn_t *conn)
{
    close(conn->fd);
    neu_conn_free_param(conn);

    free(conn);
    return;
}

neu_conn_type_e neu_conn_get_type(neu_conn_t *conn)
{
    return conn->param.type;
}

int neu_conn_get_fd(neu_conn_t *conn)
{
    return conn->fd;
}

neu_conn_tcp_client_t *neu_conn_tcp_accept(neu_conn_t *conn)
{
    if (conn->param.type == NEU_CONN_TCP_SERVER) {
        struct sockaddr_in     client     = { 0 };
        socklen_t              client_len = 0;
        neu_conn_tcp_client_t *tcp_client = NULL;

        int fd = accept(conn->fd, (struct sockaddr *) &client, &client_len);
        if (fd <= 0) {
            log_error("%s:%d accpet error: %s",
                      conn->param.params.tcp_server.ip,
                      conn->param.params.tcp_server.port, strerror(errno));
            return NULL;
        }

        tcp_client         = calloc(1, sizeof(neu_conn_tcp_client_t));
        tcp_client->fd     = fd;
        tcp_client->client = client;

        log_info("%s:%d accpet new client: %s:%d",
                 conn->param.params.tcp_server.ip,
                 conn->param.params.tcp_server.port, inet_ntoa(client.sin_addr),
                 ntohs(client.sin_port));
        return tcp_client;
    } else {
        return NULL;
    }
}

int neu_conn_tcp_client_get_fd(neu_conn_tcp_client_t *client)
{
    return client->fd;
}

ssize_t neu_conn_send(neu_conn_t *conn, neu_conn_tcp_client_t *client,
                      uint8_t *buf, ssize_t len)
{
    ssize_t ret = 0;

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        assert(client != NULL);

        ret = send(client->fd, buf, len, MSG_NOSIGNAL);
        if (ret != len) {
            log_error("tcp server fd: %d, send buf len: %d, ret: %d, errno: %s",
                      client->fd, len, ret, strerror(errno));
            return -1;
        }
        break;
    case NEU_CONN_TCP_CLIENT:
        ret = send(conn->fd, buf, len, MSG_NOSIGNAL);
        if (ret != len) {
            log_error("tcp client fd: %d, send buf len: %d, ret: %d, errno: %s",
                      conn->fd, len, ret, strerror(errno));
            return -1;
        }
        break;
    case NEU_CONN_UDP:
    case NEU_CONN_TTY_CLIENT:
        break;
    }

    return ret;
}

ssize_t neu_conn_recv(neu_conn_t *conn, neu_conn_tcp_client_t *client,
                      uint8_t *buf, ssize_t len)
{
    ssize_t ret = 0;

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        assert(client != NULL);

        ret = recv(client->fd, buf, len, MSG_WAITALL);
        if (ret != len) {
            log_error("tcp server fd: %d, recv buf len %d, ret: %d, errno: %s",
                      client->fd, len, ret, strerror(errno));
            return -1;
        }
        break;
    case NEU_CONN_TCP_CLIENT:
        ret = recv(conn->fd, buf, len, MSG_WAITALL);
        if (ret != len) {
            log_error("tcp client fd: %d, recv buf len %d, ret: %d, errno: %s",
                      client->fd, len, ret, strerror(errno));
            return -1;
        }
        break;
    case NEU_CONN_UDP:
    case NEU_CONN_TTY_CLIENT:
        break;
    }

    return 0;
}

void neu_conn_tcp_flush(neu_conn_t *conn, neu_conn_tcp_client_t *client)
{
    ssize_t ret = 1;
    char    buf = 0;

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        assert(client != NULL);

        while (ret == 1) {
            ret = recv(client->fd, &buf, 1, 0);
        }

        break;
    case NEU_CONN_TCP_CLIENT:
        while (ret == 1) {
            ret = recv(client->fd, &buf, 1, 0);
        }

        break;
    case NEU_CONN_UDP:
    case NEU_CONN_TTY_CLIENT:
        break;
    }
    return;
}

static int neu_conn_connect(neu_conn_t *conn)
{
    int                ret    = 0;
    struct sockaddr_in local  = { 0 };
    struct sockaddr_in remote = { 0 };

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        conn->fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

        local.sin_family      = AF_INET;
        local.sin_port        = htons(conn->param.params.tcp_server.port);
        local.sin_addr.s_addr = inet_addr(conn->param.params.tcp_server.ip);

        ret = bind(conn->fd, (struct sockaddr *) &local, sizeof(local));
        if (ret != 0) {
            close(conn->fd);
            log_error("tcp bind %s:%d fail, errno: %s",
                      conn->param.params.tcp_server.ip,
                      conn->param.params.tcp_server.port, strerror(errno));
            return -1;
        }

        ret = listen(conn->fd, 1);
        if (ret != 0) {
            close(conn->fd);
            log_error("tcp bind %s:%d fail, errno: %s",
                      conn->param.params.tcp_server.ip,
                      conn->param.params.tcp_server.port, strerror(errno));
            return -1;
        }

        break;
    case NEU_CONN_TCP_CLIENT:
        conn->fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

        remote.sin_family      = AF_INET;
        remote.sin_port        = htons(conn->param.params.tcp_client.port);
        remote.sin_addr.s_addr = inet_addr(conn->param.params.tcp_client.ip);

        ret = connect(conn->fd, (struct sockaddr *) &remote,
                      sizeof(struct sockaddr_in));
        if (ret != 0) {
            close(conn->fd);
            log_error("connect %s:%d error: %s",
                      conn->param.params.tcp_client.ip,
                      conn->param.params.tcp_client.port, strerror(errno));
            return -1;
        }
        break;
    case NEU_CONN_UDP:
        break;
    case NEU_CONN_TTY_CLIENT:
        break;
    }

    return 0;
}

static void neu_conn_free_param(neu_conn_t *conn)
{
    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        free(conn->param.params.tcp_server.ip);
        break;
    case NEU_CONN_TCP_CLIENT:
        free(conn->param.params.tcp_client.ip);
        break;
    case NEU_CONN_UDP:
        free(conn->param.params.udp.ip);
        break;
    case NEU_CONN_TTY_CLIENT:
        free(conn->param.params.tty_client.device);
        break;
    }
}