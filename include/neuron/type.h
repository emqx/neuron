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

#include <assert.h>
#include <netinet/in.h>
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
    NEU_TYPE_BYTES  = 14,
    NEU_TYPE_ERROR  = 15,
} neu_type_e;

#define NEU_VALUE_SIZE 128
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
    char     str[NEU_VALUE_SIZE];
    uint8_t  bytes[NEU_VALUE_SIZE];
} neu_value_u;

typedef struct {
    neu_type_e  type;
    neu_value_u value;
} neu_dvalue_t;
typedef union neu_value8 {
    uint8_t value;
    struct {
        uint8_t b0 : 1;
        uint8_t b1 : 1;
        uint8_t b2 : 1;
        uint8_t b3 : 1;
        uint8_t b4 : 1;
        uint8_t b5 : 1;
        uint8_t b6 : 1;
        uint8_t b7 : 1;
    } __attribute__((packed)) vb;
} neu_value8_u;

typedef union neu_value16 {
    uint16_t value;
    struct {
        uint8_t b0 : 1;
        uint8_t b1 : 1;
        uint8_t b2 : 1;
        uint8_t b3 : 1;
        uint8_t b4 : 1;
        uint8_t b5 : 1;
        uint8_t b6 : 1;
        uint8_t b7 : 1;
        uint8_t b8 : 1;
        uint8_t b9 : 1;
        uint8_t b10 : 1;
        uint8_t b11 : 1;
        uint8_t b12 : 1;
        uint8_t b13 : 1;
        uint8_t b14 : 1;
        uint8_t b15 : 1;
    } __attribute__((packed)) vb;
} neu_value16_u;

typedef union neu_value24 {
    uint8_t value[3];
} neu_value24_u;

typedef union neu_value32 {
    uint32_t vint;
    float    vfloat;
} neu_value32_u;

typedef union neu_value64 {
    uint64_t vint;
    double   vdouble;
} neu_value64_u;

static inline void neu_value24_set(union neu_value24 *v24, uint32_t v)
{
    assert((v & 0xff000000) == 0);

    v24->value[0] = v & 0xff;
    v24->value[1] = (v >> 8) & 0xff;
    v24->value[2] = (v >> 16) & 0xff;
}

static inline uint32_t neu_value24_get(union neu_value24 v24)
{
    uint32_t ret = 0;

    ret |= v24.value[0];
    ret |= v24.value[1] << 8;
    ret |= v24.value[2] << 16;

    return ret;
}

static inline uint8_t neu_value8_get_bit(neu_value8_u value, uint8_t index)
{
    uint8_t ret = 0;

    switch (index) {
    case 0:
        ret = value.vb.b0;
        break;
    case 1:
        ret = value.vb.b1;
        break;
    case 2:
        ret = value.vb.b2;
        break;
    case 3:
        ret = value.vb.b3;
        break;
    case 4:
        ret = value.vb.b4;
        break;
    case 5:
        ret = value.vb.b5;
        break;
    case 6:
        ret = value.vb.b6;
        break;
    case 7:
        ret = value.vb.b7;
        break;
    default:
        assert(1 == 0);
        break;
    }

    return ret;
}

static inline void neu_value8_set_bit(neu_value8_u *value, uint8_t index,
                                      uint8_t v)
{
    switch (index) {
    case 0:
        value->vb.b0 = v;
        break;
    case 1:
        value->vb.b1 = v;
        break;
    case 2:
        value->vb.b2 = v;
        break;
    case 3:
        value->vb.b3 = v;
        break;
    case 4:
        value->vb.b4 = v;
        break;
    case 5:
        value->vb.b5 = v;
        break;
    case 6:
        value->vb.b6 = v;
        break;
    case 7:
        value->vb.b7 = v;
        break;
    default:
        assert(1 == 0);
        break;
    }
}

static inline uint8_t neu_value16_get_bit(neu_value16_u value, uint8_t index)
{
    uint8_t ret = 0;

    switch (index) {
    case 0:
        ret = value.vb.b0;
        break;
    case 1:
        ret = value.vb.b1;
        break;
    case 2:
        ret = value.vb.b2;
        break;
    case 3:
        ret = value.vb.b3;
        break;
    case 4:
        ret = value.vb.b4;
        break;
    case 5:
        ret = value.vb.b5;
        break;
    case 6:
        ret = value.vb.b6;
        break;
    case 7:
        ret = value.vb.b7;
        break;
    case 8:
        ret = value.vb.b8;
        break;
    case 9:
        ret = value.vb.b9;
        break;
    case 10:
        ret = value.vb.b10;
        break;
    case 11:
        ret = value.vb.b11;
        break;
    case 12:
        ret = value.vb.b12;
        break;
    case 13:
        ret = value.vb.b13;
        break;
    case 14:
        ret = value.vb.b14;
        break;
    case 15:
        ret = value.vb.b15;
        break;
    default:
        assert(1 == 0);
        break;
    }

    return ret;
}

static inline void neu_value16_set_bit(neu_value16_u *value, uint8_t index,
                                       uint8_t v)
{
    switch (index) {
    case 0:
        value->vb.b0 = v;
        break;
    case 1:
        value->vb.b1 = v;
        break;
    case 2:
        value->vb.b2 = v;
        break;
    case 3:
        value->vb.b3 = v;
        break;
    case 4:
        value->vb.b4 = v;
        break;
    case 5:
        value->vb.b5 = v;
        break;
    case 6:
        value->vb.b6 = v;
        break;
    case 7:
        value->vb.b7 = v;
        break;
    case 8:
        value->vb.b8 = v;
        break;
    case 9:
        value->vb.b9 = v;
        break;
    case 10:
        value->vb.b10 = v;
        break;
    case 11:
        value->vb.b11 = v;
        break;
    case 12:
        value->vb.b12 = v;
        break;
    case 13:
        value->vb.b13 = v;
        break;
    case 14:
        value->vb.b14 = v;
        break;
    case 15:
        value->vb.b15 = v;
        break;
    default:
        assert(1 == 0);
        break;
    }
}

static inline void neu_ntohs24_p(uint8_t *value)
{
    uint8_t t = 0;

    t        = value[0];
    value[0] = value[2];
    value[2] = t;
}

static inline void neu_htons24_p(uint8_t *value)
{
    neu_ntohs24_p(value);
}

static inline uint64_t neu_htonll(uint64_t u)
{
    uint8_t *bytes     = (uint8_t *) &u;
    uint64_t ret       = 0;
    uint8_t *ret_bytes = (uint8_t *) &ret;

    ret_bytes[0] = bytes[3];
    ret_bytes[1] = bytes[2];
    ret_bytes[2] = bytes[1];
    ret_bytes[3] = bytes[0];

    return ret;
}

static inline uint64_t neu_ntohll(uint64_t u)
{
    return neu_htonll(u);
}

static inline void neu_ntonll_p(uint64_t *pu)
{
    *pu = neu_htonll(*pu);
}

static inline void neu_htonll_p(uint64_t *pu)
{
    neu_ntonll_p(pu);
}

static inline void neu_ntohl_p(uint32_t *pu)
{
    *pu = ntohl(*pu);
}

static inline void neu_htonl_p(uint32_t *pu)
{
    neu_ntohl_p(pu);
}

static inline void neu_ntohs_p(uint16_t *pu)
{
    *pu = ntohs(*pu);
}

static inline void neu_htons_p(uint16_t *pu)
{
    neu_ntohs_p(pu);
}

#ifdef __cplusplus
}
#endif

#endif
