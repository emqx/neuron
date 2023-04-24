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
#include <assert.h>
#include <netinet/in.h>

#include <neuron.h>

#include "modbus.h"

static uint16_t calcrc(uint8_t *buf, int len)
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

void modbus_header_wrap(neu_protocol_pack_buf_t *buf, uint16_t seq)
{
    assert(neu_protocol_pack_buf_unused_size(buf) >=
           sizeof(struct modbus_header));
    struct modbus_header *header =
        (struct modbus_header *) neu_protocol_pack_buf(
            buf, sizeof(struct modbus_header));

    header->seq      = htons(seq);
    header->protocol = 0x0;
    header->len      = htons(neu_protocol_pack_buf_used_size(buf) -
                        sizeof(struct modbus_header));
}

int modbus_header_unwrap(neu_protocol_unpack_buf_t *buf,
                         struct modbus_header *     out_header)
{
    struct modbus_header *header =
        (struct modbus_header *) neu_protocol_unpack_buf(
            buf, sizeof(struct modbus_header));

    if (header == NULL ||
        ntohs(header->len) > neu_protocol_unpack_buf_unused_size(buf)) {
        return 0;
    }

    if (header->protocol != 0x0) {
        return -1;
    }

    *out_header     = *header;
    out_header->len = ntohs(out_header->len);
    out_header->seq = ntohs(out_header->seq);

    return sizeof(struct modbus_header);
}

void modbus_code_wrap(neu_protocol_pack_buf_t *buf, uint8_t slave_id,
                      uint8_t function)
{
    assert(neu_protocol_pack_buf_unused_size(buf) >=
           sizeof(struct modbus_code));
    struct modbus_code *code = (struct modbus_code *) neu_protocol_pack_buf(
        buf, sizeof(struct modbus_code));

    code->slave_id = slave_id;
    code->function = function;
}

int modbus_code_unwrap(neu_protocol_unpack_buf_t *buf,
                       struct modbus_code *       out_code)
{
    struct modbus_code *code = (struct modbus_code *) neu_protocol_unpack_buf(
        buf, sizeof(struct modbus_code));

    if (code == NULL) {
        return 0;
    }

    *out_code = *code;

    return sizeof(struct modbus_code);
}

void modbus_address_wrap(neu_protocol_pack_buf_t *buf, uint16_t start,
                         uint16_t n_register, enum modbus_action action)
{
    if (action == MODBUS_ACTION_HOLD_REG_WRITE && n_register == 1) {
        assert(neu_protocol_pack_buf_unused_size(buf) >= sizeof(start));
        uint16_t *address =
            (uint16_t *) neu_protocol_pack_buf(buf, sizeof(start));

        *address = htons(start);
    } else {
        assert(neu_protocol_pack_buf_unused_size(buf) >=
               sizeof(struct modbus_address));
        struct modbus_address *address =
            (struct modbus_address *) neu_protocol_pack_buf(
                buf, sizeof(struct modbus_address));

        address->start_address = htons(start);
        address->n_reg         = htons(n_register);
    }
}

int modbus_address_unwrap(neu_protocol_unpack_buf_t *buf,
                          struct modbus_address *    out_address)
{
    struct modbus_address *address =
        (struct modbus_address *) neu_protocol_unpack_buf(
            buf, sizeof(struct modbus_address));

    if (address == NULL) {
        return 0;
    }

    *out_address               = *address;
    out_address->start_address = ntohs(out_address->start_address);
    out_address->n_reg         = ntohs(out_address->n_reg);

    return sizeof(struct modbus_address);
}

void modbus_data_wrap(neu_protocol_pack_buf_t *buf, uint8_t n_byte,
                      uint8_t *bytes, enum modbus_action action)
{
    assert(neu_protocol_pack_buf_unused_size(buf) >=
           sizeof(struct modbus_data) + n_byte);
    uint8_t *data = neu_protocol_pack_buf(buf, n_byte);

    memcpy(data, bytes, n_byte);

    if (action == MODBUS_ACTION_DEFAULT || n_byte > 2) {
        struct modbus_data *mdata =
            (struct modbus_data *) neu_protocol_pack_buf(
                buf, sizeof(struct modbus_data));

        mdata->n_byte = n_byte;
    }
}

int modbus_data_unwrap(neu_protocol_unpack_buf_t *buf,
                       struct modbus_data *       out_data)
{
    struct modbus_data *mdata = (struct modbus_data *) neu_protocol_unpack_buf(
        buf, sizeof(struct modbus_data));

    if (mdata == NULL ||
        mdata->n_byte > neu_protocol_unpack_buf_unused_size(buf)) {
        return 0;
    }

    *out_data = *mdata;

    return sizeof(struct modbus_data);
}

void modbus_crc_set(neu_protocol_pack_buf_t *buf)
{
    uint16_t  crc   = calcrc(neu_protocol_pack_buf_get(buf),
                          neu_protocol_pack_buf_used_size(buf) - 2);
    uint16_t *p_crc = (uint16_t *) neu_protocol_pack_buf_set(
        buf, neu_protocol_pack_buf_used_size(buf) - 2, 2);

    *p_crc = crc;
}

void modbus_crc_wrap(neu_protocol_pack_buf_t *buf)
{
    assert(neu_protocol_pack_buf_unused_size(buf) >= sizeof(struct modbus_crc));
    struct modbus_crc *crc = (struct modbus_crc *) neu_protocol_pack_buf(
        buf, sizeof(struct modbus_crc));

    crc->crc = 0;
}

int modbus_crc_unwrap(neu_protocol_unpack_buf_t *buf,
                      struct modbus_crc *        out_crc)
{
    struct modbus_crc *crc = (struct modbus_crc *) neu_protocol_unpack_buf(
        buf, sizeof(struct modbus_crc));

    if (crc == NULL) {
        return 0;
    }

    *out_crc = *crc;

    return sizeof(struct modbus_crc);
}

const char *modbus_area_to_str(modbus_area_e area)
{
    switch (area) {
    case MODBUS_AREA_COIL:
        return "coil";
    case MODBUS_AREA_INPUT:
        return "input";
    case MODBUS_AREA_INPUT_REGISTER:
        return "input register";
    case MODBUS_AREA_HOLD_REGISTER:
        return "hold register";
    default:
        return "unknown area";
    }
}
