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

#include "connection/neu_connection.h"

struct neu_conn_tcp_client {
};

struct neu_conn_ctx {
};

neu_conn_ctx_t *neu_conn_new(neu_conn_t *conn)
{
    (void) conn;

    return NULL;
}

void neu_conn_destory(neu_conn_ctx_t *ctx)
{
    (void) ctx;
    return;
}

neu_conn_type_e neu_conn_get_type(neu_conn_t *conn)
{
    (void) conn;
    return 0;
}

int neu_conn_get_fd(neu_conn_ctx_t *ctx)
{
    (void) ctx;
    return 0;
}

neu_conn_tcp_client_t *neu_conn_tcp_accept(neu_conn_ctx_t *ctx)
{
    (void) ctx;
    return NULL;
}

int neu_conn_tcp_client_get_fd(neu_conn_tcp_client_t *client)
{
    (void) client;
    return 0;
}

ssize_t neu_conn_send(neu_conn_t *conn, neu_conn_tcp_client_t *client,
                      uint8_t *buf, ssize_t len)
{
    (void) conn;
    (void) client;
    (void) buf;
    (void) len;

    return 0;
}

ssize_t neu_conn_recv(neu_conn_t *conn, neu_conn_tcp_client_t *client,
                      uint8_t *buf, ssize_t len)
{
    (void) conn;
    (void) client;
    (void) buf;
    (void) len;

    return;
}