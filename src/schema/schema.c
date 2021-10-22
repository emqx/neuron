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
#include "utils/json.h"

struct neu_schema_valid {
    void *json;
    char *content;
};

static bool valid_init(void *json, neu_json_elem_t *ele, char *name);
static int  valid_int(neu_schema_valid_t *v, void *value, char *name);
static int  valid_real(neu_schema_valid_t *v, void *value, char *name);
static int  valid_string(neu_schema_valid_t *v, void *value, char *name);
static int  valid_enum(neu_schema_valid_t *v, void *value, char *name);

neu_schema_valid_t *neu_schema_load(char *buf, char *plugin)
{
    void *              json = neu_json_decode_new(buf);
    neu_json_elem_t     elem = { .name = plugin, .t = NEU_JSON_OBJECT };
    int                 ret  = 0;
    neu_schema_valid_t *st   = NULL;

    assert(json != NULL);
    ret = neu_json_decode_value(json, &elem);
    if (ret != 0) {
        neu_json_decode_free(json);
        return st;
    }

    st       = calloc(1, sizeof(neu_schema_valid_t));
    st->json = json;
    neu_json_encode(json, &st->content);

    return st;
}

void neu_schema_free(neu_schema_valid_t *v)
{
    neu_json_decode_free(v->json);
    free(v->content);
    free(v);
}

int neu_schema_valid_param(neu_schema_valid_t *v, void *value, char *name,
                           neu_schema_valid_type_e type)
{
    int ret = 0;

    switch (type) {
    case NEU_SCHEMA_VALID_INT:
        ret = valid_int(v, value, name);
        break;
    case NEU_SCHEMA_VALID_REAL:
        ret = valid_real(v, value, name);
        break;
    case NEU_SCHEMA_VALID_STRING:
        ret = valid_string(v, value, name);
        break;
    case NEU_SCHEMA_VALID_ENUM:
        ret = valid_enum(v, value, name);
        break;
    }

    return ret;
}

int neu_schema_valid_tag_type(neu_schema_valid_t *v, uint8_t t)
{
    int size = neu_json_decode_array_size(v->content, "tag_type");

    if (size <= 0) {
        return -1;
    }

    for (int i = 0; i < size; i++) {
        neu_json_elem_t e = { .name = NULL, .t = NEU_JSON_INT };
        if (neu_json_decode_array(v->content, "tag_type", i, 1, &e) == 0) {
            if (t == e.v.val_int) {
                return 0;
            }
        }
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

static int valid_int(neu_schema_valid_t *v, void *value, char *name)
{
    neu_json_elem_t valid_ele = { 0 };
    neu_json_elem_t min_ele   = { .name = "min", .t = NEU_JSON_INT };
    neu_json_elem_t max_ele   = { .name = "max", .t = NEU_JSON_INT };
    int64_t *       vv        = (int64_t *) value;

    if (!valid_init(v->json, &valid_ele, name)) {
        return -1;
    }

    if (neu_json_decode_value(valid_ele.v.object, &min_ele) != 0 ||
        neu_json_decode_value(valid_ele.v.object, &max_ele) != 0) {
        return 0;
    }

    if (*vv >= min_ele.v.val_int && *vv <= max_ele.v.val_int) {
        return 0;
    }

    return -1;
}

static int valid_real(neu_schema_valid_t *v, void *value, char *name)
{
    neu_json_elem_t valid_ele = { 0 };
    neu_json_elem_t min_ele   = { .name = "min", .t = NEU_JSON_DOUBLE };
    neu_json_elem_t max_ele   = { .name = "max", .t = NEU_JSON_DOUBLE };
    double *        vv        = (double *) value;

    if (!valid_init(v->json, &valid_ele, name)) {
        return -1;
    }

    if (neu_json_decode_value(valid_ele.v.object, &min_ele) != 0 ||
        neu_json_decode_value(valid_ele.v.object, &max_ele) != 0) {
        return 0;
    }

    if (*vv >= min_ele.v.val_int && *vv <= max_ele.v.val_int) {
        return 0;
    }

    return -1;
}

static int valid_string(neu_schema_valid_t *v, void *value, char *name)
{
    neu_json_elem_t valid_ele = { 0 };
    neu_json_elem_t len_ele   = { .name = "length", .t = NEU_JSON_INT };
    char *          vv        = (char *) value;

    if (!valid_init(v->json, &valid_ele, name)) {
        return -1;
    }

    if (neu_json_decode_value(valid_ele.v.object, &len_ele) != 0) {
        return 0;
    }

    if ((uint64_t) len_ele.v.val_int >= strlen(vv)) {
        return 0;
    }

    return -1;
}

static int valid_enum(neu_schema_valid_t *v, void *value, char *name)
{
    neu_json_elem_t valid_ele     = { 0 };
    char *          valid_content = NULL;
    int64_t *       vv            = (int64_t *) value;
    int             size          = 0;

    if (!valid_init(v->json, &valid_ele, name)) {
        return -1;
    }

    neu_json_encode(valid_ele.v.object, &valid_content);
    size = neu_json_decode_array_size(valid_content, "value");
    if (size <= 0) {
        return -1;
    }

    for (int i = 0; i < size; i++) {
        neu_json_elem_t ve = { .name = NULL, .t = NEU_JSON_INT };
        if (neu_json_decode_array(valid_content, "value", i, 1, &ve) == 0) {
            if (ve.v.val_int == *vv) {
                free(valid_content);
                return 0;
            }
        }
    }

    free(valid_content);
    return -1;
}