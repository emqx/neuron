/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
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

#include <stdlib.h>
#include <string.h>

#include "define.h"
#include "errcodes.h"

#include "mqtt_v2_write.h"

typedef int (*write_decoder_t)(json_t *root, mqtt_v2_write_req_t *req);

static int decode_neuron(json_t *root, mqtt_v2_write_req_t *req);

// decoders of the downlink message formats, selected by `version`
static const struct {
    const char *    version;
    write_decoder_t decode;
} decoders[] = {
    { "1.0", decode_neuron },
    { "neuron", decode_neuron },
};

static int decode_array(json_t *json, neu_dvalue_t *value)
{
    size_t  n      = json_array_size(json);
    size_t  i      = 0;
    json_t *item   = NULL;
    size_t  n_bool = 0;
    size_t  n_int  = 0;
    size_t  n_num  = 0;
    size_t  n_str  = 0;

    if (0 == n) {
        return NEU_ERR_PARAM_IS_WRONG;
    }

    json_array_foreach(json, i, item)
    {
        n_bool += json_is_boolean(item) ? 1 : 0;
        n_int += json_is_integer(item) ? 1 : 0;
        n_num += json_is_number(item) ? 1 : 0;
        n_str += json_is_string(item) ? 1 : 0;
    }

    if (n == n_bool) {
        value->type               = NEU_TYPE_ARRAY_BOOL;
        value->value.bools.length = n;
        value->value.bools.bools  = calloc(n, sizeof(bool));
        if (NULL == value->value.bools.bools) {
            return NEU_ERR_EINTERNAL;
        }
        json_array_foreach(json, i, item)
        {
            value->value.bools.bools[i] = json_is_true(item);
        }
    } else if (n == n_int) {
        value->type              = NEU_TYPE_ARRAY_INT64;
        value->value.i64s.length = n;
        value->value.i64s.i64s   = calloc(n, sizeof(int64_t));
        if (NULL == value->value.i64s.i64s) {
            return NEU_ERR_EINTERNAL;
        }
        json_array_foreach(json, i, item)
        {
            value->value.i64s.i64s[i] = json_integer_value(item);
        }
    } else if (n == n_num) {
        value->type              = NEU_TYPE_ARRAY_DOUBLE;
        value->value.f64s.length = n;
        value->value.f64s.f64s   = calloc(n, sizeof(double));
        if (NULL == value->value.f64s.f64s) {
            return NEU_ERR_EINTERNAL;
        }
        json_array_foreach(json, i, item)
        {
            value->value.f64s.f64s[i] = json_number_value(item);
        }
    } else if (n == n_str) {
        value->type              = NEU_TYPE_ARRAY_STRING;
        value->value.strs.length = 0;
        value->value.strs.strs   = calloc(n, sizeof(char *));
        if (NULL == value->value.strs.strs) {
            return NEU_ERR_EINTERNAL;
        }
        json_array_foreach(json, i, item)
        {
            value->value.strs.strs[i] = strdup(json_string_value(item));
            if (NULL == value->value.strs.strs[i]) {
                return NEU_ERR_EINTERNAL;
            }
            value->value.strs.length = i + 1;
        }
    } else {
        return NEU_ERR_PARAM_IS_WRONG;
    }

    return NEU_ERR_SUCCESS;
}

static int decode_value(json_t *json, neu_dvalue_t *value)
{
    switch (json_typeof(json)) {
    case JSON_INTEGER:
        value->type      = NEU_TYPE_INT64;
        value->value.i64 = json_integer_value(json);
        return NEU_ERR_SUCCESS;
    case JSON_REAL:
        value->type      = NEU_TYPE_DOUBLE;
        value->value.d64 = json_real_value(json);
        return NEU_ERR_SUCCESS;
    case JSON_TRUE:
    case JSON_FALSE:
        value->type          = NEU_TYPE_BOOL;
        value->value.boolean = json_is_true(json);
        return NEU_ERR_SUCCESS;
    case JSON_STRING:
        if (NEU_VALUE_SIZE <= json_string_length(json)) {
            return NEU_ERR_PARAM_IS_WRONG;
        }
        value->type = NEU_TYPE_STRING;
        strcpy(value->value.str, json_string_value(json));
        return NEU_ERR_SUCCESS;
    case JSON_ARRAY:
        return decode_array(json, value);
    case JSON_OBJECT:
        value->type       = NEU_TYPE_CUSTOM;
        value->value.json = json_incref(json);
        return NEU_ERR_SUCCESS;
    default:
        return NEU_ERR_PARAM_IS_WRONG;
    }
}

// {"params": {"group": "group", "values": {"tag1": 1, "tag2": true}}}
static int decode_neuron(json_t *root, mqtt_v2_write_req_t *req)
{
    json_t *    params = json_object_get(root, "params");
    json_t *    values = json_object_get(params, "values");
    json_t *    group  = json_object_get(params, "group");
    const char *key    = NULL;
    json_t *    value  = NULL;
    int         rv     = NEU_ERR_SUCCESS;

    if (!json_is_object(values) || 0 == json_object_size(values)) {
        return NEU_ERR_BODY_IS_WRONG;
    }

    req->values = calloc(json_object_size(values), sizeof(*req->values));
    if (NULL == req->values) {
        return NEU_ERR_EINTERNAL;
    }

    json_object_foreach(values, key, value)
    {
        mqtt_v2_write_value_t *v = &req->values[req->n_value];

        v->tag = strdup(key);
        if (NULL == v->tag) {
            return NEU_ERR_EINTERNAL;
        }
        ++req->n_value;

        if (NEU_TAG_NAME_LEN <= strlen(key)) {
            v->error = NEU_ERR_TAG_NAME_TOO_LONG;
        } else {
            v->error = decode_value(value, &v->value);
        }

        if (NEU_ERR_SUCCESS == rv) {
            rv = v->error;
        }
    }

    if (NULL != group && !json_is_null(group)) {
        if (!json_is_string(group) || 0 == json_string_length(group) ||
            NEU_GROUP_NAME_LEN <= json_string_length(group)) {
            return NEU_ERR_PARAM_IS_WRONG;
        }
        req->group = strdup(json_string_value(group));
        if (NULL == req->group) {
            return NEU_ERR_EINTERNAL;
        }
    }

    return rv;
}

int mqtt_v2_write_req_decode(const uint8_t *payload, uint32_t len,
                             mqtt_v2_write_req_t *req)
{
    int             rv     = NEU_ERR_SUCCESS;
    write_decoder_t decode = decode_neuron;
    json_t *        root   = NULL;

    memset(req, 0, sizeof(*req));

    root = json_loadb((const char *) payload, len, 0, NULL);
    if (!json_is_object(root)) {
        json_decref(root);
        return NEU_ERR_BODY_IS_WRONG;
    }

    json_t *id = json_object_get(root, "id");
    if (NULL != id && !json_is_null(id)) {
        req->id = json_incref(id);
    }

    json_t *version = json_object_get(root, "version");
    if (NULL != version && !json_is_null(version)) {
        if (!json_is_string(version)) {
            rv = NEU_ERR_PARAM_IS_WRONG;
            goto end;
        }

        req->version = strdup(json_string_value(version));
        decode       = NULL;
        for (size_t i = 0; i < sizeof(decoders) / sizeof(decoders[0]); ++i) {
            if (0 == strcmp(req->version, decoders[i].version)) {
                decode = decoders[i].decode;
                break;
            }
        }

        if (NULL == decode) {
            rv = NEU_ERR_PARAM_IS_WRONG;
            goto end;
        }
    }

    rv = decode(root, req);

end:
    json_decref(root);
    return rv;
}

void mqtt_v2_write_req_fini(mqtt_v2_write_req_t *req)
{
    for (size_t i = 0; i < req->n_value; ++i) {
        free(req->values[i].tag);
        neu_free_dvalue(&req->values[i].value);
    }
    free(req->values);
    free(req->group);
    free(req->version);
    json_decref(req->id);
    memset(req, 0, sizeof(*req));
}

void mqtt_v2_write_values_fail(mqtt_v2_write_value_t *values, size_t n_value,
                               int error)
{
    for (size_t i = 0; i < n_value; ++i) {
        if (NEU_ERR_SUCCESS == values[i].error) {
            values[i].error = error;
        }
    }
}

char *mqtt_v2_write_resp_encode(json_t *id, const char *version, int error,
                                const mqtt_v2_write_value_t *values,
                                size_t                       n_value)
{
    json_t *root = json_object();
    json_t *tags = json_object();
    char *  out  = NULL;

    if (NULL == root || NULL == tags) {
        json_decref(root);
        json_decref(tags);
        return NULL;
    }

    if (NULL != id) {
        json_object_set(root, "id", id);
    }
    if (NULL != version) {
        json_object_set_new(root, "version", json_string(version));
    }
    json_object_set_new(root, "error", json_integer(error));

    for (size_t i = 0; i < n_value; ++i) {
        json_object_set_new(tags, values[i].tag, json_integer(values[i].error));
    }
    json_object_set_new(root, "tags", tags);

    out = json_dumps(root, JSON_REAL_PRECISION(16));
    json_decref(root);
    return out;
}
