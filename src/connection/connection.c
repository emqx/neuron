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

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <fcntl.h>
#include <termios.h>

#include "utils/log.h"

#include "connection/neu_connection.h"

#ifndef CMSPAR
#define CMSPAR 010000000000 /* mark or space (stick) parity */
#endif

struct tcp_client {
    int                fd;
    struct sockaddr_in client;
};

struct neu_conn {
    neu_conn_param_t param;
    void *           data;
    bool             is_connected;
    bool             stop;
    bool             connection_ok;

    neu_conn_callback connected;
    neu_conn_callback disconnected;
    bool              callback_trigger;

    pthread_mutex_t mtx;

    int  fd;
    bool block;

    neu_conn_state_t state;

    struct {
        struct tcp_client *clients;
        int                n_client;
        bool               is_listen;
    } tcp_server;

    uint8_t *buf;
    uint16_t buf_size;
    uint16_t offset;
};

static void conn_tcp_server_add_client(neu_conn_t *conn, int fd,
                                       struct sockaddr_in client);
static void conn_tcp_server_del_client(neu_conn_t *conn, int fd);
static int  conn_tcp_server_replace_client(neu_conn_t *conn, int fd,
                                           struct sockaddr_in client);

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

    conn->buf_size = 2048;
    conn->buf      = calloc(conn->buf_size, 1);
    conn->offset   = 0;
    conn->stop     = false;

    conn_tcp_server_listen(conn);

    pthread_mutex_init(&conn->mtx, NULL);

    return conn;
}

void neu_conn_stop(neu_conn_t *conn)
{
    pthread_mutex_lock(&conn->mtx);
    if (conn->tcp_server.is_listen) {
        conn_tcp_server_stop(conn);
    }
    conn->stop = true;
    conn_disconnect(conn);
    pthread_mutex_unlock(&conn->mtx);
}

void neu_conn_start(neu_conn_t *conn)
{
    pthread_mutex_lock(&conn->mtx);
    conn->state.recv_bytes = 0;
    conn->state.send_bytes = 0;
    conn->stop             = false;
    pthread_mutex_unlock(&conn->mtx);
}

neu_conn_t *neu_conn_reconfig(neu_conn_t *conn, neu_conn_param_t *param)
{
    pthread_mutex_lock(&conn->mtx);

    conn_disconnect(conn);
    conn_free_param(conn);
    conn_tcp_server_stop(conn);

    conn_init_param(conn, param);
    conn_tcp_server_listen(conn);

    conn->state.recv_bytes = 0;
    conn->state.send_bytes = 0;

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

    free(conn->buf);
    free(conn);
}

neu_conn_state_t neu_conn_state(neu_conn_t *conn)
{
    return conn->state;
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
        zlog_error(conn->param.log, "%s:%d accpet error: %s",
                   conn->param.params.tcp_server.ip,
                   conn->param.params.tcp_server.port, strerror(errno));
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

    if (conn->tcp_server.n_client >= conn->param.params.tcp_server.max_link) {
        int free_fd = conn_tcp_server_replace_client(conn, fd, client);
        if (free_fd > 0) {
            zlog_warn(conn->param.log, "replace old client %d with %d", free_fd,
                      fd);
        } else {
            close(fd);
            zlog_warn(conn->param.log, "%s:%d accpet max link: %d, reject",
                      conn->param.params.tcp_server.ip,
                      conn->param.params.tcp_server.port,
                      conn->param.params.tcp_server.max_link);
            pthread_mutex_unlock(&conn->mtx);
            return -1;
        }
    } else {
        conn_tcp_server_add_client(conn, fd, client);
    }

    conn->is_connected = true;
    conn->connected(conn->data, fd);
    conn->callback_trigger = true;

    zlog_notice(conn->param.log, "%s:%d accpet new client: %s:%d, fd: %d",
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
    conn->offset = 0;
    memset(conn->buf, 0, conn->buf_size);

    pthread_mutex_unlock(&conn->mtx);
    return 0;
}

ssize_t neu_conn_tcp_server_send(neu_conn_t *conn, int fd, uint8_t *buf,
                                 ssize_t len)
{
    ssize_t ret = 0;

    pthread_mutex_lock(&conn->mtx);
    if (conn->stop) {
        pthread_mutex_unlock(&conn->mtx);
        return ret;
    }

    conn_tcp_server_listen(conn);
    ret = send(fd, buf, len, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (ret > 0) {
        conn->state.send_bytes += ret;
    }

    if (ret <= 0 && errno != EAGAIN) {
        conn->disconnected(conn->data, fd);
        conn_tcp_server_del_client(conn, fd);
    }

    pthread_mutex_unlock(&conn->mtx);

    return ret;
}

ssize_t neu_conn_tcp_server_recv(neu_conn_t *conn, int fd, uint8_t *buf,
                                 ssize_t len)
{
    ssize_t ret = 0;

    pthread_mutex_lock(&conn->mtx);
    if (conn->stop) {
        pthread_mutex_unlock(&conn->mtx);
        return ret;
    }

    if (conn->block) {
        ret = recv(fd, buf, len, MSG_WAITALL);
    } else {
        ret = recv(fd, buf, len, 0);
    }

    if (ret > 0) {
        conn->state.recv_bytes += ret;
    }

    if (ret <= 0) {
        zlog_error(conn->param.log,
                   "conn fd: %d, recv error, "
                   "errno: %s(%d)",
                   conn->fd, strerror(errno), errno);
    }

    pthread_mutex_unlock(&conn->mtx);

    return ret;
}

ssize_t neu_conn_send(neu_conn_t *conn, uint8_t *buf, ssize_t len)
{
    ssize_t ret = 0;

    pthread_mutex_lock(&conn->mtx);
    if (conn->stop) {
        pthread_mutex_unlock(&conn->mtx);
        return ret;
    }

    if (!conn->is_connected) {
        conn_connect(conn);
    }

    if (conn->is_connected) {
        int retry = 0;
        while (ret < len) {
            int rc = 0;

            switch (conn->param.type) {
            case NEU_CONN_UDP_TO:
            case NEU_CONN_TCP_SERVER:
                assert(false);
                break;
            case NEU_CONN_TCP_CLIENT:
            case NEU_CONN_UDP:
                if (conn->block) {
                    rc = send(conn->fd, buf + ret, len - ret, MSG_NOSIGNAL);
                } else {
                    rc = send(conn->fd, buf + ret, len - ret,
                              MSG_NOSIGNAL | MSG_WAITALL);
                }
                break;
            case NEU_CONN_TTY_CLIENT:
                rc = write(conn->fd, buf + ret, len - ret);
                break;
            }

            if (rc > 0) {
                ret += rc;
                if (ret == len) {
                    break;
                }
            } else {
                if (!conn->block && rc == -1 && errno == EAGAIN) {
                    if (retry > 10) {
                        zlog_error(conn->param.log,
                                   "conn fd: %d, send buf len: %zd, ret: %zd, "
                                   "errno: %s(%d)",
                                   conn->fd, len, ret, strerror(errno), errno);
                        break;
                    } else {
                        struct timespec t1 = {
                            .tv_sec  = 0,
                            .tv_nsec = 1000 * 1000 * 50,
                        };
                        struct timespec t2 = { 0 };
                        nanosleep(&t1, &t2);
                        retry++;
                        zlog_warn(conn->param.log,
                                  "not all data send, retry: %d, ret: "
                                  "%zd(%d), len: %zd",
                                  retry, ret, rc, len);
                    }
                } else {
                    ret = rc;
                    break;
                }
            }
        }

        if (ret == -1) {
            if (errno != EAGAIN) {
                conn_disconnect(conn);
            } else {
                if (conn->connection_ok == true) {
                    conn_disconnect(conn);
                }
            }
        }

        if (ret > 0 && conn->callback_trigger == false) {
            conn->connected(conn->data, conn->fd);
            conn->callback_trigger = true;
        }
    }

    if (ret > 0) {
        conn->state.send_bytes += ret;
        conn->connection_ok = true;
    }

    pthread_mutex_unlock(&conn->mtx);

    return ret;
}

ssize_t neu_conn_recv(neu_conn_t *conn, uint8_t *buf, ssize_t len)
{
    ssize_t ret = 0;

    pthread_mutex_lock(&conn->mtx);
    if (conn->stop) {
        pthread_mutex_unlock(&conn->mtx);
        return ret;
    }

    switch (conn->param.type) {
    case NEU_CONN_UDP_TO:
    case NEU_CONN_TCP_SERVER:
        zlog_fatal(conn->param.log, "neu_conn_recv cann't recv tcp server msg");
        assert(1 == 0);
        break;
    case NEU_CONN_TCP_CLIENT:
        if (conn->block) {
            ret = recv(conn->fd, buf, len, MSG_WAITALL);
        } else {
            ret = recv(conn->fd, buf, len, 0);
        }
        break;
    case NEU_CONN_UDP:
        ret = recv(conn->fd, buf, len, 0);
        break;
    case NEU_CONN_TTY_CLIENT:
        ret = read(conn->fd, buf, len);
        while (ret > 0 && ret < len) {
            ssize_t rv = read(conn->fd, buf + ret, len - ret);
            if (rv <= 0) {
                ret = rv;
                break;
            }

            ret += rv;
        }
        break;
    }
    if (conn->param.type == NEU_CONN_TTY_CLIENT) {
        if (ret == -1) {
            zlog_error(
                conn->param.log,
                "tty conn fd: %d, recv buf len %zd, ret: %zd, errno: %s(%d)",
                conn->fd, len, ret, strerror(errno), errno);
            conn_disconnect(conn);
        }
    } else if (conn->param.type == NEU_CONN_UDP ||
               conn->param.type == NEU_CONN_UDP_TO) {
        if (ret <= 0) {
            zlog_error(
                conn->param.log,
                "udp conn fd: %d, recv buf len %zd, ret: %zd, errno: %s(%d)",
                conn->fd, len, ret, strerror(errno), errno);
            conn_disconnect(conn);
        }
    } else {
        if (ret <= 0) {
            zlog_error(
                conn->param.log,
                "tcp conn fd: %d, recv buf len %zd, ret: %zd, errno: %s(%d)",
                conn->fd, len, ret, strerror(errno), errno);
            if (ret == 0 || (ret == -1 && errno != EAGAIN)) {
                conn_disconnect(conn);
            }
        }
    }

    pthread_mutex_unlock(&conn->mtx);

    if (ret > 0) {
        conn->state.recv_bytes += ret;
    }

    return ret;
}
ssize_t neu_conn_udp_sendto(neu_conn_t *conn, uint8_t *buf, ssize_t len,
                            void *dst)
{
    ssize_t ret = 0;

    pthread_mutex_lock(&conn->mtx);
    if (conn->stop) {
        pthread_mutex_unlock(&conn->mtx);
        return ret;
    }

    if (!conn->is_connected) {
        conn_connect(conn);
    }

    if (conn->is_connected) {
        switch (conn->param.type) {
        case NEU_CONN_TCP_CLIENT:
        case NEU_CONN_TCP_SERVER:
        case NEU_CONN_TTY_CLIENT:
        case NEU_CONN_UDP:
            assert(false);
            break;
        case NEU_CONN_UDP_TO:
            if (conn->block) {
                ret = sendto(conn->fd, buf, len, MSG_NOSIGNAL,
                             (struct sockaddr *) dst, sizeof(struct sockaddr));
            } else {
                ret = sendto(conn->fd, buf, len, MSG_NOSIGNAL | MSG_DONTWAIT,
                             (struct sockaddr *) dst, sizeof(struct sockaddr));
            }
            break;
        }
        if (ret != len) {
            zlog_error(
                conn->param.log,
                "conn udp fd: %d, sendto (%s:%d) buf len: %zd, ret: %zd, "
                "errno: %s(%d)",
                conn->fd, inet_ntoa(((struct sockaddr_in *) dst)->sin_addr),
                htons(((struct sockaddr_in *) dst)->sin_port), len, ret,
                strerror(errno), errno);
        }

        if (ret == -1 && errno != EAGAIN) {
            conn_disconnect(conn);
        }

        if (ret > 0 && conn->callback_trigger == false) {
            conn->connected(conn->data, conn->fd);
            conn->callback_trigger = true;
        }
    }

    if (ret > 0) {
        conn->state.send_bytes += ret;
    }

    pthread_mutex_unlock(&conn->mtx);

    return ret;
}

ssize_t neu_conn_udp_recvfrom(neu_conn_t *conn, uint8_t *buf, ssize_t len,
                              void *src)
{
    ssize_t   ret      = 0;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    pthread_mutex_lock(&conn->mtx);
    if (conn->stop) {
        pthread_mutex_unlock(&conn->mtx);
        return ret;
    }

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
    case NEU_CONN_TCP_CLIENT:
    case NEU_CONN_TTY_CLIENT:
    case NEU_CONN_UDP:
        assert(1 == 0);
        break;
    case NEU_CONN_UDP_TO:
        ret =
            recvfrom(conn->fd, buf, len, 0, (struct sockaddr *) src, &addr_len);
        break;
    }
    if (ret <= 0) {
        zlog_error(conn->param.log,
                   "conn udp fd: %d, recv buf len %zd, ret: %zd, errno: %s(%d)",
                   conn->fd, len, ret, strerror(errno), errno);
        if (ret == 0 || (ret == -1 && errno != EAGAIN)) {
            conn_disconnect(conn);
        }
    }

    pthread_mutex_unlock(&conn->mtx);

    if (ret > 0) {
        conn->state.recv_bytes += ret;
    }

    return ret;
}

void neu_conn_connect(neu_conn_t *conn)
{
    pthread_mutex_lock(&conn->mtx);
    conn_connect(conn);
    pthread_mutex_unlock(&conn->mtx);
}

int neu_conn_fd(neu_conn_t *conn)
{
    int fd = -1;
    pthread_mutex_lock(&conn->mtx);
    fd = conn->fd;
    pthread_mutex_unlock(&conn->mtx);
    return fd;
}

void neu_conn_disconnect(neu_conn_t *conn)
{
    pthread_mutex_lock(&conn->mtx);
    conn_disconnect(conn);
    pthread_mutex_unlock(&conn->mtx);
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
        free(conn->param.params.udp.src_ip);
        free(conn->param.params.udp.dst_ip);
        break;
    case NEU_CONN_UDP_TO:
        free(conn->param.params.udpto.src_ip);
        break;
    case NEU_CONN_TTY_CLIENT:
        free(conn->param.params.tty_client.device);
        break;
    }
}

static void conn_init_param(neu_conn_t *conn, neu_conn_param_t *param)
{
    conn->param.type = param->type;
    conn->param.log  = param->log;

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
        conn->param.params.udp.src_ip   = strdup(param->params.udp.src_ip);
        conn->param.params.udp.src_port = param->params.udp.src_port;
        conn->param.params.udp.dst_ip   = strdup(param->params.udp.dst_ip);
        conn->param.params.udp.dst_port = param->params.udp.dst_port;
        conn->param.params.udp.timeout  = param->params.udp.timeout;
        conn->block                     = conn->param.params.udp.timeout > 0;
        break;
    case NEU_CONN_UDP_TO:
        conn->param.params.udpto.src_ip   = strdup(param->params.udpto.src_ip);
        conn->param.params.udpto.src_port = param->params.udpto.src_port;
        conn->param.params.udpto.timeout  = param->params.udpto.timeout;
        conn->block = conn->param.params.udpto.timeout > 0;
        break;
    case NEU_CONN_TTY_CLIENT:
        conn->param.params.tty_client.device =
            strdup(param->params.tty_client.device);
        conn->param.params.tty_client.data   = param->params.tty_client.data;
        conn->param.params.tty_client.stop   = param->params.tty_client.stop;
        conn->param.params.tty_client.baud   = param->params.tty_client.baud;
        conn->param.params.tty_client.parity = param->params.tty_client.parity;
        conn->param.params.tty_client.flow   = param->params.tty_client.flow;
        conn->param.params.tty_client.timeout =
            param->params.tty_client.timeout;
        conn->block = conn->param.params.tty_client.timeout > 0;
        break;
    }
}

static void conn_tcp_server_listen(neu_conn_t *conn)
{
    if (conn->param.type == NEU_CONN_TCP_SERVER &&
        conn->tcp_server.is_listen == false) {
        int fd = -1, ret = 0;

        if (is_ipv4(conn->param.params.tcp_server.ip)) {
            struct sockaddr_in local = {
                .sin_family      = AF_INET,
                .sin_port        = htons(conn->param.params.tcp_server.port),
                .sin_addr.s_addr = inet_addr(conn->param.params.tcp_server.ip),
            };

            fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

            ret = bind(fd, (struct sockaddr *) &local, sizeof(local));
        } else if (is_ipv6(conn->param.params.tcp_server.ip)) {
            struct sockaddr_in6 local = { 0 };
            local.sin6_family         = AF_INET6;
            local.sin6_port = htons(conn->param.params.tcp_server.port);
            inet_pton(AF_INET6, conn->param.params.tcp_server.ip,
                      &local.sin6_addr);

            fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

            ret = bind(fd, (struct sockaddr *) &local, sizeof(local));
        } else {
            zlog_error(conn->param.log, "invalid ip: %s",
                       conn->param.params.tcp_server.ip);
            return;
        }

        if (ret != 0) {
            close(fd);
            zlog_error(conn->param.log, "tcp bind %s:%d fail, errno: %s",
                       conn->param.params.tcp_server.ip,
                       conn->param.params.tcp_server.port, strerror(errno));
            return;
        }

        ret = listen(fd, 1);
        if (ret != 0) {
            close(fd);
            zlog_error(conn->param.log, "tcp bind %s:%d fail, errno: %s",
                       conn->param.params.tcp_server.ip,
                       conn->param.params.tcp_server.port, strerror(errno));
            return;
        }

        conn->fd                   = fd;
        conn->tcp_server.is_listen = true;

        conn->param.params.tcp_server.start_listen(conn->data, fd);
        zlog_notice(conn->param.log, "tcp server listen %s:%d success, fd: %d",
                    conn->param.params.tcp_server.ip,
                    conn->param.params.tcp_server.port, fd);
    }
}

static void conn_tcp_server_stop(neu_conn_t *conn)
{
    if (conn->param.type == NEU_CONN_TCP_SERVER &&
        conn->tcp_server.is_listen == true) {
        zlog_notice(conn->param.log, "tcp server close %s:%d, fd: %d",
                    conn->param.params.tcp_server.ip,
                    conn->param.params.tcp_server.port, conn->fd);

        conn->param.params.tcp_server.stop_listen(conn->data, conn->fd);

        for (int i = 0; i < conn->param.params.tcp_server.max_link; i++) {
            if (conn->tcp_server.clients[i].fd > 0) {
                close(conn->tcp_server.clients[i].fd);
                conn->tcp_server.clients[i].fd = 0;
            }
        }

        if (conn->fd > 0) {
            close(conn->fd);
            conn->fd = 0;
        }

        conn->tcp_server.n_client  = 0;
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

            if (is_ipv4(conn->param.params.tcp_client.ip)) {
                fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            } else {
                fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
            }

            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        } else {
            if (is_ipv4(conn->param.params.tcp_client.ip)) {
                fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
            } else {
                fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
            }
        }

        if (is_ipv4(conn->param.params.tcp_client.ip)) {
            struct sockaddr_in remote = {
                .sin_family      = AF_INET,
                .sin_port        = htons(conn->param.params.tcp_client.port),
                .sin_addr.s_addr = inet_addr(conn->param.params.tcp_client.ip),
            };

            ret = connect(fd, (struct sockaddr *) &remote,
                          sizeof(struct sockaddr_in));
        } else if (is_ipv6(conn->param.params.tcp_client.ip)) {
            struct sockaddr_in6 remote_ip6 = { 0 };
            remote_ip6.sin6_family         = AF_INET6;
            remote_ip6.sin6_port = htons(conn->param.params.tcp_client.port);
            inet_pton(AF_INET6, conn->param.params.tcp_client.ip,
                      &remote_ip6.sin6_addr);

            ret = connect(fd, (struct sockaddr *) &remote_ip6,
                          sizeof(remote_ip6));
        } else {
            zlog_error(conn->param.log, "invalid ip: %s",
                       conn->param.params.tcp_server.ip);
            return;
        }

        if ((conn->block && ret == 0) ||
            (!conn->block && ret != 0 && errno == EINPROGRESS)) {
            zlog_notice(conn->param.log, "connect %s:%d success",
                        conn->param.params.tcp_client.ip,
                        conn->param.params.tcp_client.port);
            conn->is_connected = true;
            conn->fd           = fd;
        } else {
            close(fd);
            zlog_error(conn->param.log, "connect %s:%d error: %s(%d)",
                       conn->param.params.tcp_client.ip,
                       conn->param.params.tcp_client.port, strerror(errno),
                       errno);
            conn->is_connected = false;
            return;
        }

        break;
    }
    case NEU_CONN_UDP: {
        if (conn->block) {
            struct timeval tv = {
                .tv_sec  = conn->param.params.udp.timeout / 1000,
                .tv_usec = (conn->param.params.udp.timeout % 1000) * 1000,
            };

            if (is_ipv4(conn->param.params.udp.dst_ip)) {
                fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            } else {
                fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            }

            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        } else {
            if (is_ipv4(conn->param.params.udp.dst_ip)) {
                fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
            } else {
                fd = socket(AF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
            }
        }

        int so_broadcast = 1;
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &so_broadcast,
                   sizeof(so_broadcast));

        if (is_ipv4(conn->param.params.udp.src_ip)) {
            struct sockaddr_in local = {
                .sin_family      = AF_INET,
                .sin_port        = htons(conn->param.params.udp.src_port),
                .sin_addr.s_addr = inet_addr(conn->param.params.udp.src_ip),
            };

            ret = bind(fd, (struct sockaddr *) &local,
                       sizeof(struct sockaddr_in));
        } else if (is_ipv6(conn->param.params.udp.src_ip)) {
            struct sockaddr_in6 local = { 0 };
            local.sin6_family         = AF_INET6;
            local.sin6_port           = htons(conn->param.params.udp.src_port);
            inet_pton(AF_INET6, conn->param.params.udp.src_ip,
                      &local.sin6_addr);

            ret = bind(fd, (struct sockaddr *) &local,
                       sizeof(struct sockaddr_in6));
        } else {
            zlog_error(conn->param.log, "invalid ip: %s",
                       conn->param.params.tcp_server.ip);
            return;
        }

        if (ret != 0) {
            close(fd);
            zlog_error(conn->param.log, "bind %s:%d error: %s(%d)",
                       conn->param.params.udp.src_ip,
                       conn->param.params.udp.src_port, strerror(errno), errno);
            conn->is_connected = false;
            return;
        }

        if (is_ipv4(conn->param.params.udp.dst_ip)) {
            struct sockaddr_in remote = {
                .sin_family      = AF_INET,
                .sin_port        = htons(conn->param.params.udp.dst_port),
                .sin_addr.s_addr = inet_addr(conn->param.params.udp.dst_ip),
            };
            ret = connect(fd, (struct sockaddr *) &remote,
                          sizeof(struct sockaddr_in));
        } else if (is_ipv6(conn->param.params.udp.dst_ip)) {
            struct sockaddr_in6 remote = { 0 };
            remote.sin6_family         = AF_INET6;
            remote.sin6_port           = htons(conn->param.params.udp.dst_port);
            inet_pton(AF_INET6, conn->param.params.udp.dst_ip,
                      &remote.sin6_addr);

            ret = connect(fd, (struct sockaddr *) &remote,
                          sizeof(struct sockaddr_in6));
        } else {
            zlog_error(conn->param.log, "invalid ip: %s",
                       conn->param.params.tcp_server.ip);
            return;
        }

        if (ret != 0 && errno != EINPROGRESS) {
            close(fd);
            zlog_error(conn->param.log, "connect %s:%d error: %s(%d)",
                       conn->param.params.udp.dst_ip,
                       conn->param.params.udp.dst_port, strerror(errno), errno);
            conn->is_connected = false;
            return;
        } else {
            zlog_notice(conn->param.log, "connect %s:%d success",
                        conn->param.params.udp.dst_ip,
                        conn->param.params.udp.dst_port);
            conn->is_connected = true;
            conn->fd           = fd;
        }
        break;
    }
    case NEU_CONN_UDP_TO: {
        if (conn->block) {
            struct timeval tv = {
                .tv_sec  = conn->param.params.udpto.timeout / 1000,
                .tv_usec = (conn->param.params.udpto.timeout % 1000) * 1000,
            };

            if (is_ipv4(conn->param.params.udpto.src_ip)) {
                fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            } else {
                fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            }
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        } else {
            if (is_ipv4(conn->param.params.udpto.src_ip)) {
                fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
            } else {
                fd = socket(AF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
            }
        }
        int so_broadcast = 1;
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &so_broadcast,
                   sizeof(so_broadcast));

        if (is_ipv4(conn->param.params.udpto.src_ip)) {
            fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);

            struct sockaddr_in local = {
                .sin_family      = AF_INET,
                .sin_port        = htons(conn->param.params.udpto.src_port),
                .sin_addr.s_addr = inet_addr(conn->param.params.udpto.src_ip),
            };

            ret = bind(fd, (struct sockaddr *) &local,
                       sizeof(struct sockaddr_in));
        } else if (is_ipv6(conn->param.params.udp.dst_ip)) {
            fd = socket(AF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);

            struct sockaddr_in6 local = { 0 };
            local.sin6_family         = AF_INET6;
            local.sin6_port = htons(conn->param.params.udpto.src_port);

            inet_pton(AF_INET6, conn->param.params.udpto.src_ip,
                      &local.sin6_addr);

            ret = bind(fd, (struct sockaddr *) &local,
                       sizeof(struct sockaddr_in6));
        } else {
            zlog_error(conn->param.log, "invalid ip: %s",
                       conn->param.params.tcp_server.ip);
            return;
        }

        if (ret != 0) {
            close(fd);
            zlog_error(conn->param.log, "bind %s:%d error: %s(%d)",
                       conn->param.params.udpto.src_ip,
                       conn->param.params.udpto.src_port, strerror(errno),
                       errno);
            conn->is_connected = false;
            return;
        }

        conn->is_connected = true;
        conn->fd           = fd;

        break;
    }
    case NEU_CONN_TTY_CLIENT: {
        struct termios tty_opt = { 0 };
#ifdef NEU_SMART_LINK
#include "connection/neu_smart_link.h"
        ret =
            neu_conn_smart_link_auto_set(conn->param.params.tty_client.device);
        zlog_notice(conn->param.log, "smart link ret: %d", ret);
        if (ret > 0) {
            fd = ret;
        }
#else
        fd = open(conn->param.params.tty_client.device, O_RDWR | O_NOCTTY, 0);
#endif
        if (fd <= 0) {
            zlog_error(conn->param.log, "open %s error: %s(%d)",
                       conn->param.params.tty_client.device, strerror(errno),
                       errno);
            return;
        }

        tcgetattr(fd, &tty_opt);
        tty_opt.c_cc[VTIME] = conn->param.params.tty_client.timeout / 100;
        tty_opt.c_cc[VMIN]  = 0;

        switch (conn->param.params.tty_client.flow) {
        case NEU_CONN_TTYP_FLOW_DISABLE:
            tty_opt.c_cflag &= ~CRTSCTS;
            break;
        case NEU_CONN_TTYP_FLOW_ENABLE:
            tty_opt.c_cflag |= CRTSCTS;
            break;
        }

        switch (conn->param.params.tty_client.baud) {
        case NEU_CONN_TTY_BAUD_115200:
            cfsetospeed(&tty_opt, B115200);
            cfsetispeed(&tty_opt, B115200);
            break;
        case NEU_CONN_TTY_BAUD_57600:
            cfsetospeed(&tty_opt, B57600);
            cfsetispeed(&tty_opt, B57600);
            break;
        case NEU_CONN_TTY_BAUD_38400:
            cfsetospeed(&tty_opt, B38400);
            cfsetispeed(&tty_opt, B38400);
            break;
        case NEU_CONN_TTY_BAUD_19200:
            cfsetospeed(&tty_opt, B19200);
            cfsetispeed(&tty_opt, B19200);
            break;
        case NEU_CONN_TTY_BAUD_9600:
            cfsetospeed(&tty_opt, B9600);
            cfsetispeed(&tty_opt, B9600);
            break;
        case NEU_CONN_TTY_BAUD_4800:
            cfsetospeed(&tty_opt, B4800);
            cfsetispeed(&tty_opt, B4800);
            break;
        case NEU_CONN_TTY_BAUD_2400:
            cfsetospeed(&tty_opt, B2400);
            cfsetispeed(&tty_opt, B2400);
            break;
        case NEU_CONN_TTY_BAUD_1800:
            cfsetospeed(&tty_opt, B1800);
            cfsetispeed(&tty_opt, B1800);
            break;
        case NEU_CONN_TTY_BAUD_1200:
            cfsetospeed(&tty_opt, B1200);
            cfsetispeed(&tty_opt, B1200);
            break;
        case NEU_CONN_TTY_BAUD_600:
            cfsetospeed(&tty_opt, B600);
            cfsetispeed(&tty_opt, B600);
            break;
        case NEU_CONN_TTY_BAUD_300:
            cfsetospeed(&tty_opt, B300);
            cfsetispeed(&tty_opt, B300);
            break;
        case NEU_CONN_TTY_BAUD_200:
            cfsetospeed(&tty_opt, B200);
            cfsetispeed(&tty_opt, B200);
            break;
        case NEU_CONN_TTY_BAUD_150:
            cfsetospeed(&tty_opt, B150);
            cfsetispeed(&tty_opt, B150);
            break;
        }

        switch (conn->param.params.tty_client.parity) {
        case NEU_CONN_TTY_PARITY_NONE:
            tty_opt.c_cflag &= ~PARENB;
            break;
        case NEU_CONN_TTY_PARITY_ODD:
            tty_opt.c_cflag |= PARENB;
            tty_opt.c_cflag |= PARODD;
            tty_opt.c_iflag = INPCK;
            break;
        case NEU_CONN_TTY_PARITY_EVEN:
            tty_opt.c_cflag |= PARENB;
            tty_opt.c_cflag &= ~PARODD;
            tty_opt.c_iflag = INPCK;
            break;
        case NEU_CONN_TTY_PARITY_MARK:
            tty_opt.c_cflag |= PARENB;
            tty_opt.c_cflag |= PARODD;
            tty_opt.c_cflag |= CMSPAR;
            tty_opt.c_iflag = INPCK;
            break;
        case NEU_CONN_TTY_PARITY_SPACE:
            tty_opt.c_cflag |= PARENB;
            tty_opt.c_cflag |= CMSPAR;
            tty_opt.c_cflag &= ~PARODD;
            tty_opt.c_iflag = INPCK;
            break;
        }

        tty_opt.c_cflag &= ~CSIZE;
        switch (conn->param.params.tty_client.data) {
        case NEU_CONN_TTY_DATA_5:
            tty_opt.c_cflag |= CS5;
            break;
        case NEU_CONN_TTY_DATA_6:
            tty_opt.c_cflag |= CS6;
            break;
        case NEU_CONN_TTY_DATA_7:
            tty_opt.c_cflag |= CS7;
            break;
        case NEU_CONN_TTY_DATA_8:
            tty_opt.c_cflag |= CS8;
            break;
        }

        switch (conn->param.params.tty_client.stop) {
        case NEU_CONN_TTY_STOP_1:
            tty_opt.c_cflag &= ~CSTOPB;
            break;
        case NEU_CONN_TTY_STOP_2:
            tty_opt.c_cflag |= CSTOPB;
            break;
        }

        tty_opt.c_cflag |= CREAD | CLOCAL;
        tty_opt.c_lflag &= ~ICANON;
        tty_opt.c_lflag &= ~ECHO;
        tty_opt.c_lflag &= ~ECHOE;
        tty_opt.c_lflag &= ~ECHONL;
        tty_opt.c_lflag &= ~ISIG;

        tty_opt.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty_opt.c_iflag &=
            ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
        tty_opt.c_oflag &= ~OPOST;
        tty_opt.c_oflag &= ~ONLCR;

        tcflush(fd, TCIOFLUSH);
        tcsetattr(fd, TCSANOW, &tty_opt);

        conn->fd           = fd;
        conn->is_connected = true;
        zlog_notice(conn->param.log, "open %s success, fd: %d",
                    conn->param.params.tty_client.device, fd);
        break;
    }
    }
}

static void conn_disconnect(neu_conn_t *conn)
{
    conn->is_connected  = false;
    conn->connection_ok = false;
    if (conn->callback_trigger == true) {
        conn->disconnected(conn->data, conn->fd);
        conn->callback_trigger = false;
    }
    conn->offset = 0;
    memset(conn->buf, 0, conn->buf_size);

    switch (conn->param.type) {
    case NEU_CONN_TCP_SERVER:
        for (int i = 0; i < conn->param.params.tcp_server.max_link; i++) {
            if (conn->tcp_server.clients[i].fd > 0) {
                close(conn->tcp_server.clients[i].fd);
                conn->tcp_server.clients[i].fd = 0;
            }
        }
        conn->tcp_server.n_client = 0;
        break;
    case NEU_CONN_TCP_CLIENT:
    case NEU_CONN_UDP:
    case NEU_CONN_UDP_TO:
    case NEU_CONN_TTY_CLIENT:
        if (conn->fd > 0) {
            close(conn->fd);
            conn->fd = 0;
        }
        break;
    }
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
        if (fd > 0 && conn->tcp_server.clients[i].fd == fd) {
            close(fd);
            conn->tcp_server.clients[i].fd = 0;
            conn->tcp_server.n_client -= 1;
            return;
        }
    }
}

static int conn_tcp_server_replace_client(neu_conn_t *conn, int fd,
                                          struct sockaddr_in client)
{
    for (int i = 0; i < conn->param.params.tcp_server.max_link; i++) {
        if (conn->tcp_server.clients[i].fd > 0) {
            int ret = conn->tcp_server.clients[i].fd;

            close(conn->tcp_server.clients[i].fd);
            conn->tcp_server.clients[i].fd = 0;

            conn->tcp_server.clients[i].fd     = fd;
            conn->tcp_server.clients[i].client = client;
            return ret;
        }
    }

    return 0;
}

int neu_conn_stream_consume(neu_conn_t *conn, void *context,
                            neu_conn_stream_consume_fn fn)
{
    ssize_t ret = neu_conn_recv(conn, conn->buf + conn->offset,
                                conn->buf_size - conn->offset);
    if (ret > 0) {
        zlog_recv_protocol(conn->param.log, conn->buf + conn->offset, ret);
        conn->offset += ret;
        neu_protocol_unpack_buf_t protocol_buf = { 0 };
        neu_protocol_unpack_buf_init(&protocol_buf, conn->buf, conn->offset);
        while (neu_protocol_unpack_buf_unused_size(&protocol_buf) > 0) {
            int used = fn(context, &protocol_buf);

            zlog_debug(conn->param.log, "buf used: %d offset: %d", used,
                       conn->offset);
            if (used == 0) {
                break;
            } else if (used == -1) {
                neu_conn_disconnect(conn);
                break;
            } else {
                pthread_mutex_lock(&conn->mtx);
                if (conn->offset - used < 0) {
                    pthread_mutex_unlock(&conn->mtx);
                    zlog_warn(conn->param.log, "reset offset: %d, used: %d",
                              conn->offset, used);
                    return -1;
                }
                conn->offset -= used;
                memmove(conn->buf, conn->buf + used, conn->offset);
                neu_protocol_unpack_buf_init(&protocol_buf, conn->buf,
                                             conn->offset);
                pthread_mutex_unlock(&conn->mtx);
            }
        }
    }

    return ret;
}

int neu_conn_stream_tcp_server_consume(neu_conn_t *conn, int fd, void *context,
                                       neu_conn_stream_consume_fn fn)
{
    ssize_t ret = neu_conn_tcp_server_recv(conn, fd, conn->buf + conn->offset,
                                           conn->buf_size - conn->offset);
    if (ret > 0) {
        conn->offset += ret;
        neu_protocol_unpack_buf_t protocol_buf = { 0 };
        neu_protocol_unpack_buf_init(&protocol_buf, conn->buf, conn->offset);
        while (neu_protocol_unpack_buf_unused_size(&protocol_buf) > 0) {
            int used = fn(context, &protocol_buf);

            if (used == 0) {
                break;
            } else if (used == -1) {
                neu_conn_tcp_server_close_client(conn, fd);
                break;
            } else {
                pthread_mutex_lock(&conn->mtx);
                if (conn->offset - used < 0) {
                    pthread_mutex_unlock(&conn->mtx);
                    zlog_warn(conn->param.log, "reset offset: %d, used: %d",
                              conn->offset, used);
                    return -1;
                }
                conn->offset -= used;
                memmove(conn->buf, conn->buf + used, conn->offset);
                neu_protocol_unpack_buf_init(&protocol_buf, conn->buf,
                                             conn->offset);
                pthread_mutex_unlock(&conn->mtx);
            }
        }
    }
    return ret;
}

int neu_conn_wait_msg(neu_conn_t *conn, void *context, uint16_t n_byte,
                      neu_conn_process_msg fn)
{
    ssize_t                   ret  = neu_conn_recv(conn, conn->buf, n_byte);
    neu_protocol_unpack_buf_t pbuf = { 0 };
    conn->offset                   = 0;

    while (ret > 0) {
        zlog_recv_protocol(conn->param.log, conn->buf + conn->offset, ret);
        conn->offset += ret;
        neu_protocol_unpack_buf_init(&pbuf, conn->buf, conn->offset);
        neu_buf_result_t result = fn(context, &pbuf);
        if (result.need > 0) {
            if (result.need <= conn->buf_size - conn->offset) {
                ret =
                    neu_conn_recv(conn, conn->buf + conn->offset, result.need);
            } else {
                zlog_error(conn->param.log,
                           "no enough recv buf, need: %" PRIu16
                           " , buf size: %" PRIu16 " offset: %" PRIu16,
                           result.need, conn->buf_size, conn->offset);
                return -1;
            }
        } else if (result.need == 0) {
            return result.used;
        } else {
            neu_conn_disconnect(conn);
            return result.used;
        }
    }

    return ret;
}

int neu_conn_tcp_server_wait_msg(neu_conn_t *conn, int fd, void *context,
                                 uint16_t n_byte, neu_conn_process_msg fn)
{
    ssize_t ret = neu_conn_tcp_server_recv(conn, fd, conn->buf, n_byte);
    neu_protocol_unpack_buf_t pbuf = { 0 };
    conn->offset                   = 0;

    while (ret > 0) {
        zlog_recv_protocol(conn->param.log, conn->buf + conn->offset, ret);
        conn->offset += ret;
        neu_protocol_unpack_buf_init(&pbuf, conn->buf, conn->offset);
        neu_buf_result_t result = fn(context, &pbuf);
        if (result.need > 0) {
            assert(result.need <= conn->buf_size - conn->offset);
            if (result.need <= conn->buf_size - conn->offset) {
                ret = neu_conn_tcp_server_recv(
                    conn, fd, conn->buf + conn->offset, result.need);
            } else {
                zlog_error(conn->param.log,
                           "no enough recv buf, need: %" PRIu16
                           " , buf size: %" PRIu16 " offset: %" PRIu16,
                           result.need, conn->buf_size, conn->offset);
                return -1;
            }
        } else if (result.need == 0) {
            return result.used;
        } else {
            neu_conn_tcp_server_close_client(conn, fd);
            return result.used;
        }
    }

    return ret;
}

int is_ipv4(const char *ip)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
}

int is_ipv6(const char *ip)
{
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, ip, &(sa.sin6_addr)) != 0;
}