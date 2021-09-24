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

#ifndef _NEU_JSON__H_
#define _NEU_JSON__H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum neu_json_type {
    NEU_JSON_UNDEFINE = 0,
    NEU_JSON_INT      = 1,
    NEU_JSON_BIT,
    NEU_JSON_STR,
    NEU_JSON_DOUBLE,
    NEU_JSON_FLOAT,
    NEU_JSON_BOOL,
    NEU_JSON_OBJECT
} neu_json_type_e;

typedef union neu_json_value {
    int64_t val_int;
    uint8_t val_bit;
    float   val_float;
    double  val_double;
    bool    val_bool;
    char *  val_str;
    void *  object;
} neu_json_value_u;

typedef struct neu_json_elem {
    char *               name;
    enum neu_json_type   t;
    union neu_json_value v;
} neu_json_elem_t;

#define NEU_JSON_ELEM_SIZE(elems) sizeof(elems) / sizeof(neu_json_elem_t)

int neu_json_decode(char *buf, int size, neu_json_elem_t *ele);
int neu_json_decode_array_size(char *buf, char *child);
int neu_json_decode_array(char *buf, char *name, int index, int size,
                          neu_json_elem_t *ele);

void *neu_json_encode_new();
void  neu_json_encode_free(void *json_object);
void *neu_json_encode_array(void *array, neu_json_elem_t *t, int n);
void *neu_json_encode_array_value(void *array, neu_json_elem_t *t, int n);
int   neu_json_encode_field(void *json_object, neu_json_elem_t *elem, int n);
int   neu_json_encode(void *json_object, char **str);

#ifdef __cplusplus
}
#endif

#endif
