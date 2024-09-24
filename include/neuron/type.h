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

#ifndef _NEU_TYPE_H_
#define _NEU_TYPE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <jansson.h>

typedef enum {
    NEU_TYPE_INT8          = 1,
    NEU_TYPE_UINT8         = 2,
    NEU_TYPE_INT16         = 3,
    NEU_TYPE_UINT16        = 4,
    NEU_TYPE_INT32         = 5,
    NEU_TYPE_UINT32        = 6,
    NEU_TYPE_INT64         = 7,
    NEU_TYPE_UINT64        = 8,
    NEU_TYPE_FLOAT         = 9,
    NEU_TYPE_DOUBLE        = 10,
    NEU_TYPE_BIT           = 11,
    NEU_TYPE_BOOL          = 12,
    NEU_TYPE_STRING        = 13,
    NEU_TYPE_BYTES         = 14,
    NEU_TYPE_ERROR         = 15,
    NEU_TYPE_WORD          = 16,
    NEU_TYPE_DWORD         = 17,
    NEU_TYPE_LWORD         = 18,
    NEU_TYPE_PTR           = 19,
    NEU_TYPE_TIME          = 20,
    NEU_TYPE_DATA_AND_TIME = 21,
    NEU_TYPE_CUSTOM        = 22,
} neu_type_e;

inline static const char *neu_type_string(neu_type_e type)
{
    switch (type) {
    case NEU_TYPE_INT8:
        return "NEU_TYPE_INT8";
    case NEU_TYPE_UINT8:
        return "NEU_TYPE_UINT8";
    case NEU_TYPE_INT16:
        return "NEU_TYPE_INT16";
    case NEU_TYPE_UINT16:
        return "NEU_TYPE_UINT16";
    case NEU_TYPE_INT32:
        return "NEU_TYPE_INT32";
    case NEU_TYPE_UINT32:
        return "NEU_TYPE_UINT32";
    case NEU_TYPE_INT64:
        return "NEU_TYPE_INT64";
    case NEU_TYPE_UINT64:
        return "NEU_TYPE_UINT64";
    case NEU_TYPE_FLOAT:
        return "NEU_TYPE_FLOAT";
    case NEU_TYPE_DOUBLE:
        return "NEU_TYPE_DOUBLE";
    case NEU_TYPE_BIT:
        return "NEU_TYPE_BIT";
    case NEU_TYPE_BOOL:
        return "NEU_TYPE_BOOL";
    case NEU_TYPE_STRING:
        return "NEU_TYPE_STRING";
    case NEU_TYPE_BYTES:
        return "NEU_TYPE_BYTES";
    case NEU_TYPE_ERROR:
        return "NEU_TYPE_ERROR";
    case NEU_TYPE_WORD:
        return "NEU_TYPE_WORD";
    case NEU_TYPE_DWORD:
        return "NEU_TYPE_DWORD";
    case NEU_TYPE_LWORD:
        return "NEU_TYPE_LWORD";
    case NEU_TYPE_PTR:
        return "NEU_TYPE_PTR";
    case NEU_TYPE_TIME:
        return "NEU_TYPE_TIME";
    case NEU_TYPE_DATA_AND_TIME:
        return "NEU_TYPE_DATA_AND_TIME";
    case NEU_TYPE_CUSTOM:
        return "NEU_TYPE_CUSTOM";
    }

    return "";
}

typedef struct {
    neu_type_e type; // string or bytes
    uint16_t   length;
    uint8_t *  ptr;
} neu_value_ptr_t;

#define NEU_VALUE_SIZE 128

typedef struct {
    uint8_t bytes[NEU_VALUE_SIZE];
    uint8_t length;
} __attribute__((packed)) neu_value_bytes_t;

typedef union {
    bool              boolean;
    int8_t            i8;
    uint8_t           u8;
    int16_t           i16;
    uint16_t          u16;
    int32_t           i32;
    uint32_t          u32;
    int64_t           i64;
    uint64_t          u64;
    float             f32;
    double            d64;
    char              str[NEU_VALUE_SIZE];
    neu_value_bytes_t bytes;
    neu_value_ptr_t   ptr;
    json_t *          json;
} neu_value_u;

static inline char *neu_value_str(neu_type_e type, neu_value_u value)
{
    static __thread char str[128] = { 0 };

    switch (type) {
    case NEU_TYPE_INT8:
        snprintf(str, sizeof(str), "type: %s, value: %i", neu_type_string(type),
                 value.i8);
        break;
    case NEU_TYPE_UINT8:
        snprintf(str, sizeof(str), "type: %s, value: %u", neu_type_string(type),
                 value.u8);
        break;
    case NEU_TYPE_INT16:
        snprintf(str, sizeof(str), "type: %s, value: %i", neu_type_string(type),
                 value.i16);
        break;
    case NEU_TYPE_WORD:
    case NEU_TYPE_UINT16:
        snprintf(str, sizeof(str), "type: %s, value: %u", neu_type_string(type),
                 value.u16);
        break;
    case NEU_TYPE_INT32:
        snprintf(str, sizeof(str), "type: %s, value: %i", neu_type_string(type),
                 value.i32);
        break;
    case NEU_TYPE_DWORD:
    case NEU_TYPE_UINT32:
        snprintf(str, sizeof(str), "type: %s, value: %u", neu_type_string(type),
                 value.u32);
        break;
    case NEU_TYPE_INT64: {
        long long int i = value.i64;
        snprintf(str, sizeof(str), "type: %s, value: %lld",
                 neu_type_string(type), i);
        break;
    }
    case NEU_TYPE_LWORD:
    case NEU_TYPE_UINT64: {
        long long unsigned int i = value.u64;
        snprintf(str, sizeof(str), "type: %s, value: %llu",
                 neu_type_string(type), i);
        break;
    }
    case NEU_TYPE_FLOAT:
        snprintf(str, sizeof(str), "type: %s, value: %f", neu_type_string(type),
                 value.f32);
        break;
    case NEU_TYPE_DOUBLE:
        snprintf(str, sizeof(str), "type: %s, value: %f", neu_type_string(type),
                 value.d64);
        break;
    case NEU_TYPE_BIT:
        snprintf(str, sizeof(str), "type: %s, value: %u", neu_type_string(type),
                 value.u8);
        break;
    case NEU_TYPE_BOOL:
        snprintf(str, sizeof(str), "type: %s, value: %u", neu_type_string(type),
                 value.boolean);
        break;
    case NEU_TYPE_STRING:
    case NEU_TYPE_TIME:
    case NEU_TYPE_DATA_AND_TIME: {
        snprintf(str, sizeof(str), "type: %s, value: %c%c%c",
                 neu_type_string(type), value.str[0], value.str[1],
                 value.str[2]);
        break;
    }
    case NEU_TYPE_CUSTOM: {
        char *result = json_dumps(value.json, JSON_REAL_PRECISION(16));
        snprintf(str, sizeof(str), "type: %s, value: %s", neu_type_string(type),
                 result);
        break;
    }
    default:
        break;
    }

    return str;
}

typedef struct {
    neu_type_e  type;
    neu_value_u value;
    uint8_t     precision;
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

static inline uint16_t neu_get_u16(uint8_t *bytes)
{
    uint16_t ret = 0;
    uint8_t *t   = NULL;

    t = (uint8_t *) &ret;

    t[0] = bytes[0];
    t[1] = bytes[1];

    return ret;
}

static inline void neu_set_u16(uint8_t *bytes, uint16_t val)
{
    uint8_t *t = NULL;

    t = (uint8_t *) &val;

    bytes[0] = t[0];
    bytes[1] = t[1];
}

static inline int16_t neu_get_i16(uint8_t *bytes)
{
    int16_t  ret = 0;
    uint8_t *t   = NULL;

    t = (uint8_t *) &ret;

    t[0] = bytes[0];
    t[1] = bytes[1];

    return ret;
}

static inline void neu_set_i16(uint8_t *bytes, int16_t val)
{
    uint8_t *t = NULL;

    t = (uint8_t *) &val;

    bytes[0] = t[0];
    bytes[1] = t[1];
}

static inline uint32_t neu_get_u32(uint8_t *bytes)
{
    uint32_t ret = 0;
    uint8_t *t   = NULL;

    t = (uint8_t *) &ret;

    t[0] = bytes[0];
    t[1] = bytes[1];
    t[2] = bytes[2];
    t[3] = bytes[3];

    return ret;
}

static inline void neu_set_u32(uint8_t *bytes, uint32_t val)
{
    uint8_t *t = NULL;

    t = (uint8_t *) &val;

    bytes[0] = t[0];
    bytes[1] = t[1];
    bytes[2] = t[2];
    bytes[3] = t[3];
}

static inline int32_t neu_get_i32(uint8_t *bytes)
{
    int32_t  ret = 0;
    uint8_t *t   = NULL;

    t = (uint8_t *) &ret;

    t[0] = bytes[0];
    t[1] = bytes[1];
    t[2] = bytes[2];
    t[3] = bytes[3];

    return ret;
}

static inline void neu_set_i32(uint8_t *bytes, int32_t val)
{
    uint8_t *t = NULL;

    t = (uint8_t *) &val;

    bytes[0] = t[0];
    bytes[1] = t[1];
    bytes[2] = t[2];
    bytes[3] = t[3];
}

static inline float neu_get_f32(uint8_t *bytes)
{
    float    ret = 0;
    uint8_t *t   = NULL;

    t = (uint8_t *) &ret;

    t[0] = bytes[0];
    t[1] = bytes[1];
    t[2] = bytes[2];
    t[3] = bytes[3];

    return ret;
}

static inline void neu_set_f32(uint8_t *bytes, float val)
{
    uint8_t *t = NULL;

    t = (uint8_t *) &val;

    bytes[0] = t[0];
    bytes[1] = t[1];
    bytes[2] = t[2];
    bytes[3] = t[3];
}

static inline uint64_t neu_get_u64(uint8_t *bytes)
{
    uint64_t ret = 0;
    uint8_t *t   = NULL;

    t = (uint8_t *) &ret;

    t[0] = bytes[0];
    t[1] = bytes[1];
    t[2] = bytes[2];
    t[3] = bytes[3];
    t[4] = bytes[4];
    t[5] = bytes[5];
    t[6] = bytes[6];
    t[7] = bytes[7];

    return ret;
}

static inline void neu_set_u64(uint8_t *bytes, uint64_t val)
{
    uint8_t *t = NULL;

    t = (uint8_t *) &val;

    bytes[0] = t[0];
    bytes[1] = t[1];
    bytes[2] = t[2];
    bytes[3] = t[3];
    bytes[4] = t[4];
    bytes[5] = t[5];
    bytes[6] = t[6];
    bytes[7] = t[7];
}

static inline int64_t neu_get_i64(uint8_t *bytes)
{
    int64_t  ret = 0;
    uint8_t *t   = NULL;

    t = (uint8_t *) &ret;

    t[0] = bytes[0];
    t[1] = bytes[1];
    t[2] = bytes[2];
    t[3] = bytes[3];
    t[4] = bytes[4];
    t[5] = bytes[5];
    t[6] = bytes[6];
    t[7] = bytes[7];

    return ret;
}

static inline void neu_set_i64(uint8_t *bytes, int64_t val)
{
    uint8_t *t = NULL;

    t = (uint8_t *) &val;

    bytes[0] = t[0];
    bytes[1] = t[1];
    bytes[2] = t[2];
    bytes[3] = t[3];
    bytes[4] = t[4];
    bytes[5] = t[5];
    bytes[6] = t[6];
    bytes[7] = t[7];
}

static inline double neu_get_f64(uint8_t *bytes)
{
    double   ret = 0;
    uint8_t *t   = NULL;

    t = (uint8_t *) &ret;

    t[0] = bytes[0];
    t[1] = bytes[1];
    t[2] = bytes[2];
    t[3] = bytes[3];
    t[4] = bytes[4];
    t[5] = bytes[5];
    t[6] = bytes[6];
    t[7] = bytes[7];

    return ret;
}

static inline void neu_set_f64(uint8_t *bytes, double val)
{
    uint8_t *t = NULL;

    t = (uint8_t *) &val;

    bytes[0] = t[0];
    bytes[1] = t[1];
    bytes[2] = t[2];
    bytes[3] = t[3];
    bytes[4] = t[4];
    bytes[5] = t[5];
    bytes[6] = t[6];
    bytes[7] = t[7];
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

    ret_bytes[0] = bytes[7];
    ret_bytes[1] = bytes[6];
    ret_bytes[2] = bytes[5];
    ret_bytes[3] = bytes[4];
    ret_bytes[4] = bytes[3];
    ret_bytes[5] = bytes[2];
    ret_bytes[6] = bytes[1];
    ret_bytes[7] = bytes[0];

    return ret;
}

static inline uint64_t neu_ntohll(uint64_t u)
{
    return neu_htonll(u);
}

static inline void neu_ntonll_p(uint64_t *pu)
{
    uint64_t ret = neu_htonll(neu_get_u64((uint8_t *) pu));
    neu_set_u64((uint8_t *) pu, ret);
}

static inline void neu_htonll_p(uint64_t *pu)
{
    neu_ntonll_p(pu);
}

static inline void neu_ntohl_p(uint32_t *pu)
{
    uint32_t ret = ntohl(neu_get_u32((uint8_t *) pu));
    neu_set_u32((uint8_t *) pu, ret);
}

static inline void neu_htonl_p(uint32_t *pu)
{
    neu_ntohl_p(pu);
}

static inline void neu_ntohs_p(uint16_t *pu)
{
    uint16_t ret = ntohs(neu_get_u16((uint8_t *) pu));
    neu_set_u16((uint8_t *) pu, ret);
}

static inline void neu_htons_p(uint16_t *pu)
{
    neu_ntohs_p(pu);
}

#ifdef __cplusplus
}
#endif

#endif
