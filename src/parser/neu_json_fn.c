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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "utils/json.h"

#include "neu_json_fn.h"
#include "neu_json_param.h"

int neu_json_encode_by_fn(void *param, neu_json_encode_fn fn, char **result)
{
    void *object = neu_json_encode_new();

    if (fn != NULL) {
        if (fn(object, param) != 0) {
            neu_json_encode_free(object);
            return -1;
        }
    }

    return neu_json_encode(object, result);
}

int neu_json_encode_with_mqtt(void *param, neu_json_encode_fn fn,
                              void *mqtt_param, neu_json_encode_fn mqtt_fn,
                              char **result)
{
    void *object = neu_json_encode_new();

    if (mqtt_fn != NULL) {
        if (mqtt_fn(object, mqtt_param) != 0) {
            neu_json_encode_free(object);
            return -1;
        }
    }

    if (fn != NULL) {
        if (fn(object, param) != 0) {
            neu_json_encode_free(object);
            return -1;
        }
    }

    return neu_json_encode(object, result);
}

int neu_parse_encode_error(void *json_object, void *param)
{
    neu_parse_error_t *res = (neu_parse_error_t *) param;

    neu_json_elem_t elems[] = {
        {
            .name      = "error",
            .t         = NEU_JSON_INT,
            .v.val_int = res->error,
        },
    };

    return neu_json_encode_field(json_object, elems, 1);
}

int neu_parse_param(char *buf, char **err_param, int n, neu_json_elem_t *ele,
                    ...)
{
    void *          json       = neu_json_decode_new(buf);
    neu_json_elem_t params_ele = { .name = "params", .t = NEU_JSON_OBJECT };
    va_list         ap;
    int             ret = 0;

    if (neu_json_decode_value(json, &params_ele) != 0) {
        neu_json_decode_free(json);
        *err_param = strdup("params");
        return -1;
    }

    va_start(ap, ele);
    for (int i = 0; i < n; i++) {
        neu_json_elem_t *tmp_ele = va_arg(ap, neu_json_elem_t *);
        if (neu_json_decode_value(params_ele.v.object, tmp_ele) != 0) {
            *err_param = strdup(tmp_ele->name);
            ret        = -1;
            break;
        }
    }

    va_end(ap);

    neu_json_decode_free(json);
    return ret;
}