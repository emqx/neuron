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

#include <netinet/in.h>
#include <stdio.h>

#include "modbus.h"

static uint16_t calcrc(char *buf, int len);

uint16_t calcrc(char *buf, int len)
{
    uint16_t crc     = 0xffff;
    uint16_t crcpoly = 0xa001;

    for (int i = 0; i < len; i++) {
        uint8_t x = buf[i];
        crc ^= x;
        for (int k = 0; k < 8; k++) {
            uint16_t usepoly = crc & 0x1;
            crc >>= 1;
            if (usepoly)
                crc ^= crcpoly;
        }
    }

    return crc;
}

int modbus_read_req_with_head(char *buf, uint16_t id, uint8_t device_address,
                              modbus_function_e function_code, uint16_t addr,
                              uint16_t n_reg)
{
    struct modbus_header *          header = (struct modbus_header *) buf;
    struct modbus_code *            code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_request *request =
        (struct modbus_pdu_read_request *) &code[1];
    uint16_t len = sizeof(*header) + sizeof(*code) + sizeof(*request);

    header->process_no = htons(id);
    header->flag       = 0x0000;
    header->len        = htons(len - sizeof(*header));

    code->device_address = device_address;
    code->function_code  = function_code;

    request->start_addr = htons(addr);
    request->n_reg      = htons(n_reg);

    return len;
}

int modbus_read_req(char *buf, uint8_t device_address,
                    modbus_function_e function_code, uint16_t addr,
                    uint16_t n_reg)
{
    struct modbus_code *            code = (struct modbus_code *) buf;
    struct modbus_pdu_read_request *request =
        (struct modbus_pdu_read_request *) &code[1];
    uint16_t *crc = (uint16_t *) &request[1];
    uint16_t  len = sizeof(*code) + sizeof(*request) + sizeof(*crc);

    code->device_address = device_address;
    code->function_code  = function_code;

    request->start_addr = htons(addr);
    request->n_reg      = htons(n_reg);

    *crc = calcrc(buf, len - sizeof(*crc));

    return len;
}

int modbus_s_write_req_with_head(char *buf, uint8_t device_address,
                                 modbus_function_e function_code, uint16_t addr,
                                 uint16_t value)
{
    struct modbus_header *             header = (struct modbus_header *) buf;
    struct modbus_code *               code = (struct modbus_code *) &header[1];
    struct modbus_pdu_s_write_request *request =
        (struct modbus_pdu_s_write_request *) &code[1];
    uint16_t len = sizeof(*header) + sizeof(*code) + sizeof(*request);

    header->process_no = 0x0000;
    header->flag       = 0x0000;
    header->len        = htons(len - sizeof(*header));

    code->device_address = device_address;
    code->function_code  = function_code;

    request->start_addr = htons(addr);
    request->value      = htons(value);

    return len;
}

int modbus_s_write_req(char *buf, uint8_t device_address,
                       modbus_function_e function_code, uint16_t addr,
                       uint16_t value)
{
    struct modbus_code *               code = (struct modbus_code *) buf;
    struct modbus_pdu_s_write_request *request =
        (struct modbus_pdu_s_write_request *) &code[1];
    uint16_t *crc = (uint16_t *) &request[1];
    uint16_t  len = sizeof(*code) + sizeof(*request) + sizeof(*crc);

    code->device_address = device_address;
    code->function_code  = function_code;

    request->start_addr = htons(addr);
    request->value      = htons(value);

    *crc = calcrc(buf, len - sizeof(*crc));

    return len;
}

int modbus_m_write_req_with_head(char *buf, uint8_t device_address,
                                 modbus_function_e function_code, uint16_t addr,
                                 uint16_t n_reg, struct modbus_data *mdata)
{
    struct modbus_header *             header = (struct modbus_header *) buf;
    struct modbus_code *               code = (struct modbus_code *) &header[1];
    struct modbus_pdu_m_write_request *request =
        (struct modbus_pdu_m_write_request *) &code[1];
    uint8_t * data8  = (uint8_t *) &request[1];
    uint16_t *data16 = (uint16_t *) &request[1];
    uint32_t *data32 = (uint32_t *) &request[1];

    uint16_t len = sizeof(*header) + sizeof(*code) + sizeof(*request);

    header->process_no = 0x0000;
    header->flag       = 0x0000;

    code->device_address = device_address;
    code->function_code  = function_code;

    request->start_addr = htons(addr);
    request->n_reg      = htons(n_reg);
    request->n_byte     = mdata->type;
    len += mdata->type;

    switch (mdata->type) {
    case MODBUS_B8:
        *data8 = mdata->val.val_u8;
        break;
    case MODBUS_B16:
        *data16 = htons(mdata->val.val_u16);
        break;
    case MODBUS_B32:
        *data32 = htonl(mdata->val.val_u32);
        break;
    }

    header->len = htons(len - sizeof(*header));
    return len;
}

int modbus_m_write_req(char *buf, uint8_t device_address,
                       modbus_function_e function_code, uint16_t addr,
                       uint16_t n_reg, struct modbus_data *mdata)
{
    struct modbus_code *               code = (struct modbus_code *) buf;
    struct modbus_pdu_m_write_request *request =
        (struct modbus_pdu_m_write_request *) &code[1];
    uint8_t * data8  = (uint8_t *) &request[1];
    uint16_t *data16 = (uint16_t *) &request[1];
    uint32_t *data32 = (uint32_t *) &request[1];
    uint16_t *crc    = NULL;

    uint16_t len = sizeof(*code) + sizeof(*request) + sizeof(*crc);

    code->device_address = device_address;
    code->function_code  = function_code;

    request->start_addr = htons(addr);
    request->n_reg      = htons(n_reg);
    request->n_byte     = mdata->type;
    len += mdata->type;

    switch (mdata->type) {
    case MODBUS_B8:
        *data8 = mdata->val.val_u8;
        crc    = (uint16_t *) &data8[1];
        break;
    case MODBUS_B16:
        *data16 = htons(mdata->val.val_u16);
        crc     = (uint16_t *) &data16[1];
        break;
    case MODBUS_B32:
        *data32 = htonl(mdata->val.val_u32);
        crc     = (uint16_t *) &data32[1];
        break;
    }

    *crc = calcrc(buf, len - sizeof(*crc));
    return len;
}
