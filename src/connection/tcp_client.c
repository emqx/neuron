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
#include <stdlib.h>
#include <sys/time.h>

#include "neu_log.h"

#include "connection/neu_tcp.h"

struct neu_tcp_client {
    in_addr_t server;
    uint16_t  port;
    int       fd;
};

static int client_connect(neu_tcp_client_t *client);

neu_tcp_client_t *neu_tcp_client_create(char *server, uint16_t port)
{
    neu_tcp_client_t *client =
        (neu_tcp_client_t *) calloc(1, sizeof(neu_tcp_client_t));

    client->port   = htons(port);
    client->server = inet_addr(server);
    client->fd     = client_connect(client);

    return client;
}

void neu_tcp_client_close(neu_tcp_client_t *client)
{
    close(client->fd);
    free(client);
}

ssize_t neu_tcp_client_send_recv(neu_tcp_client_t *client, char *send_buf,
                                 ssize_t send_len, char *recv_buf,
                                 ssize_t recv_len)
{
    if (client->fd < 0) {
        client->fd = client_connect(client);
        return -1;
    } else {
        ssize_t ret = send(client->fd, send_buf, send_len, 0);
        if (ret != send_len) {
            log_error("tcp client send error, ret: %d, len: %d, errno: %d", ret,
                      send_len, errno);
            return -1;
        }

        ret = recv(client->fd, recv_buf, recv_len, 0);
        if (ret == -1) {
            log_error("tcp client recv error, ret: %d, len: %d, errno: %d", ret,
                      recv_len, errno);
            return -1;
        }

        return ret;
    }
}

static int client_connect(neu_tcp_client_t *client)
{
    int                ret  = -1;
    int                fd   = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { 0 };
    struct timeval     tm   = {
        .tv_sec  = 1,
        .tv_usec = 0,
    };

    addr.sin_family      = AF_INET;
    addr.sin_port        = client->port;
    addr.sin_addr.s_addr = client->server;

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tm, sizeof(tm));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tm, sizeof(tm));

    ret = connect(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

    if (ret != 0) {
        log_error("connect %d:%d error: %d", client->server,
                  ntohs(client->server), errno);
        return -1;
    }

    return fd;
}