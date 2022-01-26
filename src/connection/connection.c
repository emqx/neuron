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
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "connection/neu_connection.h"
#include "neu_log.h"

struct tcp_client {
};

struct neu_conn {
    neu_conn_param_t      param;
    void *                data;
    bool                  is_init;
    neu_conn_conncted     connected;
    neu_conn_disconnected disconnected;

    pthread_mutex_t mtx;

    int fd;

    union {
        struct {
            bool is_connected;
        } tcp_client;
        struct {
            int                fd;
            struct sockaddr_in client;
            bool               is_connected;
        } tcp_server;
        struct {
            bool is_connected;
        } udp;
        struct {
            bool is_connected;
        } tty;
    } state;
};

static bool conn_is_connected(neu_conn_t *conn);

static void conn_init_conn(neu_conn_t *conn);
static void conn_uninit_conn(neu_conn_t *conn);

static void conn_connect(neu_conn_t *conn);
static void conn_disconnect(neu_conn_t *conn);

static void conn_free_param(neu_conn_t *conn);
static void conn_init_param(neu_conn_t *conn, neu_conn_param_t *param);

neu_conn_t *neu_conn_new(neu_conn_param_t *param, void *data,
                         neu_conn_conncted     connected,
                         neu_conn_disconnected disconnected)
{
    neu_conn_t *conn = calloc(1, sizeof(neu_conn_t));

    conn_init_param(conn, param);

    conn->is_init      = false;
    conn->data         = data;
    conn->disconnected = disconnected;
    conn->connected    = connected;

    conn_init_conn(conn);

    pthread_mutex_init(&conn->mtx, NULL);

    return conn;
}

neu_conn_t *neu_conn_reconfig(neu_conn_t *conn, neu_conn_param_t *param)
{
    pthread_mutex_lock(&conn->mtx);

    conn_free_param(conn);
    conn_disconnect(conn);
    conn_uninit_conn(conn);

    conn_init_param(conn, param);
    conn_init_conn(conn);

    pthread_mutex_unlock(&conn->mtx);

    return conn;
}

void neu_conn_destory(neu_conn_t *conn)
{
    pthread_mutex_lock(&conn->mtx);

    conn_free_param(conn);
    conn_disconnect(conn);
    conn_uninit_conn(conn);

    pthread_mutex_unlock(&conn->mtx);

    pthread_mutex_destroy(&conn->mtx);

    free(conn);
}

neu_conn_type_e neu_conn_get_type(neu_conn_t *conn)
{
    neu_conn_type_e type = { 0 };
    pthread_mutex_lock(&conn->mtx);
    type = conn->param.type;
    pthread_mutex_unlock(&conn->mtx);

    return type;
}

int neu_conn_get_fd(neu_conn_t *conn)
{
    int fd = 0;

    pthread_mutex_lock(&conn->mtx);
    fd = conn->fd;
    pthread_mutex_unlock(&conn->mtx);

    return fd;
}

int neu_conn_tcp_accept(neu_conn_t *conn)
{
    struct sockaddr_in client     = { 0 };
    socklen_t          client_len = 0;
    int                fd         = 0;

    pthread_mutex_lock(&conn->mtx);
    if (conn->param.type != NEU_CONN_TCP_SERVER) {
        pthread_mutex_unlock(&conn->mtx);
        return -1;
    }

    fd = accept(conn->fd, (struct sockaddr *) &client, &client_len);
    if (fd <= 0) {
        log_error("%s:%d accpet error: %s", conn->param.params.tcp_server.ip,
                  conn->param.params.tcp_server.port, strerror(errno));
        pthread_mutex_unlock(&conn->mtx);
        return -1;
    }

    if (conn->state.tcp_server.fd > 0) {
        close(conn->state.tcp_server.fd);
    }

    conn->state.tcp_server.fd           = fd;
    conn->state.tcp_server.client       = client;
    conn->state.tcp_server.is_connected = true;

    conn->connected(conn->data);

    log_info("%s:%d accpet new client: %s:%d", conn->param.params.tcp_server.ip,
             conn->param.params.tcp_server.port, inet_ntoa(client.sin_addr),
             ntohs(client.sin_port));

    pthread_mutex_unlock(&conn->mtx);

    return 0;
}

int neu_conn_tcp_client_get_fd(neu_conn_t *conn)
{
    int fd = 0;

    pthread_mutex_lock(&conn->mtx);
    fd = conn->state.tcp_server.fd;
    pthread_mutex_unlock(&conn->mtx);

    return fd;
}

ssize_t neu_conn_send(neu_conn_t *conn, uint8_t *buf, ssize_t len)
{
    ssize_t ret = 0;

    pthread_mutex_lock(&conn->mtx);
    if (!conn->is_init) {
        conn_init_conn(conn);
    }

    if (conn->is_init && !conn_is_connected(conn)) {
        conn_connect(conn);
    }

    if (conn_is_connected(conn)) {
        switch (conn->param.type) {
        case NEU_CONN_TCP_SERVER:
            ret = send(conn->state.tcp_server.fd, buf, len, MSG_NOSIGNAL);
            if (ret != len) {
                log_error(
                    "tcp server fd: %d, send buf len: %d, ret: %d, errno: %s",
                    conn->state.tcp_server.fd, len, ret, strerror(errno));
            }
            break;
        case NEU_CONN_TCP_CLIENT:
            ret = send(conn->fd, buf, len, MSG_NOSIGNAL);
            if (ret != len) {
                log_error(
                    "tcp client fd: %d, send buf len: %d, ret: %d, errno: %s",
                    conn->fd, len, ret, strerror(errno));
            }
            break;
        case NEU_CONN_UDP:
        case NEU_CONN_TTY_CLIENT:
            break;
        }

        if (ret == -1 && errno == 12) {
            conn_disconnect(conn);
        }
    }

    pthread_mutex_unlock(&conn->mtx);

    return ret;
}

ssize_t neu_conn_recv(neu_conn_t *conn, uint8_t *buf, ssize_t len)
{
    ssize_t ret = 0;

    pthread_mutex_lock(&conn->mtx);

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        ret = recv(conn->state.tcp_server.fd, buf, len, MSG_WAITALL);
        if (ret != len) {
            log_error("tcp server fd: %d, recv buf len %d, ret: %d, errno: %s",
                      conn->state.tcp_server.fd, len, ret, strerror(errno));
        }
        break;
    case NEU_CONN_TCP_CLIENT:
        ret = recv(conn->fd, buf, len, MSG_WAITALL);
        if (ret != len) {
            log_error("tcp client fd: %d, recv buf len %d, ret: %d, errno: %s",
                      conn->fd, len, ret, strerror(errno));
        }
        break;
    case NEU_CONN_UDP:
    case NEU_CONN_TTY_CLIENT:
        break;
    }

    pthread_mutex_unlock(&conn->mtx);

    return ret;
}

void neu_conn_tcp_flush(neu_conn_t *conn)
{
    ssize_t ret     = 1;
    char    buf[64] = { 0 };

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        while (ret > 0) {
            pthread_mutex_lock(&conn->mtx);
            ret = recv(conn->state.tcp_server.fd, &buf, sizeof(buf), 0);
            pthread_mutex_unlock(&conn->mtx);
        }

        break;
    case NEU_CONN_TCP_CLIENT:
        while (ret > 0) {
            pthread_mutex_lock(&conn->mtx);
            ret = recv(conn->fd, &buf, sizeof(buf), 0);
            pthread_mutex_unlock(&conn->mtx);
        }
        break;
    case NEU_CONN_UDP:
    case NEU_CONN_TTY_CLIENT:
        break;
    }
}

static void conn_free_param(neu_conn_t *conn)
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

static void conn_init_param(neu_conn_t *conn, neu_conn_param_t *param)
{
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
}

static void conn_init_conn(neu_conn_t *conn)
{
    int                ret   = 0;
    struct sockaddr_in local = { 0 };
    int                fd    = 0;

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

        local.sin_family      = AF_INET;
        local.sin_port        = htons(conn->param.params.tcp_server.port);
        local.sin_addr.s_addr = inet_addr(conn->param.params.tcp_server.ip);

        ret = bind(fd, (struct sockaddr *) &local, sizeof(local));
        if (ret != 0) {
            close(fd);
            log_error("tcp bind %s:%d fail, errno: %s",
                      conn->param.params.tcp_server.ip,
                      conn->param.params.tcp_server.port, strerror(errno));
            return;
        }

        ret = listen(fd, 1);
        if (ret != 0) {
            close(fd);
            log_error("tcp bind %s:%d fail, errno: %s",
                      conn->param.params.tcp_server.ip,
                      conn->param.params.tcp_server.port, strerror(errno));
            return;
        }

        break;
    case NEU_CONN_TCP_CLIENT:
        fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
        break;
    case NEU_CONN_UDP:
        break;
    case NEU_CONN_TTY_CLIENT:
        break;
    }

    conn->fd      = fd;
    conn->is_init = true;
}

static void conn_uninit_conn(neu_conn_t *conn)
{
    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
    case NEU_CONN_TCP_CLIENT:
        close(conn->fd);
        break;
    case NEU_CONN_UDP:
        break;
    case NEU_CONN_TTY_CLIENT:
        break;
    }

    conn->is_init = false;
}

static void conn_connect(neu_conn_t *conn)
{
    int                ret    = 0;
    struct sockaddr_in remote = { 0 };

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        break;
    case NEU_CONN_TCP_CLIENT:
        remote.sin_family      = AF_INET;
        remote.sin_port        = htons(conn->param.params.tcp_client.port);
        remote.sin_addr.s_addr = inet_addr(conn->param.params.tcp_client.ip);

        ret = connect(conn->fd, (struct sockaddr *) &remote,
                      sizeof(struct sockaddr_in));
        if (ret != 0) {
            log_error("connect %s:%d error: %s",
                      conn->param.params.tcp_client.ip,
                      conn->param.params.tcp_client.port, strerror(errno));
            conn->state.tcp_client.is_connected = false;
        } else {
            conn->state.tcp_client.is_connected = true;
            conn->connected(conn->data);
        }

        break;
    case NEU_CONN_UDP:
        break;
    case NEU_CONN_TTY_CLIENT:
        break;
    }
}

static void conn_disconnect(neu_conn_t *conn)
{
    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        close(conn->state.tcp_server.fd);
        conn->state.tcp_server.is_connected = false;
        break;
    case NEU_CONN_TCP_CLIENT:
        conn->state.tcp_client.is_connected = false;
        break;
    case NEU_CONN_UDP:
        conn->state.udp.is_connected = false;
        break;
    case NEU_CONN_TTY_CLIENT:
        conn->state.tty.is_connected = false;
        break;
    }

    conn->disconnected(conn->data);
}

static bool conn_is_connected(neu_conn_t *conn)
{
    bool is_connected = false;

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        is_connected = conn->state.tcp_server.is_connected;
        break;
    case NEU_CONN_TCP_CLIENT:
        is_connected = conn->state.tcp_client.is_connected;
        break;
    case NEU_CONN_UDP:
        is_connected = conn->state.udp.is_connected;
        break;
    case NEU_CONN_TTY_CLIENT:
        is_connected = conn->state.tty.is_connected;
        break;
    }

    return is_connected;
}