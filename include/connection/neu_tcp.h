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

#include <stdint.h>

typedef struct neu_tcp_client neu_tcp_client_t;

neu_tcp_client_t *neu_tcp_client_create(char *server, uint16_t port);
void              neu_tcp_client_close(neu_tcp_client_t *client);
ssize_t neu_tcp_client_send_recv(neu_tcp_client_t *client, char *send_buf,
                                 ssize_t send_len, char *recv_buf,
                                 ssize_t recv_len);