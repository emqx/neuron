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
 * @brief Initialization of protocol_pack_buf.
 *
 * @param[in] buf neu_protocol_pack_buf_t to be initialized.
 * @param[in] base Memory used in protocol_pack_buf.
 * @param[in] size Size of base memory.
 */
inline static void neu_protocol_pack_buf_init(neu_protocol_pack_buf_t *buf,
                                              uint8_t *base, uint16_t size)
{
    buf->base   = base;
    buf->size   = size;
    buf->offset = size;
}

/**
 * @brief Initialization of protocol_unpack_buf.
 *
 * @param[in] buf neu_protocol_unpack_buf_t to be initialized.
 * @param[in] base Memory used in protocol_unpack_buf.
 * @param[in] size Size of base memory.
 */
inline static void neu_protocol_unpack_buf_init(neu_protocol_unpack_buf_t *buf,
                                                uint8_t *base, uint16_t size)
{
    buf->base   = base;
    buf->size   = size;
    buf->offset = 0;
}

inline static uint16_t neu_protocol_buf_size(neu_protocol_buf_t *buf)
{
    return buf->size;
}

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

inline static neu_protocol_pack_buf_t *neu_protocol_pack_buf_new(uint16_t size)
{
    neu_protocol_pack_buf_t *buf =
        (neu_protocol_pack_buf_t *) calloc(1, sizeof(neu_protocol_pack_buf_t));

    buf->base   = (uint8_t *) calloc(size, sizeof(uint8_t));
    buf->size   = size;
    buf->offset = size;

    return buf;
}

inline static void neu_protocol_buf_free(neu_protocol_buf_t *buf)
{
    free(buf->base);
    free(buf);
}

inline static void neu_protocol_unpack_buf_reset(neu_protocol_buf_t *buf)
{
    buf->offset = 0;
    memset(buf->base, 0, buf->size);
}

inline static void neu_protocol_pack_buf_reset(neu_protocol_buf_t *buf)
{
    buf->offset = buf->size;
    memset(buf->base, 0, buf->size);
}

inline static uint8_t *neu_protocol_unpack_buf(neu_protocol_unpack_buf_t *buf,
                                               uint16_t                   size)
{
    if (buf->size - buf->offset < size) {
        return NULL;
    }

    buf->offset += size;

    return buf->base + buf->offset - size;
}

inline static void
neu_protocol_unpack_buf_revert(neu_protocol_unpack_buf_t *buf, uint16_t size)
{
    buf->offset -= size;
}

inline static uint8_t *
neu_protocol_unpack_buf_get(neu_protocol_unpack_buf_t *buf, uint16_t size)
{
    if (buf->size - buf->offset < size) {
        return NULL;
    }

    return buf->base + buf->offset;
}

inline static uint8_t *neu_protocol_pack_buf(neu_protocol_pack_buf_t *buf,
                                             uint16_t                 size)
{
    if (buf->offset < size) {
        return NULL;
    }

    buf->offset -= size;

    return buf->base + buf->offset;
}

inline static uint8_t *neu_protocol_pack_buf_set(neu_protocol_pack_buf_t *buf,
                                                 uint16_t offset, uint16_t size)
{
    if (buf->offset + offset + size - 1 > buf->size) {
        return NULL;
    }

    return buf->base + buf->offset + offset;
}

inline static uint8_t *neu_protocol_pack_buf_get(neu_protocol_pack_buf_t *buf)
{
    return buf->base + buf->offset;
}

inline static uint16_t
neu_protocol_pack_buf_unused_size(neu_protocol_pack_buf_t *buf)
{
    return buf->offset;
}

inline static uint16_t
neu_protocol_pack_buf_used_size(neu_protocol_pack_buf_t *buf)
{
    return buf->size - buf->offset;
}

inline static uint16_t
neu_protocol_unpack_buf_unused_size(neu_protocol_unpack_buf_t *buf)
{
    return buf->size - buf->offset;
}

inline static uint16_t
neu_protocol_unpack_buf_used_size(neu_protocol_unpack_buf_t *buf)
{
    return buf->offset;
}

inline static void
neu_protocol_unpack_buf_use_all(neu_protocol_unpack_buf_t *buf)
{
    buf->offset = buf->size;
}

inline static uint16_t
neu_protocol_unpack_buf_size(neu_protocol_unpack_buf_t *buf)
{
    return buf->size;
}

#endif
