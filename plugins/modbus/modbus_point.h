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
#ifndef _NEU_PLUGIN_MODBUS_POINT_H_
#define _NEU_PLUGIN_MODBUS_POINT_H_

/**
 * modbus_point.h
 * 本文件定义了 Modbus 点（数据点）结构体及相关函数
 * 用于描述和操作 Modbus 协议的数据点，支持读写和批量操作
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <neuron.h>

#include "modbus.h"

/**
 * Modbus 点结构体，描述单个 Modbus 数据点
 */
typedef struct modbus_point {
    uint8_t       slave_id;      // 从站 ID
    modbus_area_e area;          // Modbus 区域类型（线圈、输入、保持寄存器等）
    uint16_t      start_address; // 起始地址
    uint16_t      n_register;    // 寄存器数量

    neu_type_e                type;                   // 数据类型
    neu_datatag_addr_option_u option;                 // 地址选项
    char                      name[NEU_TAG_NAME_LEN]; // 点名称
} modbus_point_t;

/**
 * Modbus 写入点结构体，包含点信息和写入值
 */
typedef struct modbus_point_write {
    modbus_point_t point; // Modbus 点信息
    neu_value_u    value; // 写入的值
} modbus_point_write_t;

/**
 * 将通用数据标签转换为 Modbus 点
 *
 * @param tag 数据标签
 * @param point 输出 Modbus 点
 * @return 成功返回 NEU_ERR_SUCCESS，失败返回错误码
 */
int modbus_tag_to_point(const neu_datatag_t *tag, modbus_point_t *point);

/**
 * 将包含值的数据标签转换为写入点
 *
 * @param tag 包含值的数据标签
 * @param point 输出写入点
 * @return 成功返回 NEU_ERR_SUCCESS，失败返回错误码
 */
int modbus_write_tag_to_point(const neu_plugin_tag_value_t *tag,
                              modbus_point_write_t         *point);

/**
 * Modbus 读命令结构体，表示单个批量读请求
 */
typedef struct modbus_read_cmd {
    uint8_t       slave_id;      // 从站 ID
    modbus_area_e area;          // Modbus 区域类型
    uint16_t      start_address; // 起始地址
    uint16_t      n_register;    // 寄存器数量

    UT_array *tags; // modbus_point_t 指针数组
} modbus_read_cmd_t;

/**
 * Modbus 读命令排序结构体，包含多个读命令
 */
typedef struct modbus_read_cmd_sort {
    uint16_t           n_cmd; // 命令数量
    modbus_read_cmd_t *cmd;   // 命令数组
} modbus_read_cmd_sort_t;

/**
 * Modbus 写命令结构体，表示单个批量写请求
 */
typedef struct modbus_write_cmd {
    uint8_t       slave_id;      // 从站 ID
    modbus_area_e area;          // Modbus 区域类型
    uint16_t      start_address; // 起始地址
    uint16_t      n_register;    // 寄存器数量
    uint8_t       n_byte;        // 字节数
    uint8_t      *bytes;         // 数据字节数组

    UT_array *tags; // modbus_point_write_t 指针数组
} modbus_write_cmd_t;

/**
 * Modbus 写命令排序结构体，包含多个写命令
 */
typedef struct modbus_write_cmd_sort {
    uint16_t            n_cmd; // 命令数量
    modbus_write_cmd_t *cmd;   // 命令数组
} modbus_write_cmd_sort_t;

/**
 * 对 Modbus 点进行排序并分组，生成读命令列表
 *
 * @param tags 标签数组
 * @param max_byte 单次读取的最大字节数
 * @return 排序后的读命令列表
 */
modbus_read_cmd_sort_t *modbus_tag_sort(UT_array *tags, uint16_t max_byte);

/**
 * 对写入点进行排序并分组，生成写命令列表
 *
 * @param tags 标签数组
 * @param endianess_64 64位数据字节序
 * @return 排序后的写命令列表
 */
modbus_write_cmd_sort_t *
modbus_write_tags_sort(UT_array *tags, modbus_endianess_64 endianess_64);

/**
 * 释放 modbus_tag_sort 生成的资源
 *
 * @param cs 读命令排序结构
 */
void modbus_tag_sort_free(modbus_read_cmd_sort_t *cs);

/**
 * 转换64位数据的字节序
 *
 * @param value 数据值，会被修改
 * @param endianess_64 目标字节序
 */
void modbus_convert_endianess_64(neu_value_u        *value,
                                 modbus_endianess_64 endianess_64);

#ifdef __cplusplus
}
#endif

#endif