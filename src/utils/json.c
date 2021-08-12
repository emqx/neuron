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
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "json.h"
#include "neu_log.h"

static json_t *encode_object(json_t *object, neu_json_elem_t ele)
{
    json_t *ob = object;

    switch (ele.t) {
    case NEU_JSON_INT:
        json_object_set(ob, ele.name, json_integer(ele.v.val_int));
        break;
    case NEU_JSON_STR:
        json_object_set(ob, ele.name, json_string(ele.v.val_str));
        break;
    case NEU_JSON_DOUBLE:
        json_object_set(ob, ele.name, json_real(ele.v.val_double));
        break;
    case NEU_JSON_BOOL:
        json_object_set(ob, ele.name, json_boolean(ele.v.val_bool));
        break;
    case NEU_JSON_OBJECT:
        json_object_set(ob, ele.name, ele.v.object);
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
        log_error("json decode: %s failed", ele->name);
        return -1;
    }

    if (ele->t == NEU_JSON_UNDEFINE) {
        if (json_is_string(ob)) {
            ele->t = NEU_JSON_STR;
        } else if (json_is_real(ob)) {
            ele->t = NEU_JSON_DOUBLE;
        } else if (json_is_true(ob)) {
            ele->t = NEU_JSON_BOOL;
        } else if (json_is_integer(ob)) {
            ele->t = NEU_JSON_INT;
        }
    }

    switch (ele->t) {
    case NEU_JSON_INT:
        ele->v.val_int = json_integer_value(ob);
        break;
    case NEU_JSON_STR:
        ele->v.val_str = strdup(json_string_value(ob));
        break;
    case NEU_JSON_DOUBLE:
        ele->v.val_double = json_real_value(ob);
        break;
    case NEU_JSON_BOOL:
        ele->v.val_bool = json_boolean_value(ob);
        break;
    case NEU_JSON_OBJECT:
        ele->v.object = ob;
        break;
    default:
        log_error("json decode unknown type: %d", ele->t);
        return -1;
    }

    return 0;
}

int neu_json_decode(char *buf, int size, neu_json_elem_t *ele, ...)
{
    json_error_t     error;
    json_t *         root = json_loads(buf, 0, &error);
    neu_json_elem_t *tmp  = NULL;

    if (root == NULL) {
        log_error(
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return -1;
    }

    if (decode_object(root, ele) == -1) {
        json_decref(root);
        return -1;
    }

    va_list vl;
    va_start(vl, ele);
    for (int i = 0; i < size - 1; i++) {
        tmp = va_arg(vl, neu_json_elem_t *);
        if (decode_object(root, tmp) == -1) {
            json_decref(root);
            return -1;
        }
    }
    va_end(vl);

    json_decref(root);
    return 0;
}

int neu_json_decode_array(void *object, int index, int size,
                          neu_json_elem_t *ele, ...)
{
    json_t *         child = NULL;
    neu_json_elem_t *tmp;

    child = json_array_get(object, index);
    if (child == NULL) {
        return -1;
    }

    if (decode_object(child, ele) == -1) {
        return -1;
    }

    va_list vl;
    va_start(vl, ele);
    for (int i = 0; i < size - 1; i++) {
        tmp = va_arg(vl, neu_json_elem_t *);
        if (decode_object(child, tmp) == -1) {
            return -1;
        }
    }
    va_end(vl);

    return 0;
}

int neu_json_decode_array_size(char *buf, char *child)
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

int neu_json_encode(neu_json_elem_t *t, int n, char **str)
{
    json_t *root = json_object();

    for (int i = 0; i < n; i++) {
        encode_object(root, t[i]);
    }

    *str = json_dumps(root, 0);

    json_decref(root);

    return 0;
}
