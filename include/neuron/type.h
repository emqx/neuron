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
 **/

#ifndef _NEU_TYPE_H_
#define _NEU_TYPE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    NEU_TYPE_INT8   = 1,
    NEU_TYPE_UINT8  = 2,
    NEU_TYPE_INT16  = 3,
    NEU_TYPE_UINT16 = 4,
    NEU_TYPE_INT32  = 5,
    NEU_TYPE_UINT32 = 6,
    NEU_TYPE_INT64  = 7,
    NEU_TYPE_UINT64 = 8,
    NEU_TYPE_FLOAT  = 9,
    NEU_TYPE_DOUBLE = 10,
    NEU_TYPE_BIT    = 11,
    NEU_TYPE_BOOL   = 12,
    NEU_TYPE_STRING = 13,
} neu_type_e;

typedef union {
    int8_t   i8;
    uint8_t  u8;
    int16_t  i16;
    uint16_t u16;
    int32_t  i32;
    uint32_t u32;
    int64_t  i64;
    uint64_t u64;
    float    f32;
    double   d64;
    char *   str;
} neu_value_u;

#ifdef __cplusplus
}
#endif

#endif
