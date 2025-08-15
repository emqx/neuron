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

/**
 * modbus_stack.h
 * 本文件定义了 Modbus 协议栈的接口和数据结构，用于封装 Modbus TCP/RTU 协议的通信功能。
 * 提供了协议栈的创建、销毁、发送和接收数据的函数。
 */

#include <stdint.h>

#include <neuron.h>

#include "modbus.h"

typedef struct modbus_stack modbus_stack_t;

/**
 * 发送数据的回调函数类型
 * @param ctx 上下文指针
 * @param n_byte 发送的字节数
 * @param bytes 发送的数据
 * @return 成功返回发送的字节数，失败返回负数
 */
typedef int (*modbus_stack_send)(void *ctx, uint16_t n_byte, uint8_t *bytes);

/**
 * 处理接收到的数据值的回调函数类型
 * @param ctx 上下文指针
 * @param slave_id 从站 ID
 * @param n_byte 接收的字节数
 * @param bytes 接收的数据
 * @param error 错误码
 * @return 处理结果
 */
typedef int (*modbus_stack_value)(void *ctx, uint8_t slave_id, uint16_t n_byte,
                                  uint8_t *bytes, int error);

/**
 * 处理写响应的回调函数类型
 * @param ctx 上下文指针
 * @param req 请求上下文
 * @param error 错误码
 * @return 处理结果
 */
typedef int (*modbus_stack_write_resp)(void *ctx, void *req, int error);

/**
 * Modbus 协议类型枚举
 */
typedef enum modbus_protocol {
    MODBUS_PROTOCOL_TCP = 1,  // Modbus TCP 协议
    MODBUS_PROTOCOL_RTU = 2,  // Modbus RTU 协议
} modbus_protocol_e;

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
                                    modbus_stack_write_resp write_resp);

/**
 * 销毁 Modbus 协议栈对象并释放资源
 * 
 * @param stack 协议栈对象
 */
void            modbus_stack_destroy(modbus_stack_t *stack);

/**
 * 处理接收到的 Modbus 数据包
 * 
 * @param stack 协议栈对象
 * @param slave_id 从站 ID
 * @param buf 数据缓冲区
 * @return 成功返回数据包大小，失败返回负数
 */
int modbus_stack_recv(modbus_stack_t *stack, uint8_t slave_id,
                      neu_protocol_unpack_buf_t *buf);

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
int  modbus_stack_read(modbus_stack_t *stack, uint8_t slave_id,
                       enum modbus_area area, uint16_t start_address,
                       uint16_t n_reg, uint16_t *response_size);

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
int  modbus_stack_write(modbus_stack_t *stack, void *req, uint8_t slave_id,
                        enum modbus_area area, uint16_t start_address,
                        uint16_t n_reg, uint8_t *bytes, uint8_t n_byte,
                        uint16_t *response_size, bool response);

/**
 * 判断协议栈是否为 RTU 模式
 * 
 * @param stack 协议栈对象
 * @return 如果是 RTU 模式则返回 true，否则返回 false
 */
bool modbus_stack_is_rtu(modbus_stack_t *stack);

#endif