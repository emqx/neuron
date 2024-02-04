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
    const uint16_t table[256] = {
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241, 0xC601,
        0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440, 0xCC01, 0x0CC0,
        0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40, 0x0A00, 0xCAC1, 0xCB81,
        0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841, 0xD801, 0x18C0, 0x1980, 0xD941,
        0x1B00, 0xDBC1, 0xDA81, 0x1A40, 0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01,
        0x1DC0, 0x1C80, 0xDC41, 0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0,
        0x1680, 0xD641, 0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081,
        0x1040, 0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441, 0x3C00,
        0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41, 0xFA01, 0x3AC0,
        0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840, 0x2800, 0xE8C1, 0xE981,
        0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41, 0xEE01, 0x2EC0, 0x2F80, 0xEF41,
        0x2D00, 0xEDC1, 0xEC81, 0x2C40, 0xE401, 0x24C0, 0x2580, 0xE541, 0x2700,
        0xE7C1, 0xE681, 0x2640, 0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0,
        0x2080, 0xE041, 0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281,
        0x6240, 0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41, 0xAA01,
        0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840, 0x7800, 0xB8C1,
        0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41, 0xBE01, 0x7EC0, 0x7F80,
        0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40, 0xB401, 0x74C0, 0x7580, 0xB541,
        0x7700, 0xB7C1, 0xB681, 0x7640, 0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101,
        0x71C0, 0x7080, 0xB041, 0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0,
        0x5280, 0x9241, 0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481,
        0x5440, 0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841, 0x8801,
        0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40, 0x4E00, 0x8EC1,
        0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41, 0x4400, 0x84C1, 0x8581,
        0x4540, 0x8701, 0x47C0, 0x4680, 0x8641, 0x8201, 0x42C0, 0x4380, 0x8341,
        0x4100, 0x81C1, 0x8081, 0x4040
    };
    uint16_t crc = 0xffff;

    for (int i = 0; i < len; i++) {
        crc = (crc >> 8) ^ table[(crc ^ buf[i]) & 0xFF];
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
