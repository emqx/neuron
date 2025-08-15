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
#ifndef _NEU_M_PLUGIN_MODBUS_REQ_H_
#define _NEU_M_PLUGIN_MODBUS_REQ_H_

/**
 * modbus_req.h
 * 本文件定义了 Modbus 协议插件的请求处理、连接管理、定时器等接口，
 * 负责处理 Modbus 协议的各种请求和响应，是插件与协议栈交互的核心。
 */

#include <neuron.h>

#include "modbus_stack.h"

/**
 * Modbus 插件结构体，包含插件的所有状态和配置
 */
struct neu_plugin {
    neu_plugin_common_t common;      // 通用插件部分

    neu_conn_t *    conn;            // 连接对象
    modbus_stack_t *stack;           // Modbus 协议栈

    void *   plugin_group_data;      // 插件组数据
    uint16_t cmd_idx;                // 当前命令索引

    neu_event_io_t *tcp_server_io;   // TCP 服务器 IO 事件
    bool            is_server;       // 是否为服务器模式
    bool            is_serial;       // 是否为串口模式
    int             client_fd;       // 客户端文件描述符
    neu_events_t *  events;          // 事件系统

    modbus_protocol_e protocol;      // 协议类型

    uint16_t interval;               // 轮询间隔
    uint16_t retry_interval;         // 重试间隔
    uint16_t max_retries;            // 最大重试次数

    modbus_endianess_64 endianess_64; // 64位数据字节序
};

/**
 * 连接成功的回调函数
 * @param data 插件指针
 * @param fd 连接文件描述符
 */
void modbus_conn_connected(void *data, int fd);

/**
 * 连接断开的回调函数
 * @param data 插件指针
 * @param fd 连接文件描述符
 */
void modbus_conn_disconnected(void *data, int fd);

/**
 * TCP服务器监听回调函数
 * @param data 插件指针
 * @param fd 监听文件描述符
 */
void modbus_tcp_server_listen(void *data, int fd);

/**
 * TCP服务器停止监听回调函数
 * @param data 插件指针
 * @param fd 监听文件描述符
 */
void modbus_tcp_server_stop(void *data, int fd);

/**
 * TCP服务器IO事件回调函数
 * @param type IO事件类型
 * @param fd 文件描述符
 * @param usr_data 用户数据
 * @return 处理结果
 */
int  modbus_tcp_server_io_callback(enum neu_event_io_type type, int fd,
                                   void *usr_data);

/**
 * 处理分组定时器，发送读请求并处理响应
 * @param plugin 插件指针
 * @param group 分组
 * @param max_byte 最大字节数
 * @return 处理结果
 */
int modbus_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group,
                       uint16_t max_byte);

/**
 * 发送消息回调函数
 * @param ctx 上下文指针
 * @param n_byte 发送的字节数
 * @param bytes 发送的数据
 * @return 成功返回发送的字节数，失败返回负数
 */
int modbus_send_msg(void *ctx, uint16_t n_byte, uint8_t *bytes);

/**
 * 处理接收到的数据值
 * @param ctx 上下文指针
 * @param slave_id 从站 ID
 * @param n_byte 接收的字节数
 */
int modbus_value_handle(void *ctx, uint8_t slave_id, uint16_t n_byte,
                        uint8_t *bytes, int error);

/**
 * 执行写操作
 * @param plugin 插件指针
 * @param req 请求上下文
 * @param tag 标签
 * @param value 写入值
 * @param response 是否需要响应
 * @return 处理结果
 */
int modbus_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                 neu_value_u value, bool response);

/**
 * 写入单个标签
 * @param plugin 插件指针
 * @param req 请求上下文
 * @param tag 标签
 * @param value 写入值
 * @return 处理结果
 */
int modbus_write_tag(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                     neu_value_u value);

/**
 * 批量写入标签
 * @param plugin 插件指针
 * @param req 请求上下文
 * @param tags 标签数组
 * @return 处理结果
 */
int modbus_write_tags(neu_plugin_t *plugin, void *req, UT_array *tags);

/**
 * 处理写响应的回调函数
 * @param ctx 上下文指针
 * @param req 请求上下文
 * @param error 错误码
 * @return 处理结果
 */
int modbus_write_resp(void *ctx, void *req, int error);

#endif
