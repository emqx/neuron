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

#include "json/json.h"

#include "json/neu_json_fn.h"
#include "json/neu_json_param.h"
#include "json/neu_json_rw.h"

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
        neu_json_decode_free(json);
        return -1;
    }

    if (neu_json_decode_value(params_ele.v.val_object, ele) != 0) {
        *err_param = strdup(ele->name);
        neu_json_decode_free(json);
        return -1;
    }

    va_start(ap, ele);
    for (int i = 1; i < n; i++) {
        neu_json_elem_t *tmp_ele = va_arg(ap, neu_json_elem_t *);
        if (neu_json_decode_value(params_ele.v.val_object, tmp_ele) != 0) {
            *err_param = strdup(tmp_ele->name);
            ret        = -1;
            break;
        }
    }

    va_end(ap);

    neu_json_decode_free(json);
    return ret;
}

neu_data_val_t *neu_parse_write_req_to_val(neu_parse_write_req_t *req)
{
    neu_data_val_t *   val = neu_dvalue_unit_new();
    neu_fixed_array_t *array =
        neu_fixed_array_new(req->n_tag, sizeof(neu_int_val_t));

    for (int i = 0; i < req->n_tag; i++) {
        neu_int_val_t   iv;
        neu_data_val_t *v;

        switch (req->tags[i].t) {
        case NEU_JSON_INT:
            v = neu_dvalue_new(NEU_DTYPE_INT64);
            neu_dvalue_set_int64(v, req->tags[i].value.val_int);
            break;
        case NEU_JSON_BIT:
            v = neu_dvalue_new(NEU_DTYPE_BIT);
            neu_dvalue_set_bit(v, req->tags[i].value.val_bit);
            break;
        case NEU_JSON_STR:
            v = neu_dvalue_new(NEU_DTYPE_CSTR);
            neu_dvalue_set_cstr(v, req->tags[i].value.val_str);
            break;
        case NEU_JSON_DOUBLE:
            v = neu_dvalue_new(NEU_DTYPE_DOUBLE);
            neu_dvalue_set_double(v, req->tags[i].value.val_double);
            break;
        case NEU_JSON_FLOAT:
            v = neu_dvalue_new(NEU_DTYPE_FLOAT);
            neu_dvalue_set_float(v, req->tags[i].value.val_float);
            break;
        case NEU_JSON_BOOL:
            v = neu_dvalue_new(NEU_DTYPE_BOOL);
            neu_dvalue_set_bool(v, req->tags[i].value.val_bool);
            break;
        default:
            assert(false);
            break;
        }

        neu_int_val_init(&iv, req->tags[i].tag_id, v);
        neu_fixed_array_set(array, i, &iv);
    }

    neu_dvalue_init_move_array(val, NEU_DTYPE_INT_VAL, array);
    return val;
}