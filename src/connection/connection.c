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

struct neu_conn {
    neu_conn_param_t param;
    void *           data;
    bool             is_connected;

    neu_conn_callback connected;
    neu_conn_callback disconnected;
    bool              callback_trigger;

    pthread_mutex_t mtx;

    int fd;

    struct {
        int                fd;
        struct sockaddr_in client;
        bool               is_listen;
    } tcp_server;
};

static void conn_tcp_server_listen(neu_conn_t *conn);
static void conn_tcp_server_stop(neu_conn_t *conn);

static void conn_connect(neu_conn_t *conn);
static void conn_disconnect(neu_conn_t *conn);

static void conn_free_param(neu_conn_t *conn);
static void conn_init_param(neu_conn_t *conn, neu_conn_param_t *param);

neu_conn_t *neu_conn_new(neu_conn_param_t *param, void *data,
                         neu_conn_callback connected,
                         neu_conn_callback disconnected)
{
    neu_conn_t *conn = calloc(1, sizeof(neu_conn_t));

    conn_init_param(conn, param);
    conn->is_connected     = false;
    conn->callback_trigger = false;
    conn->data             = data;
    conn->disconnected     = disconnected;
    conn->connected        = connected;

    conn_tcp_server_listen(conn);

    pthread_mutex_init(&conn->mtx, NULL);

    return conn;
}

neu_conn_t *neu_conn_reconfig(neu_conn_t *conn, neu_conn_param_t *param)
{
    pthread_mutex_lock(&conn->mtx);

    conn_free_param(conn);
    conn_disconnect(conn);
    conn_tcp_server_stop(conn);

    conn_init_param(conn, param);
    conn_tcp_server_listen(conn);

    pthread_mutex_unlock(&conn->mtx);

    return conn;
}

void neu_conn_destory(neu_conn_t *conn)
{
    pthread_mutex_lock(&conn->mtx);

    conn_free_param(conn);
    conn_disconnect(conn);
    conn_tcp_server_stop(conn);

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
    int fd = -1;

    pthread_mutex_lock(&conn->mtx);
    if (conn->param.type == NEU_CONN_TCP_SERVER) {
        if (conn->tcp_server.is_listen) {
            fd = conn->fd;
        }
    } else {
        if (conn->is_connected) {
            fd = conn->fd;
        }
    }
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

    if (conn->tcp_server.fd > 0) {
        conn->disconnected(conn->data, conn->tcp_server.fd);
        close(conn->tcp_server.fd);
    }

    conn->tcp_server.fd     = fd;
    conn->tcp_server.client = client;

    conn->is_connected = true;
    conn->connected(conn->data, conn->tcp_server.fd);
    conn->callback_trigger = true;

    log_info("%s:%d accpet new client: %s:%d", conn->param.params.tcp_server.ip,
             conn->param.params.tcp_server.port, inet_ntoa(client.sin_addr),
             ntohs(client.sin_port));

    pthread_mutex_unlock(&conn->mtx);

    return 0;
}

int neu_conn_tcp_client_get_fd(neu_conn_t *conn)
{
    int fd = -1;

    pthread_mutex_lock(&conn->mtx);
    if (conn->is_connected) {
        fd = conn->tcp_server.fd;
    }
    pthread_mutex_unlock(&conn->mtx);

    return fd;
}

ssize_t neu_conn_send(neu_conn_t *conn, uint8_t *buf, ssize_t len)
{
    ssize_t ret = 0;
    int     fd  = 0;

    pthread_mutex_lock(&conn->mtx);

    conn_tcp_server_listen(conn);

    if (conn->is_connected) {
        switch (conn->param.type) {
        case NEU_CONN_TCP_SERVER:
            fd  = conn->tcp_server.fd;
            ret = send(conn->tcp_server.fd, buf, len, MSG_NOSIGNAL);
            if (ret != len) {
                log_error("tcp server fd: %d, send buf len: %d, ret: %d, "
                          "errno: %s(%d)",
                          conn->tcp_server.fd, len, ret, strerror(errno),
                          errno);
            }
            break;
        case NEU_CONN_TCP_CLIENT:
            fd  = conn->fd;
            ret = send(conn->fd, buf, len, MSG_NOSIGNAL);
            if (ret != len) {
                log_error("tcp client fd: %d, send buf len: %d, ret: %d, "
                          "errno: %s(%d)",
                          conn->fd, len, ret, strerror(errno), errno);
            }
            break;
        case NEU_CONN_UDP:
        case NEU_CONN_TTY_CLIENT:
            break;
        }

        if (ret == -1) {
            conn_disconnect(conn);
            if (conn->callback_trigger == true) {
                conn->disconnected(conn->data, fd);
                conn->callback_trigger = false;
            }
        }

        if (ret == len && conn->callback_trigger == false) {
            conn->connected(conn->data, fd);
            conn->callback_trigger = true;
        }

    } else {
        conn_connect(conn);
    }

    pthread_mutex_unlock(&conn->mtx);

    return ret;
}

ssize_t neu_conn_recv(neu_conn_t *conn, uint8_t *buf, ssize_t len)
{
    ssize_t ret = 0;
    int     fd  = 0;

    pthread_mutex_lock(&conn->mtx);

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        fd  = conn->tcp_server.fd;
        ret = recv(conn->tcp_server.fd, buf, len, MSG_WAITALL);
        if (ret != len) {
            log_error(
                "tcp server fd: %d, recv buf len %d, ret: %d, errno: %s(%d)",
                conn->tcp_server.fd, len, ret, strerror(errno), errno);
        }
        break;
    case NEU_CONN_TCP_CLIENT:
        fd  = conn->fd;
        ret = recv(conn->fd, buf, len, MSG_WAITALL);
        if (ret != len) {
            log_error(
                "tcp client fd: %d, recv buf len %d, ret: %d, errno: %s(%d)",
                conn->fd, len, ret, strerror(errno), errno);
        }
        break;
    case NEU_CONN_UDP:
    case NEU_CONN_TTY_CLIENT:
        break;
    }

    if (errno == EPIPE || ret <= 0) {
        conn_disconnect(conn);
        if (conn->callback_trigger == true) {
            conn->disconnected(conn->data, fd);
            conn->callback_trigger = false;
        }
    }

    if (ret == len && conn->callback_trigger == false) {
        conn->connected(conn->data, fd);
        conn->callback_trigger = true;
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
            ret = recv(conn->tcp_server.fd, &buf, sizeof(buf), 0);
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
    conn->param.type = param->type;

    switch (param->type) {
    case NEU_CONN_TCP_SERVER:
        conn->param.params.tcp_server.ip = strdup(param->params.tcp_server.ip);
        conn->param.params.tcp_server.port = param->params.tcp_server.port;
        conn->param.params.tcp_server.timeout =
            param->params.tcp_server.timeout;
        conn->param.params.tcp_server.max_link =
            param->params.tcp_server.max_link;
        conn->param.params.tcp_server.start_listen =
            param->params.tcp_server.start_listen;
        conn->param.params.tcp_server.stop_listen =
            param->params.tcp_server.stop_listen;
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

static void conn_tcp_server_listen(neu_conn_t *conn)
{
    if (conn->param.type == NEU_CONN_TCP_SERVER &&
        conn->tcp_server.is_listen == false) {
        int fd  = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
        int ret = 0;
        struct sockaddr_in local = {
            .sin_family      = AF_INET,
            .sin_port        = htons(conn->param.params.tcp_server.port),
            .sin_addr.s_addr = inet_addr(conn->param.params.tcp_server.ip),
        };

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

        conn->fd                   = fd;
        conn->tcp_server.is_listen = true;

        conn->param.params.tcp_server.start_listen(conn->data, fd);
        log_info("tcp server listen %s:%d success, fd: %d",
                 conn->param.params.tcp_server.ip,
                 conn->param.params.tcp_server.port, fd);
    }
}

static void conn_tcp_server_stop(neu_conn_t *conn)
{
    if (conn->param.type == NEU_CONN_TCP_SERVER &&
        conn->tcp_server.is_listen == true) {
        log_info("tcp server close %s:%d, fd: %d",
                 conn->param.params.tcp_server.ip,
                 conn->param.params.tcp_server.port, conn->fd);

        conn->param.params.tcp_server.start_listen(conn->data, conn->fd);

        close(conn->tcp_server.fd);
        close(conn->fd);

        conn->tcp_server.is_listen = false;
    }
}

static void conn_connect(neu_conn_t *conn)
{
    int ret = 0;

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        break;
    case NEU_CONN_TCP_CLIENT: {
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
        struct sockaddr_in remote = {
            .sin_family      = AF_INET,
            .sin_port        = htons(conn->param.params.tcp_client.port),
            .sin_addr.s_addr = inet_addr(conn->param.params.tcp_client.ip),
        };

        ret = connect(fd, (struct sockaddr *) &remote,
                      sizeof(struct sockaddr_in));
        if (ret != 0 && errno != EINPROGRESS) {
            close(fd);
            log_error(
                "connect %s:%d error: %s(%d)", conn->param.params.tcp_client.ip,
                conn->param.params.tcp_client.port, strerror(errno), errno);
            conn->is_connected = false;
            return;
        } else {
            log_info("connect %s:%d success", conn->param.params.tcp_client.ip,
                     conn->param.params.tcp_client.port);
            conn->is_connected = true;
            conn->fd           = fd;
        }
        break;
    }
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
        close(conn->tcp_server.fd);
        break;
    case NEU_CONN_TCP_CLIENT:
        close(conn->fd);
        break;
    case NEU_CONN_UDP:
        break;
    case NEU_CONN_TTY_CLIENT:
        break;
    }

    conn->is_connected = false;
}