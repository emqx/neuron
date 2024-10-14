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
#ifndef _NEU_M_PLUGIN_MODBUS_H_
#define _NEU_M_PLUGIN_MODBUS_H_

#include <stdbool.h>
#include <stdint.h>

#include <neuron.h>

/* LL -> LE 1,2,3,4
   LB -> LE 2,1,4,3
   BB -> BE 3,4,1,2
   BL -> BE 4,3,2,1
   L64 -> 1,2,3,4,5,6,7,8
   B64 -> 8,7,6,5,4,3,2,1
*/

/* big-endian order */

typedef enum modbus_action {
    MODBUS_ACTION_DEFAULT        = 0,
    MODBUS_ACTION_HOLD_REG_WRITE = 1
} modbus_action_e;

typedef enum modbus_function {
    MODBUS_READ_COIL            = 0x1,
    MODBUS_READ_INPUT           = 0x02,
    MODBUS_READ_HOLD_REG        = 0x03,
    MODBUS_READ_INPUT_REG       = 0x04,
    MODBUS_WRITE_S_COIL         = 0x05,
    MODBUS_WRITE_S_HOLD_REG     = 0x06,
    MODBUS_WRITE_M_HOLD_REG     = 0x10,
    MODBUS_WRITE_M_COIL         = 0x0F,
    MODBUS_READ_COIL_ERR        = 0x81,
    MODBUS_READ_INPUT_ERR       = 0x82,
    MODBUS_READ_HOLD_REG_ERR    = 0x83,
    MODBUS_READ_INPUT_REG_ERR   = 0x84,
    MODBUS_WRITE_S_COIL_ERR     = 0x85,
    MODBUS_WRITE_S_HOLD_REG_ERR = 0x86,
    MODBUS_WRITE_M_HOLD_REG_ERR = 0x90,
    MODBUS_WRITE_M_COIL_ERR     = 0x8F,
    MODBUS_DEVICE_ERR           = -2
} modbus_function_e;

typedef enum modbus_area {
    MODBUS_AREA_COIL           = 0,
    MODBUS_AREA_INPUT          = 1,
    MODBUS_AREA_INPUT_REGISTER = 3,
    MODBUS_AREA_HOLD_REGISTER  = 4,
} modbus_area_e;

typedef enum modbus_endianess {
    MODBUS_ABCD = 1,
    MODBUS_BADC = 2,
    MODBUS_DCBA = 3,
    MODBUS_CDAB = 4,
} modbus_endianess;

struct modbus_header {
    uint16_t seq;
    uint16_t protocol;
    uint16_t len;
} __attribute__((packed));

void modbus_header_wrap(neu_protocol_pack_buf_t *buf, uint16_t seq);
int  modbus_header_unwrap(neu_protocol_unpack_buf_t *buf,
                          struct modbus_header *     out_header);

struct modbus_code {
    uint8_t slave_id;
    uint8_t function;
} __attribute__((packed));

void modbus_code_wrap(neu_protocol_pack_buf_t *buf, uint8_t slave_id,
                      uint8_t function);
int  modbus_code_unwrap(neu_protocol_unpack_buf_t *buf,
                        struct modbus_code *       out_code);

struct modbus_address {
    uint16_t start_address;
    uint16_t n_reg;
} __attribute__((packed));

void modbus_address_wrap(neu_protocol_pack_buf_t *buf, uint16_t start,
                         uint16_t n_register, enum modbus_action m_action);
int  modbus_address_unwrap(neu_protocol_unpack_buf_t *buf,
                           struct modbus_address *    out_address);

struct modbus_data {
    uint8_t n_byte;
    uint8_t byte[];
} __attribute__((packed));

void modbus_data_wrap(neu_protocol_pack_buf_t *buf, uint8_t n_byte,
                      uint8_t *bytes, enum modbus_action action);
int  modbus_data_unwrap(neu_protocol_unpack_buf_t *buf,
                        struct modbus_data *       out_data);

struct modbus_crc {
    uint16_t crc;
} __attribute__((packed));

void modbus_crc_set(neu_protocol_pack_buf_t *buf);
void modbus_crc_wrap(neu_protocol_pack_buf_t *buf);
int  modbus_crc_unwrap(neu_protocol_unpack_buf_t *buf,
                       struct modbus_crc *        out_crc);

const char *modbus_area_to_str(modbus_area_e area);

#endif