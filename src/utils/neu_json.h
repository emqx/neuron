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

#ifndef _NEU_NEU_JSON__H_
#define _NEU_NEU_JSON__H_

#ifdef __cplusplus
extern "C" {
#endif

#include "neu_data_expr.h"

int neu_jsonx_decode(char *buf, neu_data_val_t *val);
int neu_jsonx_decode_array_size(char *buf, char *child);
int neu_jsonx_decode_array(char *buf, char *name, int index,
                           neu_data_val_t *val);

void *neu_jsonx_new();
void *neu_jsonx_encode_array(void *array, neu_data_val_t *val);
void *neu_jsonx_encode_array_value(void *array, neu_data_val_t *val);
int   neu_jsonx_encode_field(void *json_object, neu_fixed_array_t *kv);
int   neu_jsonx_encode(void *object, neu_data_val_t *val, char **result);

#ifdef __cplusplus
}
#endif

#endif
