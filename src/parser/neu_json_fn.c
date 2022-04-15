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

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datatag_table.h"
#include "json/json.h"

#include "json/neu_json_fn.h"
#include "json/neu_json_mqtt.h"
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

int neu_json_encode_setting_with_mqtt(uint32_t node_id, char *setting,
                                      void *mqtt_param, char **result)
{
    neu_json_mqtt_t *mqtt   = mqtt_param;
    char *           params = strstr(setting, "\"params\"");

    if (NULL == params) {
        return -1;
    }

    int i       = sizeof("\"params\"");
    int unmatch = 0;

    while (params[i]) {
        switch (params[i]) {
        case '{':
            ++unmatch; // fall through
        case ':':
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            ++i;
            break;
        default:
            return -1;
        }
        if (unmatch) {
            break;
        }
    }

    while (params[i]) {
        if ('{' == params[i]) {
            ++unmatch;
        } else if ('}' == params[i]) {
            --unmatch;
        }
        ++i;
        if (0 == unmatch) {
            break;
        }
    }

    if (unmatch) {
        return -1;
    }

    // At this point, the string started at `params` with length `i` is the
    // the settings that should present in the response.
    // Note that this is not an elegant solution due to the limitation of
    // the current implementation.

    const char *fmt = "{\"node_id\": %u, \"uuid\": \"%s\", "
                      "\"command\":\"%s\", "
                      "%.*s, \"error\": 0}";
    // plus 1 for '\0'
    int size = 1 +
        snprintf(NULL, 0, fmt, node_id, mqtt->uuid, mqtt->command, i, params);

    *result = calloc(size, sizeof(char));
    if (NULL == *result) {
        return -1;
    }

    snprintf(*result, size, fmt, node_id, mqtt->uuid, mqtt->command, i, params);

    return 0;
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

neu_data_val_t *neu_parse_write_req_to_val(neu_json_write_req_t *req,
                                           neu_datatag_table_t * tbl)
{
    neu_datatag_t *tag = neu_datatag_tbl_get_by_name(tbl, req->tag_name);

    if (tag == NULL) {
        return NULL;
    }

    neu_data_val_t *   val   = neu_dvalue_unit_new();
    neu_fixed_array_t *array = neu_fixed_array_new(1, sizeof(neu_int_val_t));

    neu_int_val_t   iv;
    neu_data_val_t *v = NULL;

    switch (tag->type) {
    case NEU_DTYPE_UINT8:
        v = neu_dvalue_new(NEU_DTYPE_UINT8);
        neu_dvalue_set_uint8(v, (uint8_t) req->value.val_int);
        break;
    case NEU_DTYPE_INT8:
        v = neu_dvalue_new(NEU_DTYPE_INT8);
        neu_dvalue_set_int8(v, (int8_t) req->value.val_int);
        break;
    case NEU_DTYPE_UINT16:
        v = neu_dvalue_new(NEU_DTYPE_UINT16);
        neu_dvalue_set_uint16(v, (uint16_t) req->value.val_int);
        break;
    case NEU_DTYPE_INT16:
        v = neu_dvalue_new(NEU_DTYPE_INT16);
        neu_dvalue_set_int16(v, (int16_t) req->value.val_int);
        break;
    case NEU_DTYPE_INT32:
        v = neu_dvalue_new(NEU_DTYPE_INT32);
        neu_dvalue_set_int32(v, (int32_t) req->value.val_int);
        break;
    case NEU_DTYPE_UINT32:
        v = neu_dvalue_new(NEU_DTYPE_UINT32);
        neu_dvalue_set_uint32(v, (uint32_t) req->value.val_int);
        break;
    case NEU_DTYPE_INT64:
        v = neu_dvalue_new(NEU_DTYPE_INT64);
        neu_dvalue_set_int64(v, (int64_t) req->value.val_int);
        break;
    case NEU_DTYPE_UINT64:
        v = neu_dvalue_new(NEU_DTYPE_UINT64);
        neu_dvalue_set_uint64(v, (uint64_t) req->value.val_int);
        break;
    case NEU_DTYPE_FLOAT:
        v = neu_dvalue_new(NEU_DTYPE_FLOAT);
        neu_dvalue_set_float(v, (float) req->value.val_double);
        break;
    case NEU_DTYPE_DOUBLE:
        v = neu_dvalue_new(NEU_DTYPE_DOUBLE);
        neu_dvalue_set_double(v, req->value.val_double);
        break;
    case NEU_DTYPE_BIT:
        v = neu_dvalue_new(NEU_DTYPE_BIT);
        neu_dvalue_set_bit(v, (uint8_t) req->value.val_int);
        break;
    case NEU_DTYPE_BOOL:
        v = neu_dvalue_new(NEU_DTYPE_BOOL);
        neu_dvalue_set_bool(v, req->value.val_bool);
        break;
    case NEU_DTYPE_CSTR:
        v = neu_dvalue_new(NEU_DTYPE_CSTR);
        neu_dvalue_set_cstr(v, req->value.val_str);
        break;
    default:
        break;
    }

    neu_int_val_init(&iv, tag->id, v);
    neu_fixed_array_set(array, 0, &iv);

    neu_dvalue_init_move_array(val, NEU_DTYPE_INT_VAL, array);
    return val;
}
