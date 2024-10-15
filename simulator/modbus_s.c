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
#include <memory.h>
#include <netinet/in.h>
#include <pthread.h>

#include <neuron.h>

#include "plugins/modbus/modbus.h"

#include "modbus_s.h"

struct modbus_register {
    pthread_mutex_t mutex;
    neu_value8_u    input[65536];
    neu_value16_u   input_register[65536];
    neu_value8_u    coil[65536];
    neu_value16_u   hold_register[65536];
};

static bool                   simulate_error = false;
static struct modbus_register rtu_registers[3];
static struct modbus_register tcp_registers[3];

static int  modbus_read(struct modbus_register *reg, uint8_t function,
                        struct modbus_address *address, uint8_t *value);
static void modbus_write(struct modbus_register *reg, uint8_t function,
                         struct modbus_address *address,
                         struct modbus_data *data, uint8_t *value);

void modbus_s_init()
{
    for (int i = 0; i < 3; i++) {
        for (int k = 0; k < 65536; k += 2) {
            rtu_registers[i].input[k].value = 0;
            tcp_registers[i].input[k].value = 0;
        }
        for (int k = 0; k < 65536; k++) {
            rtu_registers[i].input_register[k].value = 0;
            tcp_registers[i].input_register[k].value = 0;
        }

        pthread_mutex_init(&rtu_registers[i].mutex, NULL);
        pthread_mutex_init(&tcp_registers[i].mutex, NULL);
    }
}

void modbus_s_simulate_error(bool b)
{
    simulate_error = b;
}

ssize_t modbus_s_rtu_req(uint8_t *req, uint16_t req_len, uint8_t *res,
                         int res_mlen, int *res_len)
{
    struct modbus_code *   code    = (struct modbus_code *) req;
    struct modbus_address *address = (struct modbus_address *) &code[1];
    struct modbus_data *   data    = (struct modbus_data *) &address[1];
    uint8_t *              value   = (uint8_t *) &data[1];
    int                    len     = 0;
    int use_len = sizeof(struct modbus_code) + sizeof(struct modbus_address);

    struct modbus_code *   res_code    = (struct modbus_code *) res;
    struct modbus_address *res_address = (struct modbus_address *) &res_code[1];
    struct modbus_data *   res_data    = (struct modbus_data *) &res_code[1];
    uint8_t *              res_value   = (uint8_t *) &res_data[1];
    uint16_t *             crc         = NULL;

    static int start_address_8000_counter = 0;
    static int start_address_8001_counter = 0;

    if (req_len < sizeof(struct modbus_code)) {
        return 0;
    }

    if (code->slave_id != 1 && code->slave_id != 2 && code->slave_id != 3) {
        return -2;
    }

    switch (code->function) {
    case MODBUS_READ_COIL:
    case MODBUS_READ_INPUT:
    case MODBUS_READ_HOLD_REG:
    case MODBUS_READ_INPUT_REG:
        if (req_len <
            sizeof(struct modbus_code) + sizeof(struct modbus_address) + 2) {
            return 0;
        }
        use_len += 2;
        break;
    case MODBUS_WRITE_S_COIL:
    case MODBUS_WRITE_S_HOLD_REG:
        if (req_len <
            sizeof(struct modbus_code) + sizeof(struct modbus_address) + 2) {
            return 0;
        }
        use_len += 2;
        break;
    case MODBUS_WRITE_M_HOLD_REG:
    case MODBUS_WRITE_M_COIL:
        if (req_len < sizeof(struct modbus_code) +
                sizeof(struct modbus_address) + sizeof(struct modbus_data) +
                data->n_byte + 2) {
            return 0;
        }
        use_len += sizeof(struct modbus_data) + data->n_byte + 2;
        break;
    }

    memcpy(res, req, sizeof(struct modbus_code));
    *res_len = sizeof(struct modbus_code);

    switch (code->function) {
    case MODBUS_READ_COIL:
    case MODBUS_READ_INPUT:
    case MODBUS_READ_HOLD_REG:
    case MODBUS_READ_INPUT_REG:
        if (simulate_error && ntohs(address->start_address) == 7999) {
            ++start_address_8000_counter;
            if (start_address_8000_counter % 2 == 1) {
                return -2;
            }
        }

        if (simulate_error && ntohs(address->start_address) == 8004) {
            ++start_address_8001_counter;
            if (start_address_8001_counter % 3 != 0) {
                return -2;
            }
        }

        if (simulate_error && ntohs(address->start_address) == 8999) {
            res_code->function += 0x80;
            len              = modbus_read(&rtu_registers[code->slave_id - 1],
                              res_code->function, address, res_value);
            res_data->n_byte = len;
            crc              = (uint16_t *) (res_value + len);
            break;
        }

        len = modbus_read(&rtu_registers[code->slave_id - 1], code->function,
                          address, res_value);
        *res_len += sizeof(struct modbus_data);
        *res_len += len;

        res_data->n_byte = len;

        crc = (uint16_t *) (res_value + len);
        break;
    case MODBUS_WRITE_S_COIL:
    case MODBUS_WRITE_M_COIL:
    case MODBUS_WRITE_S_HOLD_REG:
    case MODBUS_WRITE_M_HOLD_REG:
        modbus_write(&rtu_registers[code->slave_id - 1], code->function,
                     address, data, value);
        *res_address = *address;

        *res_len += sizeof(struct modbus_address);

        crc = (uint16_t *) &res_address[1];
        break;
    default:
        return -1;
    }

    *crc = 0x1001;
    *res_len += 2;

    assert(res_mlen >= *res_len);

    return use_len;
}

ssize_t modbus_s_tcp_req(uint8_t *req, uint16_t req_len, uint8_t *res,
                         int res_mlen, int *res_len)
{
    struct modbus_header * header  = (struct modbus_header *) req;
    struct modbus_code *   code    = (struct modbus_code *) &header[1];
    struct modbus_address *address = (struct modbus_address *) &code[1];
    struct modbus_data *   data    = (struct modbus_data *) &address[1];
    uint8_t *              value   = (uint8_t *) &data[1];
    int                    len     = 0;

    struct modbus_header * res_header  = (struct modbus_header *) res;
    struct modbus_code *   res_code    = (struct modbus_code *) &res_header[1];
    struct modbus_address *res_address = (struct modbus_address *) &res_code[1];
    struct modbus_data *   res_data    = (struct modbus_data *) &res_code[1];
    uint8_t *              res_value   = (uint8_t *) &res_data[1];

    static int start_address_8000_counter = 0;
    static int start_address_8001_counter = 0;

    if (req_len < sizeof(struct modbus_header)) {
        return 0;
    }

    if (req_len < sizeof(struct modbus_header) + ntohs(header->len)) {
        return 0;
    }

    if (header->protocol != 0x0000) {
        return -1;
    }

    if (code->slave_id != 1 && code->slave_id != 2 && code->slave_id != 3) {
        return -1;
    }

    memcpy(res, req, sizeof(struct modbus_header) + sizeof(struct modbus_code));
    *res_len        = sizeof(struct modbus_header) + sizeof(struct modbus_code);
    res_header->len = sizeof(struct modbus_code);

    switch (code->function) {
    case MODBUS_READ_COIL:
    case MODBUS_READ_INPUT:
    case MODBUS_READ_HOLD_REG:
    case MODBUS_READ_INPUT_REG:
        nlog_info("read register: slave: %d, function: %d, address: %d, n "
                  "register: %d\n",
                  code->slave_id, code->function, ntohs(address->start_address),
                  ntohs(address->n_reg));

        if (simulate_error && ntohs(address->start_address) == 7999) {
            ++start_address_8000_counter;
            if (start_address_8000_counter % 2 == 1) {
                return -2;
            }
        }

        if (simulate_error && ntohs(address->start_address) == 8004) {
            ++start_address_8001_counter;
            if (start_address_8001_counter % 3 != 0) {
                return -2;
            }
        }

        if (simulate_error && ntohs(address->start_address) == 8999) {
            res_code->function += 0x80;
            len              = modbus_read(&tcp_registers[code->slave_id - 1],
                              res_code->function, address, res_value);
            res_data->n_byte = len;
            break;
        }

        len = modbus_read(&tcp_registers[code->slave_id - 1], code->function,
                          address, res_value);
        *res_len += sizeof(struct modbus_data);
        *res_len += len;

        if (len > 0xff) {
            res_data->n_byte = 0xff;
        } else {
            res_data->n_byte = len;
        }
        res_header->len += sizeof(struct modbus_data) + len;
        break;
    case MODBUS_WRITE_S_COIL:
    case MODBUS_WRITE_M_COIL:
    case MODBUS_WRITE_S_HOLD_REG:
    case MODBUS_WRITE_M_HOLD_REG:
        nlog_info(
            "write register: slave: %d, function: %d, address: %d, n: %d, "
            "nbyte: %d\n",
            code->slave_id, code->function, ntohs(address->start_address),
            code->function != MODBUS_WRITE_S_HOLD_REG ? ntohs(address->n_reg)
                                                      : 1,
            code->function != MODBUS_WRITE_S_HOLD_REG ? data->n_byte : 2);
        modbus_write(&tcp_registers[code->slave_id - 1], code->function,
                     address, data, value);
        *res_address = *address;

        *res_len += sizeof(struct modbus_address);
        res_header->len += sizeof(struct modbus_address);
        break;
    default:
        return -1;
    }

    assert(res_mlen >= *res_len);

    res_header->len = htons(res_header->len);

    return sizeof(struct modbus_header) + ntohs(header->len);
}

static int modbus_read(struct modbus_register *reg, uint8_t function,
                       struct modbus_address *address, uint8_t *value)
{
    int len = 0;

    pthread_mutex_lock(&reg->mutex);
    switch (function) {
    case MODBUS_READ_COIL:
    case MODBUS_READ_INPUT:
        len = ntohs(address->n_reg) / 8;
        if (ntohs(address->n_reg) % 8 != 0) {
            len += 1;
        }

        memset(value, 0, ntohs(address->n_reg) / 8 + 1);
        for (int i = 0; i < ntohs(address->n_reg); i++) {
            if (function == MODBUS_READ_COIL) {
                value[i / 8] |=
                    reg->coil[ntohs(address->start_address) + i].value
                    << (i % 8);
            } else {
                value[i / 8] |=
                    reg->input[ntohs(address->start_address) + i].value
                    << (i % 8);
            }

            nlog_info("read coil/input: %d, i: %d\n", value[i / 8], i);
        }

        break;
    case MODBUS_READ_HOLD_REG:
    case MODBUS_READ_INPUT_REG: {
        len = ntohs(address->n_reg) * 2;

        uint16_t *byte = (uint16_t *) value;

        for (int i = 0; i < ntohs(address->n_reg); i++) {
            if (function == MODBUS_READ_HOLD_REG) {
                byte[i] =
                    htons(reg->hold_register[ntohs(address->start_address) + i]
                              .value);
            } else {
                byte[i] =
                    htons(reg->input_register[ntohs(address->start_address) + i]
                              .value);
            }

            nlog_info("read hold/input register: %X, i:%d\n", byte[i], i);
        }

        break;
    }
    case MODBUS_READ_COIL_ERR:
    case MODBUS_READ_INPUT_ERR:
    case MODBUS_READ_HOLD_REG_ERR:
    case MODBUS_READ_INPUT_REG_ERR: {
        len = 0;

        nlog_info("modbus device read err");
        break;
    }

    default:
        assert(1 != 0);
    }

    pthread_mutex_unlock(&reg->mutex);
    return len;
}
static void modbus_write(struct modbus_register *reg, uint8_t function,
                         struct modbus_address *address,
                         struct modbus_data *data, uint8_t *value)
{
    pthread_mutex_lock(&reg->mutex);
    switch (function) {
    case MODBUS_WRITE_S_COIL:
        if (address->n_reg > 0) {
            reg->coil[ntohs(address->start_address)].value = 1;
        } else {
            reg->coil[ntohs(address->start_address)].value = 0;
        }
        break;
    case MODBUS_WRITE_M_COIL:
        for (int i = 0; i < ntohs(address->n_reg); i++) {
            uint8_t x = (value[i / 8] >> (i % 8)) & 0x1;

            nlog_info("write coil byte: %d, %d\n", x, i);
            if (x > 0) {
                reg->coil[ntohs(address->start_address) + i].value = 1;
            } else {
                reg->coil[ntohs(address->start_address) + i].value = 0;
            }
        }
        break;
    case MODBUS_WRITE_S_HOLD_REG:
        reg->hold_register[ntohs(address->start_address)].value = 0;

        reg->hold_register[ntohs(address->start_address)].value |=
            *(uint8_t *) &((struct modbus_data *) &address->n_reg)[0] << 8;
        reg->hold_register[ntohs(address->start_address)].value |=
            *(uint8_t *) &((struct modbus_data *) &address->n_reg)[1];
        break;
    case MODBUS_WRITE_M_HOLD_REG:
        for (int i = 0; i < data->n_byte; i++) {
            reg->hold_register[ntohs(address->start_address) + i / 2].value = 0;
        }

        for (int i = 0; i < data->n_byte; i++) {
            if (i % 2 == 0) {
                reg->hold_register[ntohs(address->start_address) + i / 2]
                    .value |= value[i] << 8;
            } else {
                reg->hold_register[ntohs(address->start_address) + i / 2]
                    .value |= value[i];
            }
            nlog_info("write hold register address: %d, byte: %X\n",
                      ntohs(address->start_address) + i / 2,
                      reg->hold_register[ntohs(address->start_address) + i / 2]
                          .value);
        }

        break;
    default:
        assert(1 != 0);
    }
    pthread_mutex_unlock(&reg->mutex);
}