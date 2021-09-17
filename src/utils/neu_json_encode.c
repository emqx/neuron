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

static json_t *encode_object_value(neu_data_val_t *v_value)
{
    json_t *object = NULL;
    int     ret    = 0;

    switch (neu_dvalue_get_type(v_value)) {
    case NEU_DTYPE_CSTR: {
        char *value = NULL;

        ret = neu_dvalue_get_ref_cstr(v_value, &value);
        assert(ret == 0);

        object = json_string(value);
        break;
    }
    case NEU_DTYPE_FLOAT: {
        float value = 0;

        ret = neu_dvalue_get_float(v_value, &value);
        assert(ret == 0);

        object = json_real(value);
        break;
    }
    case NEU_DTYPE_DOUBLE: {
        double value = 0;

        ret = neu_dvalue_get_double(v_value, &value);
        assert(ret == 0);

        object = json_real(value);
        break;
    }
    case NEU_DTYPE_BOOL: {
        bool value = false;

        ret = neu_dvalue_get_bool(v_value, &value);
        assert(ret == 0);

        object = json_boolean(value);
        break;
    }
    case NEU_DTYPE_INT8: {
        int8_t value = 0;

        ret = neu_dvalue_get_int8(v_value, &value);
        assert(ret == 0);

        object = json_integer(value);
        break;
    }
    case NEU_DTYPE_UINT8: {
        uint8_t value = 0;

        ret = neu_dvalue_get_uint8(v_value, &value);
        assert(ret == 0);

        object = json_integer(value);
        break;
    }
    case NEU_DTYPE_INT16: {
        int16_t value = 0;

        ret = neu_dvalue_get_int16(v_value, &value);
        assert(ret == 0);

        object = json_integer(value);
        break;
    }
    case NEU_DTYPE_UINT16: {
        uint16_t value = 0;

        ret = neu_dvalue_get_uint16(v_value, &value);
        assert(ret == 0);

        object = json_integer(value);
        break;
    }
    case NEU_DTYPE_INT32: {
        int32_t value = 0;

        ret = neu_dvalue_get_int32(v_value, &value);
        assert(ret == 0);

        object = json_integer(value);
        break;
    }
    case NEU_DTYPE_UINT32: {
        uint32_t value = 0;

        ret = neu_dvalue_get_uint32(v_value, &value);
        assert(ret == 0);

        object = json_integer(value);
        break;
    }
    case NEU_DTYPE_INT64: {
        int64_t value = 0;

        ret = neu_dvalue_get_int64(v_value, &value);
        assert(ret == 0);

        object = json_integer(value);
        break;
    }
    case NEU_DTYPE_UINT64: {
        uint64_t value = 0;

        ret = neu_dvalue_get_uint64(v_value, &value);
        assert(ret == 0);

        object = json_integer(value);
        break;
    }
    default:
        log_error("json encode value unknown type: %d",
                  neu_dvalue_get_type(v_value));
        return 0;
    }

    return object;
}

static void encode_object(json_t *object, neu_fixed_array_t *kv)
{
    char *          name    = NULL;
    neu_data_val_t *v_name  = (neu_data_val_t *) neu_fixed_array_get(kv, 0);
    neu_data_val_t *v_value = (neu_data_val_t *) neu_fixed_array_get(kv, 1);
    int             ret     = neu_dvalue_get_ref_cstr(v_name, &name);

    assert(ret == 0);

    switch (neu_dvalue_get_type(v_value)) {
    case NEU_DTYPE_CSTR: {
        char *value = NULL;

        ret = neu_dvalue_get_ref_cstr(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_string(value));
        break;
    }
    case NEU_DTYPE_FLOAT: {
        float value = 0;

        ret = neu_dvalue_get_float(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_real(value));
        break;
    }
    case NEU_DTYPE_DOUBLE: {
        double value = 0;

        ret = neu_dvalue_get_double(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_real(value));
        break;
    }
    case NEU_DTYPE_BOOL: {
        bool value = 0;

        ret = neu_dvalue_get_bool(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_boolean(value));
        break;
    }
    case NEU_DTYPE_INT8: {
        int8_t value = 0;

        ret = neu_dvalue_get_int8(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_integer(value));
        break;
    }
    case NEU_DTYPE_UINT8: {
        uint8_t value = 0;

        ret = neu_dvalue_get_uint8(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_integer(value));
        break;
    }
    case NEU_DTYPE_INT16: {
        int16_t value = 0;

        ret = neu_dvalue_get_int16(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_integer(value));
        break;
    }
    case NEU_DTYPE_UINT16: {
        uint16_t value = 0;

        ret = neu_dvalue_get_uint16(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_integer(value));
        break;
    }
    case NEU_DTYPE_INT32: {
        int32_t value = 0;

        ret = neu_dvalue_get_int32(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_integer(value));
        break;
    }
    case NEU_DTYPE_UINT32: {
        uint32_t value = 0;

        ret = neu_dvalue_get_uint32(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_integer(value));
        break;
    }
    case NEU_DTYPE_INT64: {
        int64_t value = 0;

        ret = neu_dvalue_get_int64(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_integer(value));
        break;
    }
    case NEU_DTYPE_UINT64: {
        uint64_t value = 0;

        ret = neu_dvalue_get_uint64(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, json_integer(value));
        break;
    }
    case NEU_DTYPE_BIT:
        break;
    case NEU_DTYPE_DATA_VAL: {
        void *value = NULL;

        ret = neu_dvalue_get_data_val(v_value, &value);
        assert(ret == 0);

        json_object_set_new(object, name, value);
        break;
    }
    default:
        log_error("json encode unknown type: %d", neu_dvalue_get_type(v_value));
        return 0;
    }
}

void *neu_jsonx_encode_array(void *array, neu_data_val_t *val)
{
    neu_fixed_array_t *v_array = NULL;
    int                ret     = neu_dvalue_get_ref_array(val, &v_array);

    if (array == NULL) {
        array = json_array();
    }

    assert(ret == 0);

    json_t *ob = json_object();
    for (int i = 0; i < v_array->length; i++) {
        neu_fixed_array_t *kv = NULL;
        neu_data_val_t *   ele =
            (neu_data_val_t *) neu_fixed_array_get(v_array, i);
        ret = neu_dvalue_get_ref_array(ele, &kv);

        assert(ret == 0);
        assert(kv->length == 2);

        encode_object(ob, kv);
    }

    json_array_append_new(array, ob);
    return array;
}

void *neu_jsonx_encode_array_value(void *array, neu_data_val_t *val)
{
    neu_fixed_array_t *v_array = NULL;
    int                ret     = neu_dvalue_get_ref_array(val, &v_array);

    if (array == NULL) {
        array = json_array();
    }

    assert(ret == 0);

    for (int i = 0; i < v_array->length; i++) {
        neu_data_val_t *ele =
            (neu_data_val_t *) neu_fixed_array_get(v_array, i);

        json_array_append_new(array, encode_object_value(ele));
    }

    return array;
}

void *neu_jsonx_new()
{
    return json_object();
}

int neu_jsonx_encode(void *object, neu_data_val_t *val, char **result)
{
    neu_fixed_array_t *v_array = NULL;
    int                ret     = neu_dvalue_get_ref_array(val, &v_array);
    json_t *           root    = object;

    assert(ret == 0);

    for (int i = 0; i < v_array->length; i++) {
        neu_fixed_array_t *kv = NULL;
        neu_data_val_t *   ele =
            (neu_data_val_t *) neu_fixed_array_get(v_array, i);
        ret = neu_dvalue_get_ref_array(ele, &kv);

        assert(ret == 0);
        assert(kv->length == 2);

        encode_object(root, kv);
    }

    *result = json_dumps(root, JSON_REAL_PRECISION(16));

    json_decref(root);
    return 0;
}

int neu_jsonx_encode_field(void *json_object, neu_fixed_array_t *kv)
{
    encode_object(json_object, kv);
}