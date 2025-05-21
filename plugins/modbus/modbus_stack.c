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

#include <neuron.h>

#include "modbus_req.h"
#include "modbus_stack.h"

#include "otel/otel_manager.h"

struct modbus_stack {
    void *                  ctx;
    modbus_stack_send       send_fn;
    modbus_stack_value      value_fn;
    modbus_stack_write_resp write_resp;

    modbus_protocol_e protocol;
    uint16_t          read_seq;
    uint16_t          write_seq;

    uint8_t *buf;
    uint16_t buf_size;

    int64_t sample_mod;
};

modbus_stack_t *modbus_stack_create(void *ctx, modbus_protocol_e protocol,
                                    modbus_stack_send       send_fn,
                                    modbus_stack_value      value_fn,
                                    modbus_stack_write_resp write_resp)
{
    modbus_stack_t *stack = calloc(1, sizeof(modbus_stack_t));

    stack->ctx        = ctx;
    stack->send_fn    = send_fn;
    stack->value_fn   = value_fn;
    stack->write_resp = write_resp;
    stack->protocol   = protocol;

    stack->buf_size = 256;
    stack->buf      = calloc(stack->buf_size, 1);

    return stack;
}

void modbus_stack_destroy(modbus_stack_t *stack)
{
    free(stack->buf);
    free(stack);
}

int modbus_stack_recv(modbus_stack_t *stack, uint8_t slave_id,
                      neu_protocol_unpack_buf_t *buf)
{
    struct modbus_header header   = { 0 };
    struct modbus_code   code     = { 0 };
    int                  ret      = 0;
    int64_t              ts_start = neu_time_ns();

    if (stack->protocol == MODBUS_PROTOCOL_TCP) {
        ret = modbus_header_unwrap(buf, &header);
        if (ret < 0) {
            plog_warn((neu_plugin_t *) stack->ctx, "try modbus rtu driver");
        }
        if (ret <= 0) {
            return -1;
        }

        neu_plugin_t *plugin = (neu_plugin_t *) stack->ctx;
        if (plugin->check_header && header.seq + 1 != stack->read_seq &&
            header.seq + 1 != stack->write_seq) {
            return -1;
        }
    }

    ret = modbus_code_unwrap(buf, &code);
    if (ret <= 0) {
        return -1;
    }

    if (code.slave_id != slave_id) {
        return -1;
    }

    switch (code.function) {
    case MODBUS_READ_COIL:
    case MODBUS_READ_INPUT:
    case MODBUS_READ_HOLD_REG:
    case MODBUS_READ_INPUT_REG: {
        neu_otel_trace_ctx trace = NULL;
        neu_otel_scope_ctx scope = NULL;
        if (neu_otel_data_is_started()) {
            trace = neu_otel_find_trace((void *) (intptr_t) stack->read_seq);
            if (trace) {
                char new_span_id[36] = { 0 };
                neu_otel_new_span_id(new_span_id);
                scope =
                    neu_otel_add_span2(trace, "driver cmd recv", new_span_id);
                neu_otel_scope_add_span_attr_int(scope, "thread id",
                                                 (int64_t)(pthread_self()));
                neu_otel_scope_set_span_start_time(scope, ts_start);
                neu_otel_scope_set_span_end_time(scope, neu_time_ns());
            }
        }

        struct modbus_data data  = { 0 };
        uint8_t *          bytes = NULL;
        ret                      = modbus_data_unwrap(buf, &data);
        if (ret <= 0) {
            return -1;
        }

        switch (stack->protocol) {
        case MODBUS_PROTOCOL_TCP:
            if (data.n_byte == 0xff) {
                bytes = neu_protocol_unpack_buf(buf,
                                                header.len -
                                                    sizeof(struct modbus_code) -
                                                    sizeof(struct modbus_data));
                if (bytes == NULL) {
                    return -1;
                }
                stack->value_fn(stack->ctx, code.slave_id,
                                header.len - sizeof(struct modbus_code) -
                                    sizeof(struct modbus_data),
                                bytes, 0, (void *) (intptr_t) stack->read_seq);
            } else {
                bytes = neu_protocol_unpack_buf(buf, data.n_byte);
                if (bytes == NULL) {
                    return -1;
                }
                stack->value_fn(stack->ctx, code.slave_id, data.n_byte, bytes,
                                0, (void *) (intptr_t) stack->read_seq);
            }
            break;
        case MODBUS_PROTOCOL_RTU:
            bytes = neu_protocol_unpack_buf(buf, data.n_byte);
            if (bytes == NULL) {
                return -1;
            }
            stack->value_fn(stack->ctx, code.slave_id, data.n_byte, bytes, 0,
                            (void *) (intptr_t) stack->read_seq);
            break;
        }

        break;
    }
    case MODBUS_WRITE_S_COIL:
    case MODBUS_WRITE_M_HOLD_REG:
    case MODBUS_WRITE_M_COIL: {
        struct modbus_address address = { 0 };
        ret                           = modbus_address_unwrap(buf, &address);
        if (ret <= 0) {
            return -1;
        }
        break;
    }
    case MODBUS_READ_COIL_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_READ_INPUT_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_READ_HOLD_REG_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_READ_INPUT_REG_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_WRITE_S_COIL_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_WRITE_S_HOLD_REG_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_WRITE_M_HOLD_REG_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_WRITE_M_COIL_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_WRITE_S_HOLD_REG:
        break;
    default:
        return -1;
    }

    if (stack->protocol == MODBUS_PROTOCOL_RTU) {
        struct modbus_crc crc = { 0 };
        ret                   = modbus_crc_unwrap(buf, &crc);
        if (ret <= 0) {
            return -1;
        }
    }

    return neu_protocol_unpack_buf_used_size(buf);
}

int modbus_stack_recv_test(neu_plugin_t *plugin, void *req,
                           modbus_point_t *           point,
                           neu_protocol_unpack_buf_t *buf)
{
    struct modbus_header header = { 0 };
    struct modbus_code   code   = { 0 };
    int                  ret    = 0;

    if (plugin->stack->protocol == MODBUS_PROTOCOL_TCP) {
        ret = modbus_header_unwrap(buf, &header);
        if (ret < 0) {
            plog_warn((neu_plugin_t *) plugin->stack->ctx,
                      "try modbus rtu driver");
        }
        if (ret <= 0) {
            return -1;
        }
    }

    ret = modbus_code_unwrap(buf, &code);
    if (ret <= 0) {
        return -1;
    }

    if (code.slave_id != point->slave_id) {
        return -1;
    }

    switch (code.function) {
    case MODBUS_READ_COIL:
    case MODBUS_READ_INPUT:
    case MODBUS_READ_HOLD_REG:
    case MODBUS_READ_INPUT_REG: {
        struct modbus_data data  = { 0 };
        uint8_t *          bytes = NULL;
        ret                      = modbus_data_unwrap(buf, &data);
        if (ret <= 0) {
            return -1;
        }

        if (data.n_byte == 0xff) {
            bytes = neu_protocol_unpack_buf(buf,
                                            header.len -
                                                sizeof(struct modbus_code) -
                                                sizeof(struct modbus_data));
            if (bytes == NULL) {
                return -1;
            }

            modbus_value_handle_test(plugin, req, point, data.n_byte, bytes);
        } else {
            bytes = neu_protocol_unpack_buf(buf, data.n_byte);
            if (bytes == NULL) {
                return -1;
            }
            modbus_value_handle_test(plugin, req, point, data.n_byte, bytes);
        }

        break;
    }
    case MODBUS_WRITE_S_COIL:
    case MODBUS_WRITE_M_HOLD_REG:
    case MODBUS_WRITE_M_COIL: {
        struct modbus_address address = { 0 };
        ret                           = modbus_address_unwrap(buf, &address);
        if (ret <= 0) {
            return -1;
        }
        break;
    }
    case MODBUS_READ_COIL_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_READ_INPUT_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_READ_HOLD_REG_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_READ_INPUT_REG_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_WRITE_S_COIL_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_WRITE_S_HOLD_REG_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_WRITE_M_HOLD_REG_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_WRITE_M_COIL_ERR:
        return MODBUS_DEVICE_ERR;
    case MODBUS_WRITE_S_HOLD_REG:
        break;
    default:
        return -1;
    }

    return neu_protocol_unpack_buf_used_size(buf);
}

int modbus_stack_read(modbus_stack_t *stack, uint8_t slave_id,
                      enum modbus_area area, uint16_t start_address,
                      uint16_t n_reg, uint16_t *response_size, bool is_test)
{
    static __thread uint8_t                 buf[16] = { 0 };
    static __thread neu_protocol_pack_buf_t pbuf    = { 0 };
    int                                     ret     = 0;
    *response_size                                  = 0;
    modbus_action_e m_action                        = MODBUS_ACTION_DEFAULT;
    int64_t         ts_start                        = neu_time_ns();

    neu_protocol_pack_buf_init(&pbuf, buf, sizeof(buf));

    if (stack->protocol == MODBUS_PROTOCOL_RTU) {
        modbus_crc_wrap(&pbuf);
    }
    modbus_address_wrap(&pbuf, start_address, n_reg, m_action);

    switch (area) {
    case MODBUS_AREA_COIL:
        modbus_code_wrap(&pbuf, slave_id, MODBUS_READ_COIL);
        *response_size += n_reg / 8 + ((n_reg % 8) > 0 ? 1 : 0);
        break;
    case MODBUS_AREA_INPUT:
        modbus_code_wrap(&pbuf, slave_id, MODBUS_READ_INPUT);
        *response_size += n_reg / 8 + ((n_reg % 8) > 0 ? 1 : 0);
        break;
    case MODBUS_AREA_INPUT_REGISTER:
        modbus_code_wrap(&pbuf, slave_id, MODBUS_READ_INPUT_REG);
        *response_size += n_reg * 2;
        break;
    case MODBUS_AREA_HOLD_REGISTER:
        modbus_code_wrap(&pbuf, slave_id, MODBUS_READ_HOLD_REG);
        *response_size += n_reg * 2;
        break;
    }

    *response_size += sizeof(struct modbus_code);
    *response_size += sizeof(struct modbus_data);

    switch (stack->protocol) {
    case MODBUS_PROTOCOL_TCP:
        modbus_header_wrap(&pbuf, stack->read_seq++);
        *response_size += sizeof(struct modbus_header);
        break;
    case MODBUS_PROTOCOL_RTU:
        modbus_crc_set(&pbuf);
        *response_size += 2;
        break;
    }

    ret = stack->send_fn(stack->ctx, neu_protocol_pack_buf_used_size(&pbuf),
                         neu_protocol_pack_buf_get(&pbuf));
    if (ret <= 0 && !is_test) {
        stack->value_fn(stack->ctx, 0, 0, NULL, NEU_ERR_PLUGIN_DISCONNECTED,
                        NULL);
        plog_warn((neu_plugin_t *) stack->ctx, "send read req fail, %hhu!%hu",
                  slave_id, start_address);
    } else {
        if (neu_otel_data_is_started()) {
            double rate        = neu_otel_data_sample_rate();
            int    sample_rate = 0;
            if (rate > 0.0 && rate <= 1.0) {
                sample_rate = (int) (1.0 / rate);
            }
            if (sample_rate != 0) {
                stack->sample_mod += 1;
                if (stack->sample_mod % sample_rate == 0) {
                    char new_trace_id[64] = { 0 };
                    neu_otel_new_trace_id(new_trace_id);
                    const char *trace_state  = "span.mytype=data-collection";
                    neu_otel_trace_ctx trace = NULL;
                    neu_otel_scope_ctx scope = NULL;
                    trace                    = neu_otel_create_trace(
                        new_trace_id, (void *) (intptr_t) stack->read_seq, 0,
                        trace_state);
                    if (trace) {
                        scope = neu_otel_add_span(trace);
                        neu_otel_scope_set_span_name(scope, "driver cmd send");
                        char new_span_id[36] = { 0 };
                        neu_otel_new_span_id(new_span_id);
                        neu_otel_scope_set_span_id(scope, new_span_id);
                        neu_otel_scope_set_span_flags(scope, 0);
                        neu_otel_scope_set_span_start_time(scope, ts_start);

                        neu_otel_scope_add_span_attr_int(
                            scope, "thread id", (int64_t) pthread_self());

                        neu_otel_scope_add_span_attr_string(
                            scope, "node",
                            ((neu_plugin_t *) stack->ctx)->common.name);
                        neu_otel_scope_set_span_end_time(scope, neu_time_ns());
                    }
                }
            }
        }
    }
    return ret;
}

int modbus_stack_write(modbus_stack_t *stack, void *req, uint8_t slave_id,
                       enum modbus_area area, uint16_t start_address,
                       uint16_t n_reg, uint8_t *bytes, uint8_t n_byte,
                       uint16_t *response_size, bool response)
{
    static __thread neu_protocol_pack_buf_t pbuf     = { 0 };
    modbus_action_e                         m_action = MODBUS_ACTION_DEFAULT;

    memset(stack->buf, 0, stack->buf_size);
    neu_protocol_pack_buf_init(&pbuf, stack->buf, stack->buf_size);

    if (stack->protocol == MODBUS_PROTOCOL_RTU) {
        modbus_crc_wrap(&pbuf);
    }

    switch (area) {
    case MODBUS_AREA_COIL:
        if (n_byte > 1) {
            n_reg = n_byte;
            modbus_data_wrap(&pbuf, (n_byte + 7) / 8, bytes, m_action);
            modbus_address_wrap(&pbuf, start_address, n_reg, m_action);
            modbus_code_wrap(&pbuf, slave_id, MODBUS_WRITE_M_COIL);
            break;
        } else {
            if (*bytes > 0) {
                modbus_address_wrap(&pbuf, start_address, 0xff00, m_action);
            } else {
                modbus_address_wrap(&pbuf, start_address, 0, m_action);
            }
            modbus_code_wrap(&pbuf, slave_id, MODBUS_WRITE_S_COIL);
            break;
        }
    case MODBUS_AREA_HOLD_REGISTER:
        m_action = MODBUS_ACTION_HOLD_REG_WRITE;
        modbus_data_wrap(&pbuf, n_byte, bytes, m_action);
        modbus_address_wrap(&pbuf, start_address, n_reg, m_action);
        modbus_code_wrap(&pbuf, slave_id,
                         n_reg > 1 ? MODBUS_WRITE_M_HOLD_REG
                                   : MODBUS_WRITE_S_HOLD_REG);
        break;
    default:
        stack->write_resp(stack->ctx, req, NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE);
        break;
    }
    *response_size += sizeof(struct modbus_code);
    *response_size += sizeof(struct modbus_address);

    switch (stack->protocol) {
    case MODBUS_PROTOCOL_TCP:
        modbus_header_wrap(&pbuf, stack->write_seq++);
        *response_size += sizeof(struct modbus_header);
        break;
    case MODBUS_PROTOCOL_RTU:
        modbus_crc_set(&pbuf);
        *response_size += 2;
        break;
    }

    int ret = stack->send_fn(stack->ctx, neu_protocol_pack_buf_used_size(&pbuf),
                             neu_protocol_pack_buf_get(&pbuf));
    if (ret > 0) {
        if (response) {
            stack->write_resp(stack->ctx, req, NEU_ERR_SUCCESS);
            plog_notice((neu_plugin_t *) stack->ctx, "send write req, %hhu!%hu",
                        slave_id, start_address);
        }
    } else {
        if (response) {
            stack->write_resp(stack->ctx, req, NEU_ERR_PLUGIN_DISCONNECTED);
            plog_warn((neu_plugin_t *) stack->ctx,
                      "send write req fail, %hhu!%hu", slave_id, start_address);
        }
    }
    return ret;
}

bool modbus_stack_is_rtu(modbus_stack_t *stack)
{
    return stack->protocol == MODBUS_PROTOCOL_RTU;
}