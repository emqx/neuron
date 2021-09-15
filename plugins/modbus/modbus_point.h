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

#ifndef _NEU_MODBUS_POINT_H_
#define _NEU_MODBUS_POINT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <sys/queue.h>

#include "modbus.h"

typedef ssize_t (*modbus_point_send_recv)(void *arg, char *send_buf,
                                          ssize_t send_len, char *recv_buf,
                                          ssize_t recv_len);

typedef struct modbus_point_context modbus_point_context_t;

modbus_point_context_t *modbus_point_init(void *arg);
int  modbus_point_add(modbus_point_context_t *ctx, char *addr,
                      modbus_data_type_t type);
int  modbus_point_all_read(modbus_point_context_t *ctx, bool with_head,
                           modbus_point_send_recv callback);
void modbus_point_new_cmd(modbus_point_context_t *ctx);
int  modbus_point_get_cmd_size(modbus_point_context_t *ctx);
void modbus_point_clean(modbus_point_context_t *ctx);
void modbus_point_destory(modbus_point_context_t *ctx);
int  modbus_point_find(modbus_point_context_t *ctx, char *addr,
                       modbus_data_t *data);
int  modbus_point_write(char *addr, modbus_data_t *data,
                        modbus_point_send_recv callback, void *arg);

#ifdef __cplusplus
}
#endif

#endif