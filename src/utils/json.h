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

#include <stdbool.h>
#include <stdint.h>

enum neu_json_type {
    NEU_JSON_INT = 0,
    NEU_JSON_STR,
    NEU_JSON_DOUBLE,
    NEU_JSON_BOOL,
    NEU_JSON_OBJECT
};

union neu_json_value {
    int64_t val_int;
    char *  val_str;
    double  val_double;
    bool    val_bool;
    void *  object;
};

typedef struct neu_json_elem {
    char *               name;
    enum neu_json_type   t;
    union neu_json_value v;
} neu_json_elem_t;

int neu_json_decode(char *buf, int size, neu_json_elem_t *t, ...);

typedef struct neu_json_ctx neu_json_ctx_t;

int             neu_json_decode_array_size(char *buf, char *child);
void            neu_json_decode_ctx_free(neu_json_ctx_t *t);
neu_json_ctx_t *neu_json_decode_object_get(char *buf, char *child);
neu_json_ctx_t *neu_json_decode_object_get_next(neu_json_ctx_t *iter,
                                                char *          child);
neu_json_ctx_t *neu_json_decode_array_next(neu_json_ctx_t *iter, int size,
                                           neu_json_elem_t *t, ...);

void *neu_json_encode_array(neu_json_elem_t **t, int n_x, int n_y);
int   neu_json_encode(neu_json_elem_t *t, int n, char **str);
#endif
