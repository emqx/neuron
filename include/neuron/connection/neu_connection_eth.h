/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef NEURON_CONNECTION_ETH_H
#define NEURON_CONNECTION_ETH_H

#include <stdint.h>

#include "define.h"

// profinet

typedef struct neu_conn_eth neu_conn_eth_t;

neu_conn_eth_t *neu_conn_eth_init(const char *interface, void *ctx);
int             neu_conn_eth_uninit(neu_conn_eth_t *conn);
int             neu_conn_eth_check_interface(const char *interface);
void            neu_conn_eth_get_mac(neu_conn_eth_t *conn, uint8_t *mac);

typedef void (*neu_conn_eth_msg_callback)(neu_conn_eth_t *conn, void *ctx,
                                          uint16_t protocol, uint16_t n_byte,
                                          uint8_t *bytes, uint8_t src_mac[6]);

int neu_conn_eth_register_lldp(neu_conn_eth_t *conn, const char *device,
                               neu_conn_eth_msg_callback callback);
int neu_conn_eth_unregister_lldp(neu_conn_eth_t *conn, const char *device);

typedef struct neu_conn_eth_sub neu_conn_eth_sub_t;

neu_conn_eth_sub_t *neu_conn_eth_register(neu_conn_eth_t *conn,
                                          uint16_t protocol, uint8_t mac[6],
                                          neu_conn_eth_msg_callback callback);
int neu_conn_eth_unregister(neu_conn_eth_t *conn, neu_conn_eth_sub_t *sub);

int neu_conn_eth_send(neu_conn_eth_t *conn, uint8_t dst_mac[6],
                      uint16_t protocol, uint16_t n_byte, uint8_t *bytes);

#endif