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

#ifndef _NEU_JSON_FN_H
#define _NEU_JSON_FN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef int (*neu_json_encode_fn)(void *object, void *param);

int neu_json_encode_by_fn(void *param, neu_json_encode_fn fn, char **result);

int neu_json_encode_with_mqtt(void *param, neu_json_encode_fn fn,
                              void *mqtt_param, neu_json_encode_fn mqtt_fn,
                              char **result);

typedef struct neu_parse_errors {
    int      n_error;
    int64_t *errors;
} neu_parse_errors_t;

typedef struct neu_parse_error {
    int64_t error;
} neu_parse_error_t;

int neu_parse_encode_error(void *json_object, void *param);

#ifdef __cplusplus
}
#endif

#endif