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
config_ **/

#ifndef _NEU_PROTOCOL_BUF_H_
#define _NEU_PROTOCOL_BUF_H_

#include <memory.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    int      used;
    uint16_t need;
} neu_buf_result_t;

typedef struct {
    uint8_t *base;
    uint16_t size;
    uint16_t offset;
} neu_protocol_buf_t, neu_protocol_pack_buf_t, neu_protocol_unpack_buf_t;

/**
 * @brief 初始化协议打包缓冲区
 *
 * @param[in] buf 需要初始化的协议打包缓冲区
 * @param[in] base 协议打包缓冲区使用的内存
 * @param[in] size 内存大小
 */
inline static void neu_protocol_pack_buf_init(neu_protocol_pack_buf_t *buf,
                                              uint8_t *base, uint16_t size)
{
    buf->base   = base;
    buf->size   = size;
    buf->offset = size;
}

/**
 * @brief 初始化协议解包缓冲区
 *
 * @param[in] buf 需要初始化的协议解包缓冲区
 * @param[in] base 协议解包缓冲区使用的内存
 * @param[in] size 内存大小
 */
inline static void neu_protocol_unpack_buf_init(neu_protocol_unpack_buf_t *buf,
                                                uint8_t *base, uint16_t size)
{
    buf->base   = base;
    buf->size   = size;
    buf->offset = 0;
}

/**
 * @brief 获取协议缓冲区的大小
 *
 * @param[in] buf 协议缓冲区
 * @return 缓冲区大小
 */
inline static uint16_t neu_protocol_buf_size(neu_protocol_buf_t *buf)
{
    return buf->size;
}

/**
 * @brief 创建新的协议解包缓冲区
 *
 * @param[in] size 缓冲区大小
 * @return 新创建的协议解包缓冲区指针
 */
inline static neu_protocol_unpack_buf_t *
neu_protocol_unpack_buf_new(uint16_t size)
{
    neu_protocol_unpack_buf_t *buf = (neu_protocol_unpack_buf_t *) calloc(
        1, sizeof(neu_protocol_unpack_buf_t));

    buf->base   = (uint8_t *) calloc(size, sizeof(uint8_t));
    buf->size   = size;
    buf->offset = 0;

    return buf;
}

/**
 * @brief 创建新的协议打包缓冲区
 *
 * @param[in] size 缓冲区大小
 * @return 新创建的协议打包缓冲区指针
 */
inline static neu_protocol_pack_buf_t *neu_protocol_pack_buf_new(uint16_t size)
{
    neu_protocol_pack_buf_t *buf =
        (neu_protocol_pack_buf_t *) calloc(1, sizeof(neu_protocol_pack_buf_t));

    buf->base   = (uint8_t *) calloc(size, sizeof(uint8_t));
    buf->size   = size;
    buf->offset = size;

    return buf;
}

/**
 * @brief 释放协议缓冲区内存
 *
 * @param[in] buf 需要释放的协议缓冲区
 */
inline static void neu_protocol_buf_free(neu_protocol_buf_t *buf)
{
    free(buf->base);
    free(buf);
}

/**
 * @brief 重置协议解包缓冲区
 *
 * @param[in] buf 需要重置的协议解包缓冲区
 */
inline static void neu_protocol_unpack_buf_reset(neu_protocol_buf_t *buf)
{
    buf->offset = 0;
    memset(buf->base, 0, buf->size);
}

/**
 * @brief 重置协议打包缓冲区
 *
 * @param[in] buf 需要重置的协议打包缓冲区
 */
inline static void neu_protocol_pack_buf_reset(neu_protocol_buf_t *buf)
{
    buf->offset = buf->size;
    memset(buf->base, 0, buf->size);
}

/**
 * @brief 从协议解包缓冲区读取数据并移动偏移量
 *
 * @param[in] buf 协议解包缓冲区
 * @param[in] size 需要读取的数据大小
 * @return 数据指针，如果剩余空间不足则返回NULL
 */
inline static uint8_t *neu_protocol_unpack_buf(neu_protocol_unpack_buf_t *buf,
                                               uint16_t                   size)
{
    if (buf->size - buf->offset < size) {
        return NULL;
    }

    buf->offset += size;

    return buf->base + buf->offset - size;
}

/**
 * @brief 恢复协议解包缓冲区的偏移量
 *
 * @param[in] buf 协议解包缓冲区
 * @param[in] size 需要回退的偏移量大小
 */
inline static void
neu_protocol_unpack_buf_revert(neu_protocol_unpack_buf_t *buf, uint16_t size)
{
    buf->offset -= size;
}

/**
 * @brief 获取协议解包缓冲区当前位置的数据，但不移动偏移量
 *
 * @param[in] buf 协议解包缓冲区
 * @param[in] size 需要获取的数据大小
 * @return 数据指针，如果剩余空间不足则返回NULL
 */
inline static uint8_t *
neu_protocol_unpack_buf_get(neu_protocol_unpack_buf_t *buf, uint16_t size)
{
    if (buf->size - buf->offset < size) {
        return NULL;
    }

    return buf->base + buf->offset;
}

/**
 * @brief 在协议打包缓冲区分配空间并移动偏移量
 *
 * @param[in] buf 协议打包缓冲区
 * @param[in] size 需要分配的空间大小
 * @return 分配空间的指针，如果空间不足则返回NULL
 */
inline static uint8_t *neu_protocol_pack_buf(neu_protocol_pack_buf_t *buf,
                                             uint16_t                 size)
{
    if (buf->offset < size) {
        return NULL;
    }

    buf->offset -= size;

    return buf->base + buf->offset;
}

/**
 * @brief 在协议打包缓冲区指定偏移位置设置数据
 *
 * @param[in] buf 协议打包缓冲区
 * @param[in] offset 相对当前位置的偏移量
 * @param[in] size 数据大小
 * @return 设置位置的指针，如果超出边界则返回NULL
 */
inline static uint8_t *neu_protocol_pack_buf_set(neu_protocol_pack_buf_t *buf,
                                                 uint16_t offset, uint16_t size)
{
    if (buf->offset + offset + size - 1 > buf->size) {
        return NULL;
    }

    return buf->base + buf->offset + offset;
}

/**
 * @brief 获取协议打包缓冲区当前位置的指针
 *
 * @param[in] buf 协议打包缓冲区
 * @return 当前位置的指针
 */
inline static uint8_t *neu_protocol_pack_buf_get(neu_protocol_pack_buf_t *buf)
{
    return buf->base + buf->offset;
}

/**
 * @brief 获取协议打包缓冲区未使用的空间大小
 *
 * @param[in] buf 协议打包缓冲区
 * @return 未使用的空间大小（字节）
 */
inline static uint16_t
neu_protocol_pack_buf_unused_size(neu_protocol_pack_buf_t *buf)
{
    return buf->offset;
}

/**
 * @brief 获取协议打包缓冲区已使用的空间大小
 *
 * @param[in] buf 协议打包缓冲区
 * @return 已使用的空间大小（字节）
 */
inline static uint16_t
neu_protocol_pack_buf_used_size(neu_protocol_pack_buf_t *buf)
{
    return buf->size - buf->offset;
}

/**
 * @brief 获取协议解包缓冲区未使用的空间大小
 *
 * @param[in] buf 协议解包缓冲区
 * @return 未使用的空间大小（字节）
 */
inline static uint16_t
neu_protocol_unpack_buf_unused_size(neu_protocol_unpack_buf_t *buf)
{
    return buf->size - buf->offset;
}

/**
 * @brief 获取协议解包缓冲区已使用的空间大小
 *
 * @param[in] buf 协议解包缓冲区
 * @return 已使用的空间大小（字节）
 */
inline static uint16_t
neu_protocol_unpack_buf_used_size(neu_protocol_unpack_buf_t *buf)
{
    return buf->offset;
}

/**
 * @brief 将协议解包缓冲区标记为全部已使用
 *
 * @param[in] buf 协议解包缓冲区
 */
inline static void
neu_protocol_unpack_buf_use_all(neu_protocol_unpack_buf_t *buf)
{
    buf->offset = buf->size;
}

/**
 * @brief 获取协议解包缓冲区的总大小
 *
 * @param[in] buf 协议解包缓冲区
 * @return 缓冲区总大小（字节）
 */
inline static uint16_t
neu_protocol_unpack_buf_size(neu_protocol_unpack_buf_t *buf)
{
    return buf->size;
}

#endif
