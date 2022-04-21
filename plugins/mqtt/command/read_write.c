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
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "read_write.h"

static uint64_t current_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ms = tv.tv_sec;
    return ms * 1000 + tv.tv_usec / 1000;
}

int command_rw_read_once_request(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                                 neu_json_read_req_t *req, uint32_t req_id)
{
    neu_node_id_t node_id =
        neu_plugin_get_node_id_by_node_name(plugin, req->node_name);
    if (node_id == 0) {
        log_debug("node %s does not exist", req->node_name);
        return -1;
    }
    log_info("READ uuid:%s, node id:%u", mqtt->uuid, node_id);
    neu_taggrp_config_t *config =
        neu_system_find_group_config(plugin, node_id, req->group_name);
    if (NULL == config) {
        log_debug("The requested config does not exist");
        return -2;
    }

    neu_plugin_send_read_cmd(plugin, req_id, node_id, config);
    return 0;
}

static int wrap_read_response_json_object(neu_fixed_array_t *   array,
                                          neu_json_read_resp_t *json,
                                          neu_plugin_t *        plugin,
                                          uint32_t              node_id)
{
    neu_int_val_t *      int_val;
    neu_data_val_t *     val;
    neu_datatag_table_t *datatag_table =
        neu_system_get_datatags_table(plugin, node_id);
    json->n_tag = array->length;

    if (0 == json->n_tag) {
        return -1;
    }

    json->tags = (neu_json_read_resp_tag_t *) calloc(
        array->length, sizeof(neu_json_read_resp_tag_t));

    for (uint32_t i = 0; i < array->length; i++) {
        int_val          = neu_fixed_array_get(array, i);
        val              = int_val->val;
        neu_dtype_e type = neu_dvalue_get_value_type(val);

        neu_datatag_t *tag = neu_datatag_tbl_get(datatag_table, int_val->key);

        if (tag == NULL) {
            continue;
        }

        json->tags[i].name  = tag->name;
        json->tags[i].error = NEU_ERR_SUCCESS;

        switch (type) {
        case NEU_DTYPE_ERRORCODE: {
            int32_t value;
            neu_dvalue_get_errorcode(val, &value);
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = value;
            json->tags[i].error         = value;
            break;
        }
        case NEU_DTYPE_BYTE: {
            uint8_t value;
            neu_dvalue_get_uint8(val, &value);
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = value;
            break;
        }
        case NEU_DTYPE_INT8: {
            int8_t value;
            neu_dvalue_get_int8(val, &value);
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = value;
            break;
        }
        case NEU_DTYPE_INT16: {
            int16_t value;
            neu_dvalue_get_int16(val, &value);
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = value;
            break;
        }
        case NEU_DTYPE_INT32: {
            int32_t value;
            neu_dvalue_get_int32(val, &value);
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = value;
            break;
        }
        case NEU_DTYPE_INT64: {
            int64_t value;
            neu_dvalue_get_int64(val, &value);
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = value;
            break;
        }
        case NEU_DTYPE_UINT8: {
            uint8_t value;
            neu_dvalue_get_uint8(val, &value);
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = value;
            break;
        }
        case NEU_DTYPE_UINT16: {
            uint16_t value;
            neu_dvalue_get_uint16(val, &value);
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = value;
            break;
        }
        case NEU_DTYPE_UINT32: {
            uint32_t value;
            neu_dvalue_get_uint32(val, &value);
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = value;
            break;
        }
        case NEU_DTYPE_UINT64: {
            uint64_t value;
            neu_dvalue_get_uint64(val, &value);
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = value;
            break;
        }
        case NEU_DTYPE_FLOAT: {
            float value;
            neu_dvalue_get_float(val, &value);
            json->tags[i].t               = NEU_JSON_FLOAT;
            json->tags[i].value.val_float = value;
            break;
        }
        case NEU_DTYPE_DOUBLE: {
            double value;
            neu_dvalue_get_double(val, &value);
            json->tags[i].t                = NEU_JSON_DOUBLE;
            json->tags[i].value.val_double = value;
            break;
        }
        case NEU_DTYPE_BOOL: {
            bool value;
            neu_dvalue_get_bool(val, &value);
            json->tags[i].t              = NEU_JSON_BOOL;
            json->tags[i].value.val_bool = value;
            break;
        }
        case NEU_DTYPE_BIT: {
            uint8_t value;
            neu_dvalue_get_bit(val, &value);
            json->tags[i].t             = NEU_JSON_BIT;
            json->tags[i].value.val_bit = value;
            break;
        }
        case NEU_DTYPE_CSTR: {
            char *value;
            neu_dvalue_get_ref_cstr(val, &value);
            json->tags[i].t             = NEU_JSON_STR;
            json->tags[i].value.val_str = value;
            break;
        }
        default:
            break;
        }
    }

    return 0;
}

static void clean_read_response_json_object(neu_json_read_resp_t *json)
{
    if (NULL == json) {
        return;
    }

    if (NULL == json->tags) {
        return;
    }

    free(json->tags);
}

char *command_rw_read_once_response(neu_plugin_t *plugin, uint32_t node_id,
                                    neu_json_mqtt_t *parse_header,
                                    neu_data_val_t * resp_val)
{
    UNUSED(plugin);

    neu_fixed_array_t *array;
    int                rc = neu_dvalue_get_ref_array(resp_val, &array);
    if (0 != rc) {
        log_info("Get array ref error");
        return NULL;
    }

    if (0 >= array->length) {
        return NULL;
    }

    char *json_str = NULL;

    neu_json_read_resp_t json;
    memset(&json, 0, sizeof(neu_json_read_resp_t));
    rc = wrap_read_response_json_object(array, &json, plugin, node_id);
    if (0 != rc) {
        return NULL;
    }

    rc = neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp,
                                   parse_header, neu_json_encode_mqtt_resp,
                                   &json_str);
    clean_read_response_json_object(&json);
    if (0 != rc) {
        log_info("Json string parse error:%d", rc);
        return NULL;
    }

    return json_str;
}

char *command_rw_read_periodic_response(neu_plugin_t *plugin, uint64_t sender,
                                        const char *         node_name,
                                        neu_taggrp_config_t *config,
                                        neu_data_val_t *     resp_val,
                                        int                  upload_format)
{
    UNUSED(plugin);

    neu_fixed_array_t *array = NULL;
    neu_dvalue_get_ref_array(resp_val, &array);
    assert(NULL != array);

    if (0 >= array->length) {
        return NULL;
    }

    char *                   json_str = NULL;
    neu_json_read_periodic_t header   = {
        .group_name = (char *) neu_taggrp_cfg_get_name(config),
        .node_name  = (char *) node_name,
        .timestamp  = current_time()
    };

    neu_json_read_resp_t json = { 0 };
    wrap_read_response_json_object(array, &json, plugin, (uint32_t) sender);

    if (0 == upload_format) { // values
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp1, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else if (1 == upload_format) { // tags
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else {
        clean_read_response_json_object(&json);
        return NULL;
    }

    clean_read_response_json_object(&json);
    return json_str;
}

static int write_command(neu_plugin_t *plugin, uint32_t dest_node_id,
                         const char *    group_config_name,
                         neu_data_val_t *write_val, uint32_t req_id)
{
    neu_taggrp_config_t *config;
    config =
        neu_system_find_group_config(plugin, dest_node_id, group_config_name);
    if (NULL == config) {
        return -2;
    }

    neu_plugin_send_write_cmd(plugin, req_id, dest_node_id, config, write_val);
    return 0;
}

int command_rw_write_request(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                             neu_json_write_req_t *write_req, uint32_t req_id)
{
    log_info("WRITE uuid:%s, group config name:%s", mqtt->uuid,
             write_req->group_name);
    neu_node_id_t node_id =
        neu_plugin_get_node_id_by_node_name(plugin, write_req->node_name);
    if (node_id == 0) {
        log_error("node %s does not exist", write_req->node_name);
        return -1;
    }

    neu_data_val_t *   write_val;
    neu_fixed_array_t *array;
    write_val = neu_dvalue_unit_new();
    if (write_val == NULL) {
        log_error("Failed to allocate data value for write data");
        return -1;
    }

    array = neu_fixed_array_new(1, sizeof(neu_int_val_t));
    if (array == NULL) {
        log_error("Failed to allocate array for write data");
        neu_dvalue_free(write_val);
        return -2;
    }

    neu_datatag_table_t *table = neu_system_get_datatags_table(plugin, node_id);

    neu_int_val_t        int_val;
    neu_data_val_t *     val   = NULL;
    union neu_json_value value = write_req->value;
    neu_datatag_t *      tag =
        neu_datatag_tbl_get_by_name(table, write_req->tag_name);

    if (tag == NULL) {
        return -1;
    }

    neu_datatag_id_t id = tag->id;

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

    neu_int_val_init(&int_val, id, val);
    neu_fixed_array_set(array, 0, (void *) &int_val);

    neu_dvalue_init_move_array(write_val, NEU_DTYPE_INT_VAL, array);
    write_command(plugin, node_id, write_req->group_name, write_val, req_id);
    return 0;
}

char *command_rw_write_response(neu_json_mqtt_t *parse_header,
                                neu_data_val_t * resp_val)
{
    neu_int_val_t *       iv    = NULL;
    neu_json_error_resp_t error = { 0 };
    neu_fixed_array_t *   array;

    neu_dvalue_get_ref_array(resp_val, &array);

    iv = (neu_int_val_t *) neu_fixed_array_get(array, 0);
    assert(neu_dvalue_get_value_type(iv->val) == NEU_DTYPE_ERRORCODE);
    neu_dvalue_get_errorcode(iv->val, (int32_t *) &error.error);

    char *json_str = NULL;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, parse_header,
                              neu_json_encode_mqtt_resp, &json_str);

    return json_str;
}
