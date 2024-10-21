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
#ifndef _NEU_M_PLUGIN_MODBUS_STACK_H_
#define _NEU_M_PLUGIN_MODBUS_STACK_H_

#include <stdint.h>

#include <neuron.h>

#include "modbus.h"
#include "modbus_point.h"

typedef struct modbus_stack modbus_stack_t;

typedef int (*modbus_stack_send)(void *ctx, uint16_t n_byte, uint8_t *bytes);
typedef int (*modbus_stack_value)(void *ctx, uint8_t slave_id, uint16_t n_byte,
                                  uint8_t *bytes, int error, void *trace);
typedef int (*modbus_stack_write_resp)(void *ctx, void *req, int error);

typedef enum modbus_protocol {
    MODBUS_PROTOCOL_TCP = 1,
    MODBUS_PROTOCOL_RTU = 2,
} modbus_protocol_e;

modbus_stack_t *modbus_stack_create(void *ctx, modbus_protocol_e protocol,
                                    modbus_stack_send       send_fn,
                                    modbus_stack_value      value_fn,
                                    modbus_stack_write_resp write_resp);
void            modbus_stack_destroy(modbus_stack_t *stack);

int modbus_stack_recv(modbus_stack_t *stack, uint8_t slave_id,
                      neu_protocol_unpack_buf_t *buf);
int modbus_stack_recv_test(neu_plugin_t *plugin, void *req,
                           modbus_point_t *           point,
                           neu_protocol_unpack_buf_t *buf);

int  modbus_stack_read(modbus_stack_t *stack, uint8_t slave_id,
                       enum modbus_area area, uint16_t start_address,
                       uint16_t n_reg, uint16_t *response_size, bool is_test);
int  modbus_stack_write(modbus_stack_t *stack, void *req, uint8_t slave_id,
                        enum modbus_area area, uint16_t start_address,
                        uint16_t n_reg, uint8_t *bytes, uint8_t n_byte,
                        uint16_t *response_size, bool response);
bool modbus_stack_is_rtu(modbus_stack_t *stack);

#endif