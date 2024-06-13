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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "utils/log.h"
#include "json/json.h"

static double format_tag_value(float ele_value)
{
    double scale    = pow(10, 5);
    double value    = ele_value;
    int    negative = 1;

    if (value < 0) {
        value *= -1;
        negative = -1;
    }

    int64_t integer_part = (int64_t)(value);
    double  decimal_part = value - integer_part;
    decimal_part *= scale;
    decimal_part = round(decimal_part);

    char str[6] = { 0 };
    snprintf(str, sizeof(str), "%05" PRId64 "", (int64_t) decimal_part);
    int i = 0, flag = 0;
    for (; i < 4; i++) {
        if (str[i] == '0' && str[i + 1] == '0') {
            flag = 1;
            break;
        } else if (str[i] == '9' && str[i + 1] == '9') {
            flag = 2;
            break;
        }
    }
    if (flag != 0 && i != 0) {
        decimal_part = round(decimal_part / pow(10, 5 - i));
        value        = (double) integer_part + decimal_part / pow(10, i);
    } else {
        value = (double) integer_part + decimal_part / scale;
    }

    return value * negative;
}

static json_t *encode_object_value(neu_json_elem_t *ele)
{
    json_t *ob = NULL;

    switch (ele->t) {
    case NEU_JSON_BIT:
        ob = json_integer(ele->v.val_bit);
        break;
    case NEU_JSON_INT:
        ob = json_integer(ele->v.val_int);
        break;
    case NEU_JSON_STR:
        ob = json_string(ele->v.val_str);
        break;
    case NEU_JSON_DOUBLE:
        ob = json_realp(ele->v.val_double, ele->precision);
        break;
    case NEU_JSON_FLOAT: {
        double t = ele->v.val_float;
        if (ele->precision == 0 && ele->bias == 0) {
            t = format_tag_value(ele->v.val_float);
        }
        ob = json_realp(t, ele->precision);
        break;
    }
    case NEU_JSON_BOOL:
        ob = json_boolean(ele->v.val_bool);
        break;
    default:
        break;
    }

    return ob;
}

static json_t *encode_object(json_t *object, neu_json_elem_t ele)
{
    json_t *ob = object;

    switch (ele.t) {
    case NEU_JSON_BIT:
        json_object_set_new(ob, ele.name, json_integer(ele.v.val_bit));
        break;
    case NEU_JSON_INT:
        json_object_set_new(ob, ele.name, json_integer(ele.v.val_int));
        break;
    case NEU_JSON_STR:
        json_object_set_new(ob, ele.name, json_string(ele.v.val_str));
        break;
    case NEU_JSON_FLOAT: {
        double t = ele.v.val_float;
        if (ele.precision == 0 && ele.bias == 0) {
            t = format_tag_value(ele.v.val_float);
        }
        json_object_set_new(ob, ele.name, json_realp(t, ele.precision));
        break;
    }
    case NEU_JSON_DOUBLE:
        json_object_set_new(ob, ele.name,
                            json_realp(ele.v.val_double, ele.precision));
        break;
    case NEU_JSON_BOOL:
        json_object_set_new(ob, ele.name, json_boolean(ele.v.val_bool));
        break;
    case NEU_JSON_BYTES: {
        void *bytes = json_array();

        for (int i = 0; i < ele.v.val_bytes.length; i++) {
            json_array_append_new(bytes,
                                  json_integer(ele.v.val_bytes.bytes[i]));
        }

        json_object_set_new(ob, ele.name, bytes);
        break;
    }
    case NEU_JSON_OBJECT:
        json_object_set_new(ob, ele.name, ele.v.val_object);
        break;
    default:
        break;
    }

    return ob;
}

static int decode_object(json_t *root, neu_json_elem_t *ele)
{
    json_t *ob = NULL;

    if (ele->name == NULL) {
        ob = root;
    } else {
        ob = json_object_get(root, ele->name);
    }

    if (ob == NULL) {
        if (ele->attribute == NEU_JSON_ATTRIBUTE_OPTIONAL) {
            return 0;
        }
        zlog_error(neuron, "json decode: %s failed", ele->name);
        return -1;
    }

    if (ele->t == NEU_JSON_UNDEFINE) {
        if (json_is_string(ob)) {
            ele->t = NEU_JSON_STR;
        } else if (json_is_real(ob)) {
            ele->t = NEU_JSON_DOUBLE;
        } else if (json_is_boolean(ob)) {
            ele->t = NEU_JSON_BOOL;
        } else if (json_is_integer(ob)) {
            ele->t = NEU_JSON_INT;
        } else if (json_is_array(ob)) {
            ele->t = NEU_JSON_BYTES;
        }
    }

    switch (ele->t) {
    case NEU_JSON_BIT:
        ele->v.val_bit = json_integer_value(ob);
        break;
    case NEU_JSON_INT:
        ele->v.val_int = json_integer_value(ob);
        break;
    case NEU_JSON_STR:
        if (!json_is_string(ob)) {
            zlog_error(neuron, "json decode: %s failed", ele->name);
            return -1;
        }
        const char *str_val = json_string_value(ob);
        if (str_val == NULL) {
            zlog_error(neuron, "json decode: %s failed", ele->name);
            return -1;
        }
        ele->v.val_str = strdup(str_val);
        break;
    case NEU_JSON_FLOAT:
        if (json_is_integer(ob)) {
            ele->v.val_float = (float) json_integer_value(ob);
        } else {
            ele->v.val_float = json_real_value(ob);
        }
        break;
    case NEU_JSON_DOUBLE:
        if (json_is_integer(ob)) {
            ele->v.val_double = (double) json_integer_value(ob);
        } else {
            ele->v.val_double = json_real_value(ob);
        }
        break;
    case NEU_JSON_BOOL:
        ele->v.val_bool = json_boolean_value(ob);
        break;
    case NEU_JSON_BYTES: {
        json_t *value = NULL;

        ele->v.val_bytes.length = json_array_size(ob);
        if (ele->v.val_bytes.length > 0) {
            int index = 0;

            ele->v.val_bytes.bytes =
                calloc(ele->v.val_bytes.length, sizeof(uint8_t));
            json_array_foreach(ob, index, value)
            {
                ele->v.val_bytes.bytes[index] =
                    (uint8_t) json_integer_value(value);
            }
        }
        break;
    }
    case NEU_JSON_OBJECT:
        ele->v.val_object = ob;
        break;
    default:
        zlog_error(neuron, "json decode unknown type: %d", ele->t);
        return -1;
    }

    return 0;
}

void *neu_json_array()
{
    return json_array();
}

int neu_json_decode(char *buf, int size, neu_json_elem_t *ele)
{
    json_error_t error;
    json_t *     root = json_loads(buf, 0, &error);

    if (root == NULL) {
        zlog_error(neuron,
                   "json load error, line: %d, column: %d, position: %d, info: "
                   "%s",
                   error.line, error.column, error.position, error.text);
        return -1;
    }

    for (int i = 0; i < size; i++) {
        if (decode_object(root, &ele[i]) == -1) {
            json_decref(root);
            return -1;
        }
    }

    json_decref(root);
    return 0;
}

int neu_json_decode_by_json(void *json, int size, neu_json_elem_t *ele)
{
    if (json == NULL) {
        zlog_error(neuron, "The param json is NULL");
        return -1;
    }

    for (int i = 0; i < size; i++) {
        if (decode_object(json, &ele[i]) == -1) {
            return -1;
        }
    }

    return 0;
}

int neu_json_decode_array(char *buf, char *name, int index, int size,
                          neu_json_elem_t *ele)
{
    json_t *     child  = NULL;
    json_t *     object = NULL;
    json_error_t error;
    json_t *     root = json_loads(buf, 0, &error);

    if (root == NULL) {
        zlog_error(
            neuron,
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return -1;
    }

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

    for (int i = 0; i < size; i++) {
        if (decode_object(child, &ele[i]) == -1) {
            json_decref(root);
            return -1;
        }
    }

    json_decref(root);
    return 0;
}

int neu_json_decode_array_elem(void *json, int index, int size,
                               neu_json_elem_t *ele)
{
    json_t *child = NULL;

    child = json_array_get(json, index);
    if (child == NULL) {
        return -1;
    }

    for (int i = 0; i < size; i++) {
        if (decode_object(child, &ele[i]) == -1) {
            return -1;
        }
    }

    return 0;
}

int neu_json_decode_array_by_json(void *json, char *name, int index, int size,
                                  neu_json_elem_t *ele)
{
    json_t *child  = NULL;
    json_t *object = NULL;

    object = json_object_get(json, name);
    if (object == NULL) {
        return -1;
    }

    child = json_array_get(object, index);
    if (child == NULL) {
        return -1;
    }

    for (int i = 0; i < size; i++) {
        if (decode_object(child, &ele[i]) == -1) {
            return -1;
        }
    }

    return 0;
}

int neu_json_decode_array_size_by_json(void *json, char *child)
{
    json_t *ob;
    int     ret = -1;

    ob = json_object_get(json, child);
    if (ob != NULL && json_is_array(ob)) {
        ret = json_array_size(ob);
    } else {
        zlog_error(neuron, "json get array object fail, %s", child);
    }

    return ret;
}

int neu_json_decode_array_size(char *buf, char *child)
{
    json_error_t error;
    json_t *     root = json_loads(buf, 0, &error);
    json_t *     ob;
    int          ret = -1;

    if (root == NULL) {
        zlog_error(
            neuron,
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return -1;
    }

    ob = json_object_get(root, child);
    if (ob != NULL && json_is_array(ob)) {
        ret = json_array_size(ob);
    } else {
        zlog_error(neuron, "json get array object fail, %s", child);
    }

    json_decref(root);
    return ret;
}

void *neu_json_encode_array_value(void *array, neu_json_elem_t *t, int n)
{

    if (array == NULL) {
        array = json_array();
    }

    for (int i = 0; i < n; i++) {
        json_array_append_new(array, encode_object_value(&t[i]));
    }

    return array;
}

void *neu_json_encode_array(void *array, neu_json_elem_t *t, int n)
{
    if (array == NULL) {
        array = json_array();
    }

    json_t *ob = json_object();
    for (int j = 0; j < n; j++) {
        encode_object(ob, t[j]);
    }

    json_array_append_new(array, ob);
    return array;
}

void *neu_json_encode_new()
{
    return json_object();
}

void neu_json_encode_free(void *json_object)
{
    json_decref(json_object);
}

int neu_json_encode_field(void *json_object, neu_json_elem_t *elem, int n)
{
    for (int i = 0; i < n; i++) {
        encode_object(json_object, elem[i]);
    }

    return 0;
}

int neu_json_encode(void *json_object, char **str)
{
    *str = json_dumps(json_object, JSON_REAL_PRECISION(16));

    return 0;
}

void *neu_json_decode_new(const char *buf)
{
    json_error_t error;
    json_t *     root = json_loads(buf, JSON_DECODE_ANY, &error);

    if (root == NULL) {
        zlog_error(
            neuron,
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return NULL;
    }

    return root;
}

void *neu_json_decode_newb(char *buf, size_t len)
{
    json_error_t error;
    json_t *     root = json_loadb(buf, len, 0, &error);

    if (root == NULL) {
        zlog_error(
            neuron,
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return NULL;
    }

    return root;
}

void neu_json_decode_free(void *ob)
{
    json_decref(ob);
}

int neu_json_decode_value(void *object, neu_json_elem_t *ele)
{
    return decode_object(object, ele);
}

int neu_json_dump_key(void *object, const char *key, char **const result,
                      bool must_exist)
{
    int rv        = 0;
    json_t *new   = NULL;
    json_t *value = NULL;

    value = json_object_get(object, key);
    if (NULL == value) {
        if (must_exist) {
            zlog_error(neuron, "json has no key `%s`", key);
            return -1;
        } else {
            *result = NULL;
            return 0;
        }
    }

    if (NULL == (new = json_object()) || json_object_set(new, key, value) < 0 ||
        NULL == (*result = json_dumps(new, JSON_COMPACT))) {
        rv = -1;
    }

    json_decref(new);
    return rv;
}

int neu_json_load_key(void *object, const char *key, const char *input,
                      bool must_exist)
{
    int          rv           = 0;
    json_t *     input_object = NULL;
    json_t *     value        = NULL;
    json_error_t error        = {};

    input_object = json_loads(input, 0, &error);
    if (NULL == input_object) {
        zlog_error(neuron,
                   "json load error, line: %d, column: %d, position: %d, info: "
                   "%s",
                   error.line, error.column, error.position, error.text);
        return -1;
    }

    value = json_object_get(input_object, key);
    if (NULL == value) {
        if (must_exist) {
            zlog_error(neuron, "json has no key `%s`", key);
            rv = -1;
        }
    } else if (json_object_set(object, key, value) < 0) {
        rv = -1;
    }

    json_decref(input_object);
    return rv;
}
