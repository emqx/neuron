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

#include <jansson.h>

#include "json_rw.h"

int wrap_tag_data(neu_json_read_resp_tag_t *json_tag, neu_int_val_t *int_val,
                  neu_datatag_table_t *datatag_table)
{
    neu_data_val_t *val;

    if (NULL == json_tag || NULL == int_val) {
        return NEU_ERR_EINVAL;
    }

    val              = int_val->val;
    neu_dtype_e type = neu_dvalue_get_value_type(val);

    neu_datatag_t *tag = neu_datatag_tbl_get(datatag_table, int_val->key);

    if (tag == NULL) {
        return NEU_ERR_TAG_NOT_EXIST;
    }

    json_tag->name  = tag->name;
    json_tag->error = NEU_ERR_SUCCESS;

    switch (type) {
    case NEU_DTYPE_ERRORCODE: {
        int32_t value;
        neu_dvalue_get_errorcode(val, &value);
        json_tag->t             = NEU_JSON_INT;
        json_tag->value.val_int = value;
        json_tag->error         = value;
        break;
    }
    case NEU_DTYPE_BYTE: {
        uint8_t value;
        neu_dvalue_get_uint8(val, &value);
        json_tag->t             = NEU_JSON_INT;
        json_tag->value.val_int = value;
        break;
    }
    case NEU_DTYPE_INT8: {
        int8_t value;
        neu_dvalue_get_int8(val, &value);
        json_tag->t             = NEU_JSON_INT;
        json_tag->value.val_int = value;
        break;
    }
    case NEU_DTYPE_INT16: {
        int16_t value;
        neu_dvalue_get_int16(val, &value);
        json_tag->t             = NEU_JSON_INT;
        json_tag->value.val_int = value;
        break;
    }
    case NEU_DTYPE_INT32: {
        int32_t value;
        neu_dvalue_get_int32(val, &value);
        json_tag->t             = NEU_JSON_INT;
        json_tag->value.val_int = value;
        break;
    }
    case NEU_DTYPE_INT64: {
        int64_t value;
        neu_dvalue_get_int64(val, &value);
        json_tag->t             = NEU_JSON_INT;
        json_tag->value.val_int = value;
        break;
    }
    case NEU_DTYPE_UINT8: {
        uint8_t value;
        neu_dvalue_get_uint8(val, &value);
        json_tag->t             = NEU_JSON_INT;
        json_tag->value.val_int = value;
        break;
    }
    case NEU_DTYPE_UINT16: {
        uint16_t value;
        neu_dvalue_get_uint16(val, &value);
        json_tag->t             = NEU_JSON_INT;
        json_tag->value.val_int = value;
        break;
    }
    case NEU_DTYPE_UINT32: {
        uint32_t value;
        neu_dvalue_get_uint32(val, &value);
        json_tag->t             = NEU_JSON_INT;
        json_tag->value.val_int = value;
        break;
    }
    case NEU_DTYPE_UINT64: {
        uint64_t value;
        neu_dvalue_get_uint64(val, &value);
        json_tag->t             = NEU_JSON_INT;
        json_tag->value.val_int = value;
        break;
    }
    case NEU_DTYPE_FLOAT: {
        float value;
        neu_dvalue_get_float(val, &value);
        json_tag->t               = NEU_JSON_FLOAT;
        json_tag->value.val_float = value;
        break;
    }
    case NEU_DTYPE_DOUBLE: {
        double value;
        neu_dvalue_get_double(val, &value);
        json_tag->t                = NEU_JSON_DOUBLE;
        json_tag->value.val_double = value;
        break;
    }
    case NEU_DTYPE_BOOL: {
        bool value;
        neu_dvalue_get_bool(val, &value);
        json_tag->t              = NEU_JSON_BOOL;
        json_tag->value.val_bool = value;
        break;
    }
    case NEU_DTYPE_BIT: {
        uint8_t value;
        neu_dvalue_get_bit(val, &value);
        json_tag->t             = NEU_JSON_BIT;
        json_tag->value.val_bit = value;
        break;
    }
    case NEU_DTYPE_CSTR: {
        char *value;
        neu_dvalue_get_ref_cstr(val, &value);
        json_tag->t             = NEU_JSON_STR;
        json_tag->value.val_str = value;
        break;
    }
    default:
        return NEU_ERR_ENOTSUP;
    }

    return 0;
}

int json_encode_read_resp_header(void *json_object, void *param)
{
    int                      ret    = 0;
    json_read_resp_header_t *header = param;

    neu_json_elem_t resp_elems[] = { {
                                         .name      = "node_name",
                                         .t         = NEU_JSON_STR,
                                         .v.val_str = header->node_name,
                                     },
                                     {
                                         .name      = "group_name",
                                         .t         = NEU_JSON_STR,
                                         .v.val_str = header->group_name,
                                     },
                                     {
                                         .name      = "timestamp",
                                         .t         = NEU_JSON_INT,
                                         .v.val_int = header->timestamp,
                                     } };
    ret = neu_json_encode_field(json_object, resp_elems,
                                NEU_JSON_ELEM_SIZE(resp_elems));

    return ret;
}

int json_encode_read_resp_tags(void *json_object, void *param)
{
    int                    ret           = 0;
    json_read_resp_tags_t *resp_tags     = param;
    neu_datatag_table_t *  datatag_table = resp_tags->sender_datatag_table;
    neu_fixed_array_t *    array         = resp_tags->array;
    neu_int_val_t *        int_val       = NULL;
    void *                 values_object = NULL;
    void *                 errors_object = NULL;

    if (0 == array->length) {
        return -1;
    }

    values_object = neu_json_encode_new();
    if (NULL == values_object) {
        return -1;
    }
    errors_object = neu_json_encode_new();
    if (NULL == values_object) {
        json_decref(values_object);
        return -1;
    }

    for (uint32_t i = 0; i < array->length; i++) {
        neu_json_read_resp_tag_t json_tag = { 0 };
        int_val                           = neu_fixed_array_get(array, i);

        if (0 != wrap_tag_data(&json_tag, int_val, datatag_table)) {
            continue; // ignore
        }

        neu_json_elem_t tag_elem = {
            .name = json_tag.name,
            .t    = json_tag.t,
            .v    = json_tag.value,
        };

        ret = neu_json_encode_field((0 != json_tag.error) ? errors_object
                                                          : values_object,
                                    &tag_elem, 1);
        if (0 != ret) {
            json_decref(errors_object);
            json_decref(values_object);
            return ret;
        }
    }

    neu_json_elem_t resp_elems[] = { {
                                         .name         = "values",
                                         .t            = NEU_JSON_OBJECT,
                                         .v.val_object = values_object,
                                     },
                                     {
                                         .name         = "errors",
                                         .t            = NEU_JSON_OBJECT,
                                         .v.val_object = errors_object,

                                     } };
    // steals `values_object` and `errors_object`
    ret = neu_json_encode_field(json_object, resp_elems,
                                NEU_JSON_ELEM_SIZE(resp_elems));

    return ret;
}

int json_encode_read_resp(void *json_object, void *param)
{
    int               ret  = 0;
    json_read_resp_t *resp = param;

    neu_fixed_array_t *array = NULL;
    neu_dvalue_get_ref_array(resp->data_val, &array);
    assert(NULL != array);

    if (0 >= array->length) {
        return -1;
    }

    json_read_resp_header_t header = {
        .group_name = (char *) neu_taggrp_cfg_get_name(resp->grp_config),
        .node_name  = (char *) resp->sender_name,
        .timestamp  = time_ms()
    };

    ret = json_encode_read_resp_header(json_object, &header);
    if (0 != ret) {
        return ret;
    }

    json_read_resp_tags_t tags = {
        .sender_id            = resp->sender_id,
        .sender_datatag_table = resp->sender_datatag_table,
        .array                = array,
    };
    ret = json_encode_read_resp_tags(json_object, &tags);

    return ret;
}
