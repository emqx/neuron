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
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#include "log.h"

#include "connection/neu_tcp.h"

struct neu_tcp_server {
    in_addr_t host;
    uint16_t  port;
    int       listen_fd;
    int       client_fd;
};

neu_tcp_server_t *neu_tcp_server_create(char *local_host, uint16_t local_port)
{
    neu_tcp_server_t * server = calloc(1, sizeof(neu_tcp_server_t));
    struct sockaddr_in local  = { 0 };
    int                ret    = 0;

    server->host = inet_addr(local_host);
    server->port = htons(local_port);
    server->listen_fd =
        socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

    //    signal(SIGPIPE, SIG_IGN);

    if (server->listen_fd < 0) {
        log_error("new socket fail, errno: %s", strerror(errno));
        free(server);
        return NULL;
    }

    local.sin_family      = AF_INET;
    local.sin_port        = server->port;
    local.sin_addr.s_addr = server->host;

    ret = bind(server->listen_fd, (struct sockaddr *) &local, sizeof(local));

    if (ret != 0) {
        log_error("bind: %s:%d fail, errno: %s", local_host, local_port,
                  strerror(errno));
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    ret = listen(server->listen_fd, 1);

    if (ret != 0) {
        log_error("listen: %s:%d fail, errno: %s", local_host, local_port,
                  strerror(errno));
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    log_info("listen: %s:%d success, listen fd: %d", local_host, local_port,
             server->listen_fd);
    return server;
}

int neu_tcp_server_wait_client(neu_tcp_server_t *server)
{
    struct sockaddr_in client     = { 0 };
    socklen_t          client_len = 0;
    int                fd         = 0;
    struct timeval     tv         = { .tv_sec = 5, .tv_usec = 0 };

    if (server->client_fd != 0) {
        close(server->client_fd);
        server->client_fd = 0;
    }

    fd = accept(server->listen_fd, (struct sockaddr *) &client, &client_len);
    if (fd <= 0) {
        log_error("accept error: %s", strerror(errno));
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    server->client_fd = fd;

    log_info("accept new client: %s:%d, client fd: %d, listen fd: %d",
             inet_ntoa(client.sin_addr), ntohs(client.sin_port), fd,
             server->listen_fd);

    return 0;
}

void neu_tcp_server_close(neu_tcp_server_t *server)
{
    close(server->client_fd);
    close(server->listen_fd);

    free(server);
}

ssize_t neu_tcp_server_send_recv(neu_tcp_server_t *server, char *send_buf,
                                 ssize_t send_len, char *recv_buf,
                                 ssize_t recv_len)
{
    ssize_t ret = 0;

    if (server->client_fd <= 0) {
        log_error("no client connected to the server, listen fd: %d",
                  server->listen_fd);
        return -1;
    }

    ret = send(server->client_fd, send_buf, send_len, MSG_NOSIGNAL);
    if (ret <= 0 || ret != send_len) {
        log_error("send buf error, ret: %d, errno: %s, len: %d", ret,
                  strerror(errno), send_len);
        return -1;
    }

    ret = recv(server->client_fd, recv_buf, recv_len, MSG_WAITALL);
    if (ret <= 0 || ret != recv_len) {
        log_error("recv buf error, ret: %d, errno: %s, len: %d", ret,
                  strerror(errno), recv_len);
        return -1;
    }

    return 0;
}

ssize_t neu_tcp_server_recv(neu_tcp_server_t *server, char *recv_buf,
                            ssize_t buf_len, ssize_t recv_len)
{
    ssize_t ret = 0;

    if (server->client_fd <= 0) {
        log_error("no client connected to the server, listen fd: %d",
                  server->listen_fd);
        return -1;
    }

    if (recv_len > 0) {
        ret = recv(server->client_fd, recv_buf, recv_len, MSG_WAITALL);
    } else {
        ret = recv(server->client_fd, recv_buf, buf_len, 0);
    }

    if (ret <= 0) {
        log_error("recv buf error, ret: %d, errno: %s, len: %d", ret,
                  strerror(errno), recv_len);
        return -1;
    }

    return ret;
}

ssize_t neu_tcp_server_send(neu_tcp_server_t *server, char *send_buf,
                            ssize_t send_len)
{
    ssize_t ret = 0;

    if (server->client_fd <= 0) {
        log_error("no client connected to the server, listen fd: %d",
                  server->listen_fd);
        return -1;
    }

    ret = send(server->client_fd, send_buf, send_len, MSG_NOSIGNAL);
    if (ret <= 0 || ret != send_len) {
        log_error("send buf error, ret: %d, errno: %s, len: %d", ret,
                  strerror(errno), send_len);
        return -1;
    }

    return ret;
}
