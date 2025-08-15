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
#ifndef _NEU_M_PLUGIN_MODBUS_H_
#define _NEU_M_PLUGIN_MODBUS_H_

/**
 * modbus.h
 * 本文件定义了 Modbus 协议的基础数据结构和常量，包括：
 * - 功能码枚举
 * - 区域类型枚举
 * - 字节序枚举
 * - 协议报文结构
 * - 数据包处理函数
 */

#include <stdbool.h>
#include <stdint.h>

#include <neuron.h>

/* 字节序说明:
   LL -> LE 1,2,3,4    (小端，小端)
   LB -> LE 2,1,4,3    (小端，大端)
   BB -> BE 3,4,1,2    (大端，大端)
   BL -> BE 4,3,2,1    (大端，小端)
   L64 -> 1,2,3,4,5,6,7,8    (小端)
   B64 -> 8,7,6,5,4,3,2,1    (大端)
*/

/* 大端字节序 */

/**
 * Modbus 动作类型枚举
 */
typedef enum modbus_action {
    MODBUS_ACTION_DEFAULT        = 0,  // 默认动作
    MODBUS_ACTION_HOLD_REG_WRITE = 1   // 保持寄存器写入动作
} modbus_action_e;

/**
 * Modbus 功能码枚举，定义了支持的所有功能码和错误码
 */
typedef enum modbus_function {
    MODBUS_READ_COIL            = 0x1,   // 读线圈
    MODBUS_READ_INPUT           = 0x02,  // 读离散输入
    MODBUS_READ_HOLD_REG        = 0x03,  // 读保持寄存器
    MODBUS_READ_INPUT_REG       = 0x04,  // 读输入寄存器
    MODBUS_WRITE_S_COIL         = 0x05,  // 写单个线圈
    MODBUS_WRITE_S_HOLD_REG     = 0x06,  // 写单个保持寄存器
    MODBUS_WRITE_M_HOLD_REG     = 0x10,  // 写多个保持寄存器
    MODBUS_WRITE_M_COIL         = 0x0F,  // 写多个线圈
    MODBUS_READ_COIL_ERR        = 0x81,  // 读线圈错误
    MODBUS_READ_INPUT_ERR       = 0x82,  // 读离散输入错误
    MODBUS_READ_HOLD_REG_ERR    = 0x83,  // 读保持寄存器错误
    MODBUS_READ_INPUT_REG_ERR   = 0x84,  // 读输入寄存器错误
    MODBUS_WRITE_S_COIL_ERR     = 0x85,  // 写单个线圈错误
    MODBUS_WRITE_S_HOLD_REG_ERR = 0x86,  // 写单个保持寄存器错误
    MODBUS_WRITE_M_HOLD_REG_ERR = 0x90,  // 写多个保持寄存器错误
    MODBUS_WRITE_M_COIL_ERR     = 0x8F,  // 写多个线圈错误
    MODBUS_DEVICE_ERR           = -2     // 设备错误
} modbus_function_e;

/**
 * Modbus 区域类型枚举，定义了 Modbus 协议的四种数据区域
 */
typedef enum modbus_area {
    MODBUS_AREA_COIL           = 0,  // 线圈区域（可读写的位区域）
    MODBUS_AREA_INPUT          = 1,  // 离散输入区域（只读的位区域）
    MODBUS_AREA_INPUT_REGISTER = 3,  // 输入寄存器区域（只读的寄存器区域）
    MODBUS_AREA_HOLD_REGISTER  = 4,  // 保持寄存器区域（可读写的寄存器区域）
} modbus_area_e;

/**
 * Modbus 64位数据字节序枚举
 */
typedef enum modbus_endianess_64 {
    MODBUS_LL = 1, // 小端-小端：12 34 56 78
    MODBUS_LB = 2, // 小端-大端：21 43 65 87
    MODBUS_BB = 3, // 大端-大端：87 65 43 21
    MODBUS_BL = 4, // 大端-小端：78 56 34 12
} modbus_endianess_64;

/**
 * Modbus TCP 协议报文头部结构
 */
struct modbus_header {
    uint16_t seq;       // 序列号
    uint16_t protocol;  // 协议标识
    uint16_t len;       // 数据长度
} __attribute__((packed));

/**
 * 打包 Modbus 报文头部
 * @param buf 打包缓冲区
 * @param seq 序列号
 */
void modbus_header_wrap(neu_protocol_pack_buf_t *buf, uint16_t seq);

/**
 * 解析 Modbus 报文头部
 * @param buf 解包缓冲区
 * @param out_header 输出头部结构
 * @return 成功返回头部大小，失败返回负数
 */
int  modbus_header_unwrap(neu_protocol_unpack_buf_t *buf,
                          struct modbus_header *     out_header);

/**
 * Modbus 功能码结构
 */
struct modbus_code {
    uint8_t slave_id;   // 从站 ID
    uint8_t function;   // 功能码
} __attribute__((packed));

/**
 * 打包 Modbus 功能码
 * @param buf 打包缓冲区
 * @param slave_id 从站 ID
 * @param function 功能码
 */
void modbus_code_wrap(neu_protocol_pack_buf_t *buf, uint8_t slave_id,
                      uint8_t function);

/**
 * 解析 Modbus 功能码
 * @param buf 解包缓冲区
 * @param out_code 输出功能码结构
 * @return 成功返回功能码大小，失败返回负数
 */
int  modbus_code_unwrap(neu_protocol_unpack_buf_t *buf,
                        struct modbus_code *       out_code);

/**
 * Modbus 地址结构
 */
struct modbus_address {
    uint16_t start_address;  // 起始地址
    uint16_t n_reg;          // 寄存器数量
} __attribute__((packed));

/**
 * 打包 Modbus 地址
 * @param buf 打包缓冲区
 * @param start 起始地址
 * @param n_register 寄存器数量
 * @param m_action Modbus 动作类型
 */
void modbus_address_wrap(neu_protocol_pack_buf_t *buf, uint16_t start,
                         uint16_t n_register, enum modbus_action m_action);

/**
 * 解析 Modbus 地址
 * @param buf 解包缓冲区
 * @param out_address 输出地址结构
 * @return 成功返回地址大小，失败返回负数
 */
int  modbus_address_unwrap(neu_protocol_unpack_buf_t *buf,
                           struct modbus_address *    out_address);

/**
 * Modbus 数据结构
 */
struct modbus_data {
    uint8_t n_byte;     // 数据字节数
    uint8_t byte[];     // 数据字节数组（可变长度）
} __attribute__((packed));

/**
 * 打包 Modbus 数据
 * @param buf 打包缓冲区
 * @param n_byte 数据字节数
 * @param bytes 数据字节数组
 * @param action Modbus 动作类型
 */
void modbus_data_wrap(neu_protocol_pack_buf_t *buf, uint8_t n_byte,
                      uint8_t *bytes, enum modbus_action action);

/**
 * 解析 Modbus 数据
 * @param buf 解包缓冲区
 * @param out_data 输出数据结构
 * @return 成功返回数据头大小，失败返回负数
 */
int  modbus_data_unwrap(neu_protocol_unpack_buf_t *buf,
                        struct modbus_data *       out_data);

/**
 * Modbus RTU CRC 校验结构
 */
struct modbus_crc {
    uint16_t crc;  // CRC 校验值
} __attribute__((packed));

/**
 * 计算并设置 Modbus CRC 校验值
 * @param buf 打包缓冲区
 */
void modbus_crc_set(neu_protocol_pack_buf_t *buf);

/**
 * 在缓冲区中预留 CRC 校验的空间
 * @param buf 打包缓冲区
 */
void modbus_crc_wrap(neu_protocol_pack_buf_t *buf);

/**
 * 解析 Modbus CRC 校验
 * @param buf 解包缓冲区
 * @param out_crc 输出 CRC 结构
 * @return 成功返回 CRC 大小，失败返回负数
 */
int  modbus_crc_unwrap(neu_protocol_unpack_buf_t *buf,
                       struct modbus_crc *        out_crc);

/**
 * 将 Modbus 区域类型转换为字符串
 * @param area 区域类型
 * @return 区域类型的字符串表示
 */
const char *modbus_area_to_str(modbus_area_e area);

#endif