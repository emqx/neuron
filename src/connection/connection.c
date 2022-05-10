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
#include <sys/time.h>

#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#include "connection/neu_connection.h"
#include "log.h"

struct tcp_client {
    int                fd;
    struct sockaddr_in client;
};

struct neu_conn {
    neu_conn_param_t param;
    void *           data;
    bool             is_connected;

    neu_conn_callback connected;
    neu_conn_callback disconnected;
    bool              callback_trigger;

    pthread_mutex_t mtx;

    int  fd;
    bool block;

    struct {
        struct tcp_client *clients;
        int                n_client;
        bool               is_listen;
    } tcp_server;
};

static void conn_tcp_server_add_client(neu_conn_t *conn, int fd,
                                       struct sockaddr_in client);
static void conn_tcp_server_del_client(neu_conn_t *conn, int fd);

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

    conn_disconnect(conn);
    conn_free_param(conn);
    conn_tcp_server_stop(conn);

    conn_init_param(conn, param);
    conn_tcp_server_listen(conn);

    pthread_mutex_unlock(&conn->mtx);

    return conn;
}

void neu_conn_destory(neu_conn_t *conn)
{
    pthread_mutex_lock(&conn->mtx);

    conn_tcp_server_stop(conn);
    conn_disconnect(conn);
    conn_free_param(conn);

    pthread_mutex_unlock(&conn->mtx);

    pthread_mutex_destroy(&conn->mtx);

    free(conn);
}

int neu_conn_tcp_server_accept(neu_conn_t *conn)
{
    struct sockaddr_in client     = { 0 };
    socklen_t          client_len = sizeof(struct sockaddr_in);
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

    if (conn->tcp_server.n_client >= conn->param.params.tcp_server.max_link) {
        close(fd);
        log_warn("%s:%d accpet max link: %d, reject",
                 conn->param.params.tcp_server.ip,
                 conn->param.params.tcp_server.port,
                 conn->param.params.tcp_server.max_link);
        pthread_mutex_unlock(&conn->mtx);
        return -1;
    }

    if (conn->block) {
        struct timeval tv = {
            .tv_sec  = conn->param.params.tcp_server.timeout / 1000,
            .tv_usec = (conn->param.params.tcp_server.timeout % 1000) * 1000,
        };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }

    conn_tcp_server_add_client(conn, fd, client);

    conn->is_connected = true;
    conn->connected(conn->data, fd);
    conn->callback_trigger = true;

    log_info("%s:%d accpet new client: %s:%d, fd: %d",
             conn->param.params.tcp_server.ip,
             conn->param.params.tcp_server.port, inet_ntoa(client.sin_addr),
             ntohs(client.sin_port), fd);

    pthread_mutex_unlock(&conn->mtx);

    return fd;
}

int neu_conn_tcp_server_close_client(neu_conn_t *conn, int fd)
{
    pthread_mutex_lock(&conn->mtx);
    if (conn->param.type != NEU_CONN_TCP_SERVER) {
        pthread_mutex_unlock(&conn->mtx);
        return -1;
    }

    conn->disconnected(conn->data, fd);
    conn_tcp_server_del_client(conn, fd);

    pthread_mutex_unlock(&conn->mtx);
    return 0;
}

ssize_t neu_conn_tcp_server_send(neu_conn_t *conn, int fd, uint8_t *buf,
                                 ssize_t len)
{
    conn_tcp_server_listen(conn);
    ssize_t ret = send(fd, buf, len, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (ret != len) {
        log_error("tcp server fd: %d, send buf len: %d, ret: %d, "
                  "errno: %s(%d)",
                  fd, len, ret, strerror(errno), errno);
        return -1;
    }

    return ret;
}

ssize_t neu_conn_tcp_server_recv(neu_conn_t *conn, int fd, uint8_t *buf,
                                 ssize_t len)
{
    ssize_t ret = 0;

    if (conn->block) {
        ret = recv(fd, buf, len, MSG_WAITALL);
    } else {
        ret = recv(fd, buf, len, 0);
    }
    if (ret == -1) {
        log_error("tcp server fd: %d, recv buf len %d, ret: %d, errno: %s(%d)",
                  fd, len, ret, strerror(errno), errno);
        return -1;
    }

    return ret;
}

ssize_t neu_conn_send(neu_conn_t *conn, uint8_t *buf, ssize_t len)
{
    ssize_t ret = 0;

    pthread_mutex_lock(&conn->mtx);

    if (conn->is_connected) {
        switch (conn->param.type) {
        case NEU_CONN_TCP_SERVER:
            log_fatal("neu_conn_send cann't send tcp server msg");
            assert(1 == 0);
            break;
        case NEU_CONN_TCP_CLIENT:
        case NEU_CONN_UDP:
            ret = send(conn->fd, buf, len, MSG_NOSIGNAL | MSG_DONTWAIT);
            if (ret != len) {
                log_error("conn fd: %d, send buf len: %d, ret: %d, "
                          "errno: %s(%d)",
                          conn->fd, len, ret, strerror(errno), errno);
            }
            break;
        case NEU_CONN_TTY_CLIENT:
            break;
        }

        if (ret == -1 && errno != EAGAIN) {
            conn_disconnect(conn);
            if (conn->callback_trigger == true) {
                conn->disconnected(conn->data, conn->fd);
                conn->callback_trigger = false;
            }
        }

        if (ret > 0 && conn->callback_trigger == false) {
            conn->connected(conn->data, conn->fd);
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

    pthread_mutex_lock(&conn->mtx);

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        log_fatal("neu_conn_recv cann't recv tcp server msg");
        assert(1 == 0);
        break;
    case NEU_CONN_TCP_CLIENT:
        if (conn->block) {
            ret = recv(conn->fd, buf, len, MSG_WAITALL);
        } else {
            ret = recv(conn->fd, buf, len, 0);
        }
        if (ret == -1) {
            log_error(
                "tcp client fd: %d, recv buf len %d, ret: %d, errno: %s(%d)",
                conn->fd, len, ret, strerror(errno), errno);
        }
        break;
    case NEU_CONN_UDP:
        ret = recv(conn->fd, buf, len, 0);
        if (ret == -1) {
            log_error("udp fd: %d, recv buf len %d, ret: %d, errno: %s(%d)",
                      conn->fd, len, ret, strerror(errno), errno);
        }
        break;
    case NEU_CONN_TTY_CLIENT:
        break;
    }

    if (errno == EPIPE || ret <= 0) {
        conn_disconnect(conn);
        if (conn->callback_trigger == true) {
            conn->disconnected(conn->data, conn->fd);
            conn->callback_trigger = false;
        }
    }

    if (ret > 0 && conn->callback_trigger == false) {
        conn->connected(conn->data, conn->fd);
        conn->callback_trigger = true;
    }

    pthread_mutex_unlock(&conn->mtx);

    return ret;
}

void neu_conn_tcp_server_flush(neu_conn_t *conn, int fd)
{
    (void) conn;
    ssize_t ret     = 1;
    char    buf[64] = { 0 };

    while (ret > 0) {
        ret = recv(fd, &buf, sizeof(buf), 0);
    }
}

void neu_conn_flush(neu_conn_t *conn)
{
    ssize_t ret     = 1;
    char    buf[64] = { 0 };

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        log_fatal("neu_conn_flush cann't flush tcp server msg");
        assert(1 == 0);
        break;
    case NEU_CONN_TCP_CLIENT:
    case NEU_CONN_UDP:
        while (ret > 0) {
            pthread_mutex_lock(&conn->mtx);
            ret = recv(conn->fd, buf, sizeof(buf), 0);
            pthread_mutex_unlock(&conn->mtx);
        }
        break;
    case NEU_CONN_TTY_CLIENT:
        break;
    }
}

static void conn_free_param(neu_conn_t *conn)
{
    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        free(conn->param.params.tcp_server.ip);
        free(conn->tcp_server.clients);
        conn->tcp_server.n_client = 0;
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
        conn->tcp_server.clients = calloc(
            conn->param.params.tcp_server.max_link, sizeof(struct tcp_client));
        if (conn->param.params.tcp_server.timeout > 0) {
            conn->block = true;
        } else {
            conn->block = false;
        }
        break;
    case NEU_CONN_TCP_CLIENT:
        conn->param.params.tcp_client.ip = strdup(param->params.tcp_client.ip);
        conn->param.params.tcp_client.port = param->params.tcp_client.port;
        conn->param.params.tcp_client.timeout =
            param->params.tcp_client.timeout;
        conn->block = conn->param.params.tcp_client.timeout > 0;
        break;
    case NEU_CONN_UDP:
        conn->param.params.udp.ip   = strdup(param->params.udp.ip);
        conn->param.params.udp.port = param->params.udp.port;
        conn->block                 = conn->param.params.udp.timeout > 0;
        break;
    case NEU_CONN_TTY_CLIENT:
        conn->param.params.tty_client.device =
            strdup(param->params.tty_client.device);
        conn->param.params.tty_client.data   = param->params.tty_client.data;
        conn->param.params.tty_client.stop   = param->params.tty_client.stop;
        conn->param.params.tty_client.baud   = param->params.tty_client.baud;
        conn->param.params.tty_client.parity = param->params.tty_client.parity;
        conn->block                          = false;
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

        conn->param.params.tcp_server.stop_listen(conn->data, conn->fd);

        for (int i = 0; i < conn->param.params.tcp_server.max_link; i++) {
            if (conn->tcp_server.clients[i].fd > 0) {
                close(conn->tcp_server.clients[i].fd);
            }
        }

        close(conn->fd);

        conn->tcp_server.is_listen = false;
    }
}

static void conn_connect(neu_conn_t *conn)
{
    int ret = 0;
    int fd  = 0;

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        break;
    case NEU_CONN_TCP_CLIENT: {
        if (conn->block) {
            struct timeval tv = {
                .tv_sec = conn->param.params.tcp_client.timeout / 1000,
                .tv_usec =
                    (conn->param.params.tcp_client.timeout % 1000) * 1000,
            };

            fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        } else {
            fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
        }
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
    case NEU_CONN_UDP: {
        if (conn->block) {
            struct timeval tv = {
                .tv_sec  = conn->param.params.udp.timeout / 1000,
                .tv_usec = (conn->param.params.udp.timeout % 1000) * 1000,
            };

            fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        } else {
            fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
        }
        struct sockaddr_in remote = {
            .sin_family      = AF_INET,
            .sin_port        = htons(conn->param.params.udp.port),
            .sin_addr.s_addr = inet_addr(conn->param.params.udp.ip),
        };
        ret = connect(fd, (struct sockaddr *) &remote,
                      sizeof(struct sockaddr_in));
        if (ret != 0 && errno != EINPROGRESS) {
            close(fd);
            log_error("connect %s:%d error: %s(%d)", conn->param.params.udp.ip,
                      conn->param.params.udp.port, strerror(errno), errno);
            conn->is_connected = false;
            return;
        } else {
            log_info("connect %s:%d success", conn->param.params.udp.ip,
                     conn->param.params.udp.port);
            conn->is_connected = true;
            conn->fd           = fd;
        }
        break;
    }
    case NEU_CONN_TTY_CLIENT:
        break;
    }
}

static void conn_disconnect(neu_conn_t *conn)
{
    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        for (int i = 0; i < conn->param.params.tcp_server.max_link; i++) {
            if (conn->tcp_server.clients[i].fd > 0) {
                close(conn->tcp_server.clients[i].fd);
            }
        }
        break;
    case NEU_CONN_TCP_CLIENT:
    case NEU_CONN_UDP:
        close(conn->fd);
        break;
    case NEU_CONN_TTY_CLIENT:
        break;
    }

    conn->is_connected = false;
}

static void conn_tcp_server_add_client(neu_conn_t *conn, int fd,
                                       struct sockaddr_in client)
{
    for (int i = 0; i < conn->param.params.tcp_server.max_link; i++) {
        if (conn->tcp_server.clients[i].fd == 0) {
            conn->tcp_server.clients[i].fd     = fd;
            conn->tcp_server.clients[i].client = client;
            conn->tcp_server.n_client += 1;
            return;
        }
    }

    assert(1 == 0);
    return;
}

static void conn_tcp_server_del_client(neu_conn_t *conn, int fd)
{
    for (int i = 0; i < conn->param.params.tcp_server.max_link; i++) {
        if (conn->tcp_server.clients[i].fd == fd) {
            close(fd);
            conn->tcp_server.clients[i].fd = 0;
            conn->tcp_server.n_client -= 1;
            return;
        }
    }

    assert(1 == 0);
}

void neu_conn_stream_consume(neu_conn_t *conn, void *context,
                             neu_conn_stream_consume_fn fn)
{
    static __thread uint8_t  recv_buf[2048] = { 0 };
    static __thread uint16_t offset         = 0;
    ssize_t                  ret =
        neu_conn_recv(conn, recv_buf + offset, sizeof(recv_buf) - offset);
    if (ret > 0) {
        offset += ret;
        neu_protocol_unpack_buf_t protocol_buf = { 0 };
        neu_protocol_unpack_buf_init(&protocol_buf, recv_buf, offset);
        while (neu_protocol_unpack_buf_unused_size(&protocol_buf) > 0) {
            int used = fn(context, &protocol_buf);

            if (used == 0) {
                break;
            } else if (used == -1) {
                memset(recv_buf, 0, sizeof(recv_buf));
                offset = 0;
                break;
            } else {
                assert(used > 0);
                offset -= used;
                memmove(recv_buf, recv_buf + used, offset);
            }
        }
    }
}

void neu_conn_stream_tcp_server_consume(neu_conn_t *conn, int fd, void *context,
                                        neu_conn_stream_consume_fn fn)
{
    static __thread uint8_t  recv_buf[2048] = { 0 };
    static __thread uint16_t offset         = 0;
    ssize_t ret = neu_conn_tcp_server_recv(conn, fd, recv_buf + offset,
                                           sizeof(recv_buf) - offset);
    if (ret > 0) {
        offset += ret;
        neu_protocol_unpack_buf_t protocol_buf = { 0 };
        neu_protocol_unpack_buf_init(&protocol_buf, recv_buf, offset);
        while (neu_protocol_unpack_buf_unused_size(&protocol_buf) > 0) {
            int used = fn(context, &protocol_buf);

            if (used == 0) {
                break;
            } else if (used == -1) {
                memset(recv_buf, 0, sizeof(recv_buf));
                offset = 0;
                break;
            } else {
                assert(used > 0);
                offset -= used;
                memmove(recv_buf, recv_buf + used, offset);
            }
        }
    }
}