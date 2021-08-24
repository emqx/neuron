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
        *data8 = mdata->val.val_8;
        break;
    case MODBUS_B16:
        *data16 = mdata->val.val_16;
        break;
    case MODBUS_B32:
        *data32 = mdata->val.val_32;
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
    uint16_t *crc;

    uint16_t len = sizeof(*code) + sizeof(*request) + sizeof(*crc);

    code->device_address = device_address;
    code->function_code  = function_code;

    request->start_addr = htons(addr);
    request->n_reg      = htons(n_reg);
    request->n_byte     = mdata->type;
    len += mdata->type;

    switch (mdata->type) {
    case MODBUS_B8:
        *data8 = mdata->val.val_8;
        crc    = (uint16_t *) &data8[1];
        break;
    case MODBUS_B16:
        *data16 = mdata->val.val_16;
        crc     = (uint16_t *) &data16[1];
        break;
    case MODBUS_B32:
        *data32 = mdata->val.val_32;
        crc     = (uint16_t *) &data32[1];
        break;
    }

    *crc = calcrc(buf, len - sizeof(*crc));
    return len;
}

/*
**  modbus address format
**  1!400001
**  1!400001.1
**  1!400001#2
*/

int modbus_address_parse(char *str_addr, modbus_point_t *e)
{
    int     n, n1, n2 = 0;
    char    ar;
    uint8_t en = -1;

    e->bit    = -1;
    e->endian = MODBUS_ENDIAN_UNDEFINE;

    n1 =
        sscanf(str_addr, "%hhd!%c%hd.%hhd", &e->device, &ar, &e->addr, &e->bit);
    n2 = sscanf(str_addr, "%hhd!%c%hd#%hhd", &e->device, &ar, &e->addr, &en);

    if (n2 > n1) {
        n = n2;
    } else {
        n = n1;
    }

    if (n != 3 && n != 4) {
        return -1;
    }

    e->endian = en;
    switch (ar) {
    case '0':
        e->area = MODBUS_AREA_COIL;
        break;
    case '1':
        e->area = MODBUS_AREA_INPUT;
        break;
    case '3':
        e->area = MODBUS_AREA_INPUT_REGISTER;
        break;
    case '4':
        e->area = MODBUS_AREA_HOLD_REGISTER;
        break;
    }

    e->addr -= 1;
    return 0;
}

int modbus_point_cmp(modbus_point_t *e1, modbus_point_t *e2)
{
    if (e1->device > e2->device) {
        return 1;
    } else if (e1->device < e2->device) {
        return -1;
    }

    if (e1->area > e2->area) {
        return 1;
    } else if (e1->area < e2->area) {
        return -1;
    }

    if (e1->addr > e2->addr) {
        return 1;
    } else if (e1->addr < e2->addr) {
        return -1;
    }

    if (e1->value.type > e2->value.type) {
        return 1;
    } else if (e1->value.type < e2->value.type) {
        return -1;
    }

    return 0;
}

void modbus_point_pre_process(bool init, modbus_point_t *point)
{
    static uint16_t           id            = 1;
    static modbus_area_e      before_area   = MODBUS_AREA_UNDEFINE;
    static uint8_t            before_device = 0;
    static uint16_t           before_addr   = 0;
    static modbus_data_type_t before_type   = 0;

    if (init) {
        before_area   = MODBUS_AREA_UNDEFINE;
        before_device = 0;
        before_addr   = 0;
        before_type   = 0;
    }

    switch (point->area) {
    case MODBUS_AREA_COIL:
        point->function = MODBUS_READ_COIL;
        break;
    case MODBUS_AREA_INPUT:
        point->function = MODBUS_READ_INPUT_CONTACT;
        break;
    case MODBUS_AREA_INPUT_REGISTER:
        point->function = MODBUS_READ_INPUT_REG;
        break;
    case MODBUS_AREA_HOLD_REGISTER:
        point->function = MODBUS_READ_HOLD_REG;
        break;
    default:
        break;
    }

    if (before_area == MODBUS_AREA_UNDEFINE) {
        point->order  = MODBUS_POINT_ADDR_HEAD;
        before_area   = point->area;
        before_device = point->device;
        before_addr   = point->addr;
        before_type   = point->value.type;
        point->id     = id++;
        return;
    }

    if (point->area != before_area || point->device != before_device) {
        point->order  = MODBUS_POINT_ADDR_HEAD;
        before_area   = point->area;
        before_device = point->device;
        before_addr   = point->addr;
        before_type   = point->value.type;
        point->id     = id++;
        return;
    }

    // same area and device
    if (point->addr == before_addr) {
        point->order = MODBUS_POINT_ADDR_SAME;
        before_type  = point->value.type;
        return;
    }

    switch (point->area) {
    case MODBUS_AREA_COIL:
    case MODBUS_AREA_INPUT:
        if (before_addr / 16 == point->addr / 16) {
            point->order = MODBUS_POINT_ADDR_SAME;
        } else if (before_addr / 16 + 1 == point->addr / 16) {
            point->order = MODBUS_POINT_ADDR_NEXT;
        } else {
            point->order = MODBUS_POINT_ADDR_HEAD;
            point->id    = id++;
        }
        break;
    case MODBUS_AREA_INPUT_REGISTER:
    case MODBUS_AREA_HOLD_REGISTER:
        if ((before_type == MODBUS_B32 && before_addr + 2 == point->addr) ||
            before_addr + 1 == point->addr) {
            point->order = MODBUS_POINT_ADDR_NEXT;
        } else {
            point->order = MODBUS_POINT_ADDR_HEAD;
            point->id    = id++;
        }
        break;
    default:
        break;
    }

    before_type = point->value.type;
    before_addr = point->addr;
}