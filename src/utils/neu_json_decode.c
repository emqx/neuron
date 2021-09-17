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
#include <assert.h>

#include <jansson.h>

#include "neu_json.h"

static int decode_object(json_t *root, neu_fixed_array_t *kv)
{
    json_t *        ob      = NULL;
    neu_data_val_t *v_name  = (neu_data_val_t *) neu_fixed_array_get(kv, 0);
    neu_data_val_t *v_value = (neu_data_val_t *) neu_fixed_array_get(kv, 1);
    char *          name    = NULL;
    int             ret     = neu_dvalue_get_ref_cstr(v_name, &name);

    assert(ret == 0);

    if (name == NULL) {
        ob = root;
    } else {
        ob = json_object_get(root, name);
    }

    if (ob == NULL) {
        log_error("json decode: %s failed", name);
        return -1;
    }

    if (neu_dvalue_get_type(v_value) == NEU_DTYPE_UNIT) {
        if (json_is_string(ob)) {
            neu_dvalue_init_ref_cstr(v_value, NULL);
        } else if (json_is_real(ob)) {
            neu_dvalue_init_double(v_value, 0);
        } else if (json_is_boolean(ob)) {
            neu_dvalue_init_bool(v_value, false);
        } else if (json_is_integer(ob)) {
            neu_dvalue_init_int64(v_value, 0);
        }
    }

    switch (neu_dvalue_get_type(v_value)) {
    case NEU_DTYPE_CSTR:
        neu_dvalue_init_copy_cstr(v_value, (char *) json_string_value(ob));
        break;
    case NEU_DTYPE_FLOAT:
        neu_dvalue_init_float(v_value, json_real_value(ob));
        break;
    case NEU_DTYPE_DOUBLE:
        neu_dvalue_init_double(v_value, json_real_value(ob));
        break;
    case NEU_DTYPE_BOOL:
        neu_dvalue_init_bool(v_value, json_boolean_value(ob));
        break;
    case NEU_DTYPE_INT8:
        neu_dvalue_init_int8(v_value, json_integer_value(ob));
        break;
    case NEU_DTYPE_UINT8:
        neu_dvalue_init_uint8(v_value, json_integer_value(ob));
        break;
    case NEU_DTYPE_INT16:
        neu_dvalue_init_int16(v_value, json_integer_value(ob));
        break;
    case NEU_DTYPE_UINT16:
        neu_dvalue_init_uint16(v_value, json_integer_value(ob));
        break;
    case NEU_DTYPE_INT32:
        neu_dvalue_init_int32(v_value, json_integer_value(ob));
        break;
    case NEU_DTYPE_UINT32:
        neu_dvalue_init_uint32(v_value, json_integer_value(ob));
        break;
    case NEU_DTYPE_INT64:
        neu_dvalue_init_int64(v_value, json_integer_value(ob));
        break;
    case NEU_DTYPE_UINT64:
        neu_dvalue_init_uint64(v_value, json_integer_value(ob));
        break;
    case NEU_DTYPE_BIT:
        break;
    case NEU_DTYPE_DATA_VAL:
        neu_dvalue_init_data_val(v_value, ob);
        break;
    default:
        log_error("json decode unknown type: %d", neu_dvalue_get_type(v_value));
        return 0;
    }

    return 0;
}

int neu_jsonx_decode(char *buf, neu_data_val_t *val)
{
    json_error_t       error;
    json_t *           root  = json_loads(buf, 0, &error);
    neu_fixed_array_t *array = NULL;
    int                ret   = neu_dvalue_get_ref_array(val, &array);

    if (root == NULL) {
        log_error(
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return -1;
    }

    assert(ret == 0);

    for (uint32_t i = 0; i < array->length; i++) {
        neu_fixed_array_t *kv = NULL;
        neu_data_val_t *ele = (neu_data_val_t *) neu_fixed_array_get(array, i);
        ret                 = neu_dvalue_get_ref_array(ele, &kv);

        assert(ret == 0);
        assert(kv->length == 2);

        if (decode_object(root, kv) == -1) {
            json_decref(root);
            return -1;
        }
    }

    json_decref(root);
    return 0;
}

int neu_jsonx_decode_array_size(char *buf, char *child)
{
    json_error_t error;
    json_t *     root = json_loads(buf, 0, &error);
    json_t *     ob;
    int          ret = -1;

    if (root == NULL) {
        log_error(
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return -1;
    }

    ob = json_object_get(root, child);
    if (ob != NULL && json_is_array(ob)) {
        ret = json_array_size(ob);
    } else {
        log_error("json get array object fail, %s", child);
    }

    json_decref(root);
    return ret;
}

int neu_jsonx_decode_array(char *buf, char *name, int index,
                           neu_data_val_t *val)
{
    json_error_t       error;
    json_t *           child  = NULL;
    json_t *           object = NULL;
    json_t *           root   = json_loads(buf, 0, &error);
    neu_fixed_array_t *array  = NULL;
    int                ret    = neu_dvalue_get_ref_array(val, &array);

    if (root == NULL) {
        log_error(
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return -1;
    }

    assert(ret == 0);

    object = json_object_get(root, name);
    if (object == NULL) {
        json_decref(root);
        return -1;
    }

    child = json_array_get(object, index);
    if (child == NULL) {
        json_decref(root);
        return -1;
    }

    for (uint32_t i = 0; i < array->length; i++) {
        neu_fixed_array_t *kv = NULL;
        neu_data_val_t *ele = (neu_data_val_t *) neu_fixed_array_get(array, i);
        ret                 = neu_dvalue_get_ref_array(ele, &kv);

        assert(ret == 0);
        assert(kv->length == 2);

        if (decode_object(root, kv) == -1) {
            json_decref(root);
            return -1;
        }
    }

    json_decref(root);
    return 0;
}
