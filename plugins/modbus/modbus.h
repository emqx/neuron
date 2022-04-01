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

#ifndef _NEU_MODBUS_H_
#define _NEU_MODBUS_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/queue.h>

typedef enum modbus_function {
    MODBUS_READ_COIL          = 0x1,  // bit
    MODBUS_READ_INPUT_CONTACT = 0x02, // bit
    MODBUS_READ_HOLD_REG      = 0x03, // word
    MODBUS_READ_INPUT_REG     = 0x04, // word
    MODBUS_WRITE_S_COIL       = 0x05, // bit
    MODBUS_WRITE_S_HOLD_REG   = 0x06, // word
    MODBUS_WRITE_M_HOLD_REG   = 0x10, // bit
    MODBUS_WRITE_M_COIL       = 0x0F  // word
} modbus_function_e;

typedef enum modbus_area {
    MODBUS_AREA_COIL           = 0,
    MODBUS_AREA_INPUT          = 1,
    MODBUS_AREA_INPUT_REGISTER = 3,
    MODBUS_AREA_HOLD_REGISTER  = 4,
} modbus_area_e;

typedef enum modbus_endian {
    MODBUS_ENDIAN_UNDEFINE   = -1,
    MODBUS_ENDIAN_BIG4321    = 0,
    MODBUS_ENDIAN_BIG3412    = 1,
    MODBUS_ENDIAN_LITTLE1234 = 2,
    MODBUS_ENDIAN_LITTLE2143 = 3
} modbus_endian_e;

struct modbus_header {
    uint16_t process_no;
    uint16_t flag;
    uint16_t len;
} __attribute__((packed));

struct modbus_code {
    uint8_t device_address;
    uint8_t function_code;
} __attribute__((packed));

struct modbus_pdu_read_request {
    uint16_t start_addr;
    uint16_t n_reg;
} __attribute__((packed));

struct modbus_pdu_read_response {
    uint8_t n_byte;
    uint8_t byte[];
} __attribute__((packed));

struct modbus_pdu_s_write_request {
    uint16_t start_addr;
    uint16_t value;
} __attribute__((packed));

struct modbus_pdu_m_write_request {
    uint16_t start_addr;
    uint16_t n_reg;
    uint8_t  n_byte;
    uint8_t  byte[];
} __attribute__((packed));

struct modbus_pdu_write_response {
    uint16_t start_addr;
    uint16_t n_reg;
} __attribute__((packed));

typedef enum modbus_data_type {
    MODBUS_B8  = 1,
    MODBUS_B16 = 2,
    MODBUS_B32 = 4,
} modbus_data_type_t;

typedef struct modbus_data {
    enum modbus_data_type type;
    union {
        uint8_t  val_u8;
        int16_t  val_i16;
        int32_t  val_i32;
        uint16_t val_u16;
        uint32_t val_u32;
        float    val_f32;
    } val;
    modbus_endian_e endian;
} modbus_data_t;

int modbus_read_req_with_head(char *buf, uint16_t id, uint8_t device,
                              modbus_function_e function_code, uint16_t addr,
                              uint16_t n_reg);

int modbus_read_req(char *buf, uint8_t device_address,
                    modbus_function_e function_code, uint16_t addr,
                    uint16_t n_reg);

int modbus_s_write_req_with_head(char *buf, uint8_t device_address,
                                 modbus_function_e function_code, uint16_t addr,
                                 uint16_t value);

int modbus_s_write_req(char *buf, uint8_t device_address,
                       modbus_function_e function_code, uint16_t addr,
                       uint16_t value);

int modbus_m_write_req_with_head(char *buf, uint8_t device_address,
                                 modbus_function_e function_code, uint16_t addr,
                                 uint16_t n_reg, struct modbus_data *mdata);

int modbus_m_write_req(char *buf, uint8_t device_address,
                       modbus_function_e function_code, uint16_t addr,
                       uint16_t n_reg, struct modbus_data *mdata);

#endif