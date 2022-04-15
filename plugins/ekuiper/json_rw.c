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

#include "neuron/tag.h"

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

int json_decode_write_req(char *buf, size_t len, json_write_req_t **result)
{
    int               ret      = 0;
    void *            json_obj = NULL;
    json_write_req_t *req      = calloc(1, sizeof(json_write_req_t));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_newb(buf, len);
    if (NULL == json_obj) {
        goto decode_fail;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node_name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group_name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "tag_name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "value",
            .t    = NEU_JSON_VALUE,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->node_name  = req_elems[0].v.val_str;
    req->group_name = req_elems[1].v.val_str;
    req->tag_name   = req_elems[2].v.val_str;
    req->t          = req_elems[3].t;
    req->value      = req_elems[3].v;

    *result = req;
    goto decode_exit;

decode_fail:
    if (req != NULL) {
        free(req);
    }
    ret = -1;

decode_exit:
    if (json_obj != NULL) {
        neu_json_decode_free(json_obj);
    }
    return ret;
}

void json_decode_write_req_free(json_write_req_t *req)
{
    if (NULL == req) {
        return;
    }

    free(req->group_name);
    free(req->node_name);
    free(req->tag_name);
    if (req->t == NEU_JSON_STR) {
        free(req->value.val_str);
    }

    free(req);
}

int unwrap_write_req_val(json_write_req_t *   req,
                         neu_datatag_table_t *datatag_table,
                         neu_data_val_t **    data_val)
{
    neu_data_val_t *     val   = NULL;
    union neu_json_value value = req->value;
    neu_datatag_t *      tag =
        neu_datatag_tbl_get_by_name(datatag_table, req->tag_name);

    if (NULL == data_val || tag == NULL) {
        return NEU_ERR_EINVAL;
    }

    switch (tag->type) {
    case NEU_DTYPE_UINT8:
        val = neu_dvalue_new(NEU_DTYPE_UINT8);
        neu_dvalue_set_uint8(val, (uint8_t) value.val_int);
        break;
    case NEU_DTYPE_INT8:
        val = neu_dvalue_new(NEU_DTYPE_INT8);
        neu_dvalue_set_int8(val, (int8_t) value.val_int);
        break;
    case NEU_DTYPE_UINT16:
        val = neu_dvalue_new(NEU_DTYPE_UINT16);
        neu_dvalue_set_uint16(val, (uint16_t) value.val_int);
        break;
    case NEU_DTYPE_INT16:
        val = neu_dvalue_new(NEU_DTYPE_INT16);
        neu_dvalue_set_int16(val, (int16_t) value.val_int);
        break;
    case NEU_DTYPE_INT32:
        val = neu_dvalue_new(NEU_DTYPE_INT32);
        neu_dvalue_set_int32(val, (int32_t) value.val_int);
        break;
    case NEU_DTYPE_UINT32:
        val = neu_dvalue_new(NEU_DTYPE_UINT32);
        neu_dvalue_set_uint32(val, (uint32_t) value.val_int);
        break;
    case NEU_DTYPE_INT64:
        val = neu_dvalue_new(NEU_DTYPE_INT64);
        neu_dvalue_set_int64(val, (int64_t) value.val_int);
        break;
    case NEU_DTYPE_UINT64:
        val = neu_dvalue_new(NEU_DTYPE_UINT64);
        neu_dvalue_set_uint64(val, (uint64_t) value.val_int);
        break;
    case NEU_DTYPE_FLOAT:
        val = neu_dvalue_new(NEU_DTYPE_FLOAT);
        neu_dvalue_set_float(val, (float) value.val_double);
        break;
    case NEU_DTYPE_DOUBLE:
        val = neu_dvalue_new(NEU_DTYPE_DOUBLE);
        neu_dvalue_set_double(val, value.val_double);
        break;
    case NEU_DTYPE_BIT:
        val = neu_dvalue_new(NEU_DTYPE_BIT);
        neu_dvalue_set_bit(val, (uint8_t) value.val_int);
        break;
    case NEU_DTYPE_BOOL:
        val = neu_dvalue_new(NEU_DTYPE_BOOL);
        neu_dvalue_set_bool(val, value.val_bool);
        break;
    case NEU_DTYPE_CSTR:
        val = neu_dvalue_new(NEU_DTYPE_CSTR);
        neu_dvalue_set_cstr(val, value.val_str);
        break;
    default:
        break;
    }

    *data_val = val;

    return 0;
}

int plugin_send_write_cmd_from_write_req(neu_plugin_t *plugin, uint32_t req_id,
                                         json_write_req_t *write_req)
{
    int rv = 0;

    neu_node_id_t node_id =
        neu_plugin_get_node_id_by_node_name(plugin, write_req->node_name);
    if (node_id == 0) {
        log_error("node %s does not exist", write_req->node_name);
        return NEU_ERR_NODE_NOT_EXIST;
    }

    neu_datatag_table_t *table = neu_system_get_datatags_table(plugin, node_id);
    neu_datatag_t *      tag =
        neu_datatag_tbl_get_by_name(table, write_req->tag_name);
    if (tag == NULL) {
        return NEU_ERR_TAG_NOT_EXIST;
    }

    neu_taggrp_config_t *config =
        neu_system_find_group_config(plugin, node_id, write_req->group_name);
    if (NULL == config) {
        return NEU_ERR_GRP_CONFIG_NOT_EXIST;
    }

    neu_data_val_t *write_val = neu_dvalue_unit_new();
    if (write_val == NULL) {
        log_error("Failed to allocate data value for write data");
        neu_taggrp_cfg_free(config);
        return NEU_ERR_ENOMEM;
    }

    neu_fixed_array_t *array = neu_fixed_array_new(1, sizeof(neu_int_val_t));
    if (array == NULL) {
        log_error("Failed to allocate array for write data");
        neu_dvalue_free(write_val);
        neu_taggrp_cfg_free(config);
        return NEU_ERR_ENOMEM;
    }

    neu_data_val_t *val = NULL;
    rv                  = unwrap_write_req_val(write_req, table, &val);
    if (rv < 0) {
        neu_fixed_array_free(array);
        neu_dvalue_free(write_val);
        neu_taggrp_cfg_free(config);
        return rv;
    }

    neu_datatag_id_t id = tag->id;

    neu_int_val_t int_val;
    neu_int_val_init(&int_val, id, val);
    neu_fixed_array_set(array, 0, (void *) &int_val);

    neu_dvalue_init_move_array(write_val, NEU_DTYPE_INT_VAL, array);

    neu_plugin_send_write_cmd(plugin, req_id, node_id, config, write_val);

    neu_taggrp_cfg_free(config);
    return rv;
}
