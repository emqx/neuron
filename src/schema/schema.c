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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "neu_log.h"
#include "schema.h"
#include "json/json.h"

struct neu_schema_valid {
    void *   root;
    void *   plugin_json;
    int      n_type;
    uint32_t types[32];
};

static bool valid_init(void *json, neu_json_elem_t *ele, char *name);

neu_schema_valid_t *neu_schema_load(char *buf, char *plugin)
{
    void *              json = neu_json_decode_new(buf);
    neu_json_elem_t     elem = { .name = plugin, .t = NEU_JSON_OBJECT };
    int                 ret  = 0;
    neu_schema_valid_t *st   = NULL;
    int                 size = 0;

    assert(json != NULL);
    ret = neu_json_decode_value(json, &elem);
    if (ret != 0) {
        neu_json_decode_free(json);
        return st;
    }
    size = neu_json_decode_array_size_by_json(elem.v.object, "tag_type");

    if (size <= 0) {
        neu_json_decode_free(json);
        return NULL;
    }
    assert(size <= 32);

    st = calloc(1, sizeof(neu_schema_valid_t));

    for (int i = 0; i < size; i++) {
        neu_json_elem_t e = { .name = NULL, .t = NEU_JSON_INT };
        if (neu_json_decode_array_by_json(elem.v.object, "tag_type", i, 1,
                                          &e) == 0) {
            st->types[st->n_type] = e.v.val_int;
            st->n_type++;
        }
    }

    st->root        = json;
    st->plugin_json = elem.v.object;

    return st;
}

void neu_schema_free(neu_schema_valid_t *v)
{
    neu_json_decode_free(v->root);
    free(v);
}

int neu_schema_valid_tag_type(neu_schema_valid_t *v, uint8_t t)
{
    for (int i = 0; i < v->n_type; i++) {
        if (v->types[i] == t)
            return 0;
    }

    return -1;
}

static bool valid_init(void *json, neu_json_elem_t *vele, char *name)
{
    neu_json_elem_t ele = { .name = name, .t = NEU_JSON_OBJECT };

    vele->name = "valid";
    vele->t    = NEU_JSON_OBJECT;

    return neu_json_decode_value(json, &ele) == 0 &&
        neu_json_decode_value(ele.v.object, vele) == 0;
}

int neu_schema_valid_param_int(neu_schema_valid_t *v, int64_t value, char *name)
{
    neu_json_elem_t valid_ele = { 0 };
    neu_json_elem_t min_ele   = { .name = "min", .t = NEU_JSON_INT };
    neu_json_elem_t max_ele   = { .name = "max", .t = NEU_JSON_INT };

    if (!valid_init(v->plugin_json, &valid_ele, name)) {
        return -1;
    }

    if (neu_json_decode_value(valid_ele.v.object, &min_ele) != 0 ||
        neu_json_decode_value(valid_ele.v.object, &max_ele) != 0) {
        return 0;
    }

    if (value >= min_ele.v.val_int && value <= max_ele.v.val_int) {
        return 0;
    }

    return -1;
}

int neu_schema_valid_param_real(neu_schema_valid_t *v, double value, char *name)
{
    neu_json_elem_t valid_ele = { 0 };
    neu_json_elem_t min_ele   = { .name = "min", .t = NEU_JSON_DOUBLE };
    neu_json_elem_t max_ele   = { .name = "max", .t = NEU_JSON_DOUBLE };

    if (!valid_init(v->plugin_json, &valid_ele, name)) {
        return -1;
    }

    if (neu_json_decode_value(valid_ele.v.object, &min_ele) != 0 ||
        neu_json_decode_value(valid_ele.v.object, &max_ele) != 0) {
        return 0;
    }

    if (value >= min_ele.v.val_double && value <= max_ele.v.val_double) {
        return 0;
    }

    return -1;
}

int neu_schema_valid_param_string(neu_schema_valid_t *v, char *value,
                                  char *name)
{
    neu_json_elem_t valid_ele = { 0 };
    neu_json_elem_t len_ele   = { .name = "length", .t = NEU_JSON_INT };

    if (!valid_init(v->plugin_json, &valid_ele, name)) {
        return -1;
    }

    if (neu_json_decode_value(valid_ele.v.object, &len_ele) != 0) {
        return 0;
    }

    if ((uint64_t) len_ele.v.val_int >= strlen(value)) {
        return 0;
    }

    return -1;
}

int neu_schema_valid_param_enum(neu_schema_valid_t *v, int64_t value,
                                char *name)
{
    neu_json_elem_t valid_ele = { 0 };
    int             size      = 0;

    if (!valid_init(v->plugin_json, &valid_ele, name)) {
        return -1;
    }

    size = neu_json_decode_array_size_by_json(valid_ele.v.object, "value");
    if (size <= 0) {
        return -1;
    }

    for (int i = 0; i < size; i++) {
        neu_json_elem_t ve = { .name = NULL, .t = NEU_JSON_INT };
        if (neu_json_decode_array_by_json(valid_ele.v.object, "value", i, 1,
                                          &ve) == 0) {
            if (ve.v.val_int == value) {
                return 0;
            }
        }
    }

    return -1;
}