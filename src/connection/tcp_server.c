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

#include "connection/neu_tcp.h"

struct neu_tcp_server_context {
};

neu_tcp_server_context_t *
neu_tcp_server_init(char *host, uint16_t port,
                    neu_tcp_process_response *callback)
{
    return NULL;
}

int neu_tcp_server_send(neu_tcp_server_context_t *, char *buf, ssize_t len)
{
    return 0;
}

void neu_tcp_server_close(neu_tcp_server_context_t *)
{
    return;
}
