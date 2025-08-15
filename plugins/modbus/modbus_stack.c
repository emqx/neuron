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

/**
 * modbus_stack.c
 * 本文件实现了 Modbus 协议栈的核心功能，包括：
 * - 协议栈的创建和销毁
 * - 数据收发操作
 * - 读写命令的处理
 * - TCP/RTU 协议的支持
 */

/**
 * Modbus 协议栈结构体
 */
struct modbus_stack {
    void                   *ctx;        // 上下文指针
    modbus_stack_send       send_fn;    // 发送数据的回调函数
    modbus_stack_value      value_fn;   // 处理值的回调函数
    modbus_stack_write_resp write_resp; // 处理写响应的回调函数

    modbus_protocol_e protocol;  // 协议类型：TCP 或 RTU
    uint16_t          read_seq;  // 读请求序列号
    uint16_t          write_seq; // 写请求序列号

    uint8_t *buf;      // 数据缓冲区
    uint16_t buf_size; // 缓冲区大小
};

/**
 * 创建 Modbus 协议栈对象
 *
 * @param ctx 上下文指针
 * @param protocol 协议类型：TCP 或 RTU
 * @param send_fn 发送数据的回调函数
 * @param value_fn 处理值的回调函数
 * @param write_resp 处理写响应的回调函数
 * @return 协议栈对象指针
 */
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

/**
 * 销毁 Modbus 协议栈对象并释放资源
 *
 * @param stack 协议栈对象
 */
void modbus_stack_destroy(modbus_stack_t *stack)
{
    free(stack->buf);
    free(stack);
}

/**
 * 处理接收到的 Modbus 数据包
 *
 * @param stack 协议栈对象
 * @param slave_id 从站 ID
 * @param buf 数据缓冲区
 * @return 成功返回数据包大小，失败返回负数
 */
int modbus_stack_recv(modbus_stack_t *stack, uint8_t slave_id,
                      neu_protocol_unpack_buf_t *buf)
{
    struct modbus_header header = { 0 };
    struct modbus_code   code   = { 0 };
    int                  ret    = 0;

    if (stack->protocol == MODBUS_PROTOCOL_TCP) {
        ret = modbus_header_unwrap(buf, &header);
        if (ret < 0) {
            plog_warn((neu_plugin_t *) stack->ctx, "try modbus rtu driver");
        }
        if (ret <= 0) {
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
        struct modbus_data data  = { 0 };
        uint8_t           *bytes = NULL;
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
                                bytes, 0);
            } else {
                bytes = neu_protocol_unpack_buf(buf, data.n_byte);
                if (bytes == NULL) {
                    return -1;
                }
                stack->value_fn(stack->ctx, code.slave_id, data.n_byte, bytes,
                                0);
            }
            break;
        case MODBUS_PROTOCOL_RTU:
            bytes = neu_protocol_unpack_buf(buf, data.n_byte);
            if (bytes == NULL) {
                return -1;
            }
            stack->value_fn(stack->ctx, code.slave_id, data.n_byte, bytes, 0);
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

/**
 * 发送 Modbus 读请求
 *
 * @param stack 协议栈对象
 * @param slave_id 从站 ID
 * @param area Modbus 区域类型
 * @param start_address 起始地址
 * @param n_reg 寄存器数量
 * @param response_size 返回预期响应大小
 * @return 成功返回发送的字节数，失败返回负数
 */
int modbus_stack_read(modbus_stack_t *stack, uint8_t slave_id,
                      enum modbus_area area, uint16_t start_address,
                      uint16_t n_reg, uint16_t *response_size)
{
    static __thread uint8_t                 buf[16] = { 0 };
    static __thread neu_protocol_pack_buf_t pbuf    = { 0 };
    int                                     ret     = 0;
    *response_size                                  = 0;
    modbus_action_e m_action                        = MODBUS_ACTION_DEFAULT;

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
    if (ret <= 0) {
        stack->value_fn(stack->ctx, 0, 0, NULL, NEU_ERR_PLUGIN_DISCONNECTED);
        plog_warn((neu_plugin_t *) stack->ctx, "send read req fail, %hhu!%hu",
                  slave_id, start_address);
    }

    return ret;
}

/**
 * 发送 Modbus 写请求
 *
 * @param stack 协议栈对象
 * @param req 请求上下文
 * @param slave_id 从站 ID
 * @param area Modbus 区域类型
 * @param start_address 起始地址
 * @param n_reg 寄存器数量
 * @param bytes 写入数据
 * @param n_byte 数据字节数
 * @param response_size 返回预期响应大小
 * @param response 是否需要响应
 * @return 成功返回发送的字节数，失败返回负数
 */
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

/**
 * 判断协议栈是否为 RTU 模式
 *
 * @param stack 协议栈对象
 * @return 如果是 RTU 模式则返回 true，否则返回 false
 */
bool modbus_stack_is_rtu(modbus_stack_t *stack)
{
    return stack->protocol == MODBUS_PROTOCOL_RTU;
}