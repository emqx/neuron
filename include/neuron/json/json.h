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

#ifndef _NEU_JSON__H_
#define _NEU_JSON__H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum neu_json_type {
    NEU_JSON_UNDEFINE = 0,
    NEU_JSON_INT      = 1,
    NEU_JSON_BIT,
    NEU_JSON_STR,
    NEU_JSON_DOUBLE,
    NEU_JSON_FLOAT,
    NEU_JSON_BOOL,
    NEU_JSON_OBJECT,
    NEU_JSON_ARRAY_INT8,
    NEU_JSON_ARRAY_UINT8,
    NEU_JSON_ARRAY_INT16,
    NEU_JSON_ARRAY_UINT16,
    NEU_JSON_ARRAY_INT32,
    NEU_JSON_ARRAY_UINT32,
    NEU_JSON_ARRAY_INT64,
    NEU_JSON_ARRAY_UINT64,
    NEU_JSON_ARRAY_FLOAT,
    NEU_JSON_ARRAY_DOUBLE,
    NEU_JSON_ARRAY_BOOL,
    NEU_JSON_VALUE = NEU_JSON_UNDEFINE
} neu_json_type_e;

typedef struct {
    int8_t *i8s;
    uint8_t length;
} neu_json_value_array_int8_t;

typedef struct {
    uint8_t *u8s;
    uint8_t  length;
} neu_json_value_array_uint8_t;

typedef struct {
    int16_t *i16s;
    uint8_t  length;
} neu_json_value_array_int16_t;

typedef struct {
    uint16_t *u16s;
    uint8_t   length;
} neu_json_value_array_uint16_t;

typedef struct {
    int32_t *i32s;
    uint8_t  length;
} neu_json_value_array_int32_t;

typedef struct {
    uint32_t *u32s;
    uint8_t   length;
} neu_json_value_array_uint32_t;

typedef struct {
    int64_t *i64s;
    uint8_t  length;
} neu_json_value_array_int64_t;

typedef struct {
    uint64_t *u64s;
    uint8_t   length;
} neu_json_value_array_uint64_t;

typedef struct {
    float * f32s;
    uint8_t length;
} neu_json_value_array_float_t;

typedef struct {
    double *f64s;
    uint8_t length;
} neu_json_value_array_double_t;

typedef struct {
    bool *  bools;
    uint8_t length;
} neu_json_value_array_bool_t;

typedef union neu_json_value {
    int64_t                       val_int;
    uint8_t                       val_bit;
    float                         val_float;
    double                        val_double;
    bool                          val_bool;
    char *                        val_str;
    void *                        val_object;
    neu_json_value_array_int8_t   val_array_int8;
    neu_json_value_array_uint8_t  val_array_uint8;
    neu_json_value_array_int16_t  val_array_int16;
    neu_json_value_array_uint16_t val_array_uint16;
    neu_json_value_array_int32_t  val_array_int32;
    neu_json_value_array_uint32_t val_array_uint32;
    neu_json_value_array_int64_t  val_array_int64;
    neu_json_value_array_uint64_t val_array_uint64;
    neu_json_value_array_float_t  val_array_float;
    neu_json_value_array_double_t val_array_double;
    neu_json_value_array_bool_t   val_array_bool;
} neu_json_value_u;

typedef enum neu_json_attribute {
    NEU_JSON_ATTRIBUTE_REQUIRED,
    NEU_JSON_ATTRIBUTE_OPTIONAL,
} neu_json_attribute_t;

typedef struct neu_json_elem {
    neu_json_attribute_t attribute;
    char *               name;
    char *               op_name;
    enum neu_json_type   t;
    union neu_json_value v;
    uint8_t              precision;
    double               bias;
} neu_json_elem_t;

#define NEU_JSON_ELEM_SIZE(elems) sizeof(elems) / sizeof(neu_json_elem_t)

/* New a empty josn array */
void *neu_json_array();

int   neu_json_decode_by_json(void *json, int size, neu_json_elem_t *ele);
int   neu_json_decode(char *buf, int size, neu_json_elem_t *ele);
int   neu_json_decode_array_size_by_json(void *json, char *child);
int   neu_json_decode_array_elem(void *json, int index, int size,
                                 neu_json_elem_t *ele);
int   neu_json_decode_array_by_json(void *json, char *name, int index, int size,
                                    neu_json_elem_t *ele);
int   neu_json_decode_array_size(char *buf, char *child);
int   neu_json_decode_array(char *buf, char *name, int index, int size,
                            neu_json_elem_t *ele);
void *neu_json_decode_new(const char *buf);
void *neu_json_decode_newb(char *buf, size_t len);
void  neu_json_decode_free(void *ob);
int   neu_json_decode_value(void *object, neu_json_elem_t *ele);

void *neu_json_encode_new();
void  neu_json_encode_free(void *json_object);
void *neu_json_encode_array(void *array, neu_json_elem_t *t, int n);
void *neu_json_encode_array_value(void *array, neu_json_elem_t *t, int n);
int   neu_json_encode_field(void *json_object, neu_json_elem_t *elem, int n);
int   neu_json_encode(void *json_object, char **str);

int neu_json_dump_key(void *object, const char *key, char **const result,
                      bool must_exist);
int neu_json_load_key(void *object, const char *key, const char *input,
                      bool must_exist);

#ifdef __cplusplus
}
#endif

#endif
