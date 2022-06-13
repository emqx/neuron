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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "read_write.h"
#include "utils/log.h"

static uint64_t current_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ms = tv.tv_sec;
    return ms * 1000 + tv.tv_usec / 1000;
}

int command_rw_read_once_request(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                                 neu_json_read_req_t *req)
{
    neu_reqresp_head_t header = { 0 };
    header.ctx                = mqtt;
    header.type               = NEU_REQ_READ_GROUP;
    neu_req_read_group_t cmd  = { 0 };
    strcpy(cmd.driver, req->node);
    strcpy(cmd.group, req->group);
    int ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        return -1;
    }

    return 0;
}

static void wrap_read_response_json(neu_resp_tag_value_t *tags, uint16_t len,
                                    neu_json_read_resp_t *json)
{
    json->n_tag = len;
    if (0 < json->n_tag) {
        json->tags = (neu_json_read_resp_tag_t *) calloc(
            json->n_tag, sizeof(neu_json_read_resp_tag_t));
    }

    for (int i = 0; i < json->n_tag; i++) {
        neu_resp_tag_value_t *tag  = &tags[i];
        neu_type_e            type = tag->value.type;
        json->tags[i].name         = tag->tag;
        json->tags[i].error        = NEU_ERR_SUCCESS;

        switch (type) {
        case NEU_TYPE_ERROR:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.i32;
            json->tags[i].error         = tag->value.value.i32;
            break;
        case NEU_TYPE_UINT8:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.u8;
            break;
        case NEU_TYPE_INT8:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.i8;
            break;
        case NEU_TYPE_INT16:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.i16;
            break;
        case NEU_TYPE_INT32:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.i32;
            break;
        case NEU_TYPE_INT64:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.i64;
            break;
        case NEU_TYPE_UINT16:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.u16;
            break;
        case NEU_TYPE_UINT32:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.u32;
            break;
        case NEU_TYPE_UINT64:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.u64;
            break;
        case NEU_TYPE_FLOAT:
            json->tags[i].t               = NEU_JSON_FLOAT;
            json->tags[i].value.val_float = tag->value.value.f32;
            break;
        case NEU_TYPE_DOUBLE:
            json->tags[i].t                = NEU_JSON_DOUBLE;
            json->tags[i].value.val_double = tag->value.value.d64;
            break;
        case NEU_TYPE_BOOL:
            json->tags[i].t              = NEU_JSON_BOOL;
            json->tags[i].value.val_bool = tag->value.value.boolean;
            break;
        case NEU_TYPE_BIT:
            json->tags[i].t             = NEU_JSON_BIT;
            json->tags[i].value.val_bit = tag->value.value.u8;
            break;
        case NEU_TYPE_STRING:
            json->tags[i].t             = NEU_JSON_STR;
            json->tags[i].value.val_str = tag->value.value.str;
            break;
        default:
            break;
        }
    }
}

static void clean_read_response_json(neu_json_read_resp_t *json)
{
    if (NULL == json) {
        return;
    }

    if (NULL == json->tags) {
        return;
    }

    free(json->tags);
}

char *command_rw_read_once_response(neu_reqresp_head_t *   head,
                                    neu_resp_read_group_t *data, int format)
{
    UNUSED(format);

    neu_resp_tag_value_t *tags     = data->tags;
    uint16_t              len      = data->n_tag;
    char *                json_str = NULL;
    neu_json_read_resp_t  json     = { 0 };
    wrap_read_response_json(tags, len, &json);
    neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp, head->ctx,
                              neu_json_encode_mqtt_resp, &json_str);
    clean_read_response_json(&json);
    return json_str;
}

char *command_rw_read_periodic_response(neu_reqresp_trans_data_t *data,
                                        int                       format)
{
    neu_resp_tag_value_t *   tags     = data->tags;
    uint16_t                 len      = data->n_tag;
    char *                   json_str = NULL;
    neu_json_read_periodic_t header   = { .group     = (char *) data->group,
                                        .node      = (char *) data->driver,
                                        .timestamp = current_time() };
    neu_json_read_resp_t     json     = { 0 };
    wrap_read_response_json(tags, len, &json);

    if (0 == format) { // values
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp1, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else if (1 == format) { // tags
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else {
        clean_read_response_json(&json);
        return NULL;
    }

    clean_read_response_json(&json);
    return json_str;
}

// static int write_command(neu_plugin_t *plugin, uint32_t dest_node_id,
// const char *    group_config_name,
// neu_data_val_t *write_val, uint32_t req_id)
//{
// neu_taggrp_config_t *config;
// config =
// neu_system_find_group_config(plugin, dest_node_id, group_config_name);
// if (NULL == config) {
// return -2;
//}

// neu_plugin_send_write_cmd(plugin, req_id, dest_node_id, config, write_val);
// return 0;
//}

int command_rw_write_request(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                             neu_json_write_req_t *write_req, uint32_t req_id)
{
    (void) plugin;
    (void) mqtt;
    (void) write_req;
    (void) req_id;
    // zlog_info(neuron, "WRITE uuid:%s, group config name:%s", mqtt->uuid,
    // write_req->group_name);
    // neu_node_id_t node_id =
    // neu_plugin_get_node_id_by_node_name(plugin, write_req->node_name);
    // if (node_id == 0) {
    // zlog_error(neuron, "node %s does not exist", write_req->node_name);
    // return -1;
    //}

    // neu_data_val_t *   write_val;
    // neu_fixed_array_t *array;
    // write_val = neu_dvalue_unit_new();
    // if (write_val == NULL) {
    // zlog_error(neuron, "Failed to allocate data value for write data");
    // return -1;
    //}

    // array = neu_fixed_array_new(1, sizeof(neu_int_val_t));
    // if (array == NULL) {
    // zlog_error(neuron, "Failed to allocate array for write data");
    // neu_dvalue_free(write_val);
    // return -2;
    //}

    // neu_datatag_table_t *table = neu_system_get_datatags_table(plugin,
    // node_id);

    // neu_int_val_t        int_val;
    // neu_data_val_t *     val   = NULL;
    // union neu_json_value value = write_req->value;
    // neu_datatag_t *      tag =
    // neu_datatag_tbl_get_by_name(table, write_req->tag_name);

    // if (tag == NULL) {
    // return -1;
    //}

    // neu_datatag_id_t id = tag->id;

    // switch (tag->type) {
    // case NEU_DTYPE_UINT8:
    // val = neu_dvalue_new(NEU_DTYPE_UINT8);
    // neu_dvalue_set_uint8(val, (uint8_t) value.val_int);
    // break;
    // case NEU_DTYPE_INT8:
    // val = neu_dvalue_new(NEU_DTYPE_INT8);
    // neu_dvalue_set_int8(val, (int8_t) value.val_int);
    // break;
    // case NEU_DTYPE_UINT16:
    // val = neu_dvalue_new(NEU_DTYPE_UINT16);
    // neu_dvalue_set_uint16(val, (uint16_t) value.val_int);
    // break;
    // case NEU_DTYPE_INT16:
    // val = neu_dvalue_new(NEU_DTYPE_INT16);
    // neu_dvalue_set_int16(val, (int16_t) value.val_int);
    // break;
    // case NEU_DTYPE_INT32:
    // val = neu_dvalue_new(NEU_DTYPE_INT32);
    // neu_dvalue_set_int32(val, (int32_t) value.val_int);
    // break;
    // case NEU_DTYPE_UINT32:
    // val = neu_dvalue_new(NEU_DTYPE_UINT32);
    // neu_dvalue_set_uint32(val, (uint32_t) value.val_int);
    // break;
    // case NEU_DTYPE_INT64:
    // val = neu_dvalue_new(NEU_DTYPE_INT64);
    // neu_dvalue_set_int64(val, (int64_t) value.val_int);
    // break;
    // case NEU_DTYPE_UINT64:
    // val = neu_dvalue_new(NEU_DTYPE_UINT64);
    // neu_dvalue_set_uint64(val, (uint64_t) value.val_int);
    // break;
    // case NEU_DTYPE_FLOAT:
    // val = neu_dvalue_new(NEU_DTYPE_FLOAT);
    // neu_dvalue_set_float(val, (float) value.val_double);
    // break;
    // case NEU_DTYPE_DOUBLE:
    // val = neu_dvalue_new(NEU_DTYPE_DOUBLE);
    // neu_dvalue_set_double(val, value.val_double);
    // break;
    // case NEU_DTYPE_BIT:
    // val = neu_dvalue_new(NEU_DTYPE_BIT);
    // neu_dvalue_set_bit(val, (uint8_t) value.val_int);
    // break;
    // case NEU_DTYPE_BOOL:
    // val = neu_dvalue_new(NEU_DTYPE_BOOL);
    // neu_dvalue_set_bool(val, value.val_bool);
    // break;
    // case NEU_DTYPE_CSTR:
    // val = neu_dvalue_new(NEU_DTYPE_CSTR);
    // neu_dvalue_set_cstr(val, value.val_str);
    // break;
    // default:
    // break;
    //}

    // neu_int_val_init(&int_val, id, val);
    // neu_fixed_array_set(array, 0, (void *) &int_val);

    // neu_dvalue_init_move_array(write_val, NEU_DTYPE_INT_VAL, array);
    // write_command(plugin, node_id, write_req->group_name, write_val, req_id);
    return 0;
}

// char *command_rw_write_response(neu_json_mqtt_t *parse_header,
// neu_data_val_t * resp_val)
//{
// neu_int_val_t *       iv    = NULL;
// neu_json_error_resp_t error = { 0 };
// neu_fixed_array_t *   array;

// neu_dvalue_get_ref_array(resp_val, &array);

// iv = (neu_int_val_t *) neu_fixed_array_get(array, 0);
// assert(neu_dvalue_get_value_type(iv->val) == NEU_DTYPE_ERRORCODE);
// neu_dvalue_get_errorcode(iv->val, (int32_t *) &error.error);

// char *json_str = NULL;
// neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, parse_header,
// neu_json_encode_mqtt_resp, &json_str);

// return json_str;
//}
