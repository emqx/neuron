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

#include "read_write.h"

// static uint64_t current_time()
// {
//     struct timeval tv;
//     gettimeofday(&tv, NULL);
//     uint64_t ms = tv.tv_sec;
//     return ms * 1000 + tv.tv_usec / 1000;
// }

int command_rw_read_once_request(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                                 neu_json_read_req_t *req)
{
    plog_info(plugin, "read uuid:%s, group:%s, node:%s", mqtt->uuid, req->group,
              req->node);

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
            json->tags[i].precision       = tag->value.precision;
            break;
        case NEU_TYPE_DOUBLE:
            json->tags[i].t                = NEU_JSON_DOUBLE;
            json->tags[i].value.val_double = tag->value.value.d64;
            json->tags[i].precision        = tag->value.precision;
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

char *command_rw_read_once_response(neu_plugin_t *         plugin,
                                    neu_reqresp_head_t *   head,
                                    neu_resp_read_group_t *data, int format)
{
    UNUSED(format);

    neu_resp_tag_value_t *tags     = data->tags;
    uint16_t              len      = data->n_tag;
    char *                json_str = NULL;
    neu_json_read_resp_t  json     = { 0 };

    plog_info(plugin, "read resp uuid: %s",
              ((neu_json_mqtt_t *) head->ctx)->uuid);
    wrap_read_response_json(tags, len, &json);
    neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp, head->ctx,
                              neu_json_encode_mqtt_resp, &json_str);
    clean_read_response_json(&json);
    neu_json_decode_mqtt_req_free(head->ctx);
    return json_str;
}

char *command_rw_read_periodic_response(neu_plugin_t *            plugin,
                                        neu_reqresp_trans_data_t *data,
                                        int                       format)
{
    UNUSED(plugin);

    neu_resp_tag_value_t *   tags     = data->tags;
    uint16_t                 len      = data->n_tag;
    char *                   json_str = NULL;
    neu_json_read_periodic_t header   = { .group     = (char *) data->group,
                                        .node      = (char *) data->driver,
                                        .timestamp = plugin->common.timestamp };
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

int command_rw_write_request(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                             neu_json_write_req_t *req)
{
    plog_info(plugin, "write uuid:%s, group:%s, node:%s", mqtt->uuid,
              req->group, req->node);

    neu_reqresp_head_t  header = { 0 };
    neu_req_write_tag_t cmd    = { 0 };

    header.ctx  = mqtt;
    header.type = NEU_REQ_WRITE_TAG;

    strcpy(cmd.driver, req->node);
    strcpy(cmd.group, req->group);
    strcpy(cmd.tag, req->tag);

    switch (req->t) {
    case NEU_JSON_INT:
        cmd.value.type      = NEU_TYPE_INT64;
        cmd.value.value.u64 = req->value.val_int;
        break;
    case NEU_JSON_STR:
        cmd.value.type = NEU_TYPE_STRING;
        strcpy(cmd.value.value.str, req->value.val_str);
        break;
    case NEU_JSON_DOUBLE:
        cmd.value.type      = NEU_TYPE_DOUBLE;
        cmd.value.value.d64 = req->value.val_double;
        break;
    case NEU_JSON_BOOL:
        cmd.value.type          = NEU_TYPE_BOOL;
        cmd.value.value.boolean = req->value.val_bool;
        break;
    default:
        assert(false);
        break;
    }

    int ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        return -1;
    }

    return 0;
}

char *command_rw_write_response(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                                neu_resp_error_t *data)
{
    neu_json_error_resp_t error    = { .error = data->error };
    char *                json_str = NULL;

    plog_info(plugin, "write resp uuid: %s",
              ((neu_json_mqtt_t *) head->ctx)->uuid);
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, head->ctx,
                              neu_json_encode_mqtt_resp, &json_str);
    neu_json_decode_mqtt_req_free(head->ctx);
    return json_str;
}
