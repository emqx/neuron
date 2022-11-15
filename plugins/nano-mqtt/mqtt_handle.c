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

#include "errcodes.h"
#include "version.h"

#include "mqtt_handle.h"
#include "mqtt_plugin.h"

static char *generate_heartbeat_json(neu_plugin_t *plugin, UT_array *states)
{
    (void) plugin;
    char *                 version  = NEURON_VERSION;
    neu_json_states_head_t header   = { .version   = version,
                                      .timpstamp = global_timestamp };
    neu_json_states_t      json     = { 0 };
    char *                 json_str = NULL;

    json.n_state = utarray_len(states);
    json.states  = calloc(json.n_state, sizeof(neu_json_node_state_t));
    if (NULL == json.states) {
        return NULL;
    }

    utarray_foreach(states, neu_nodes_state_t *, state)
    {
        int index                  = utarray_eltidx(states, state);
        json.states[index].node    = state->node;
        json.states[index].link    = state->state.link;
        json.states[index].running = state->state.running;
    }

    neu_json_encode_with_mqtt(&json, neu_json_encode_states_resp, &header,
                              neu_json_encode_state_header_resp, &json_str);

    free(json.states);
    return json_str;
}

static int tag_values_to_json(neu_resp_tag_value_t *tags, uint16_t len,
                              neu_json_read_resp_t *json)
{
    if (0 == len) {
        return 0;
    }

    json->n_tag = len;
    json->tags  = (neu_json_read_resp_tag_t *) calloc(
        json->n_tag, sizeof(neu_json_read_resp_tag_t));
    if (NULL == json->tags) {
        return -1;
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

    return 0;
}

static char *generate_upload_json(neu_plugin_t *            plugin,
                                  neu_reqresp_trans_data_t *data,
                                  mqtt_upload_format_e      format)
{
    neu_resp_tag_value_t *   tags     = data->tags;
    uint16_t                 len      = data->n_tag;
    char *                   json_str = NULL;
    neu_json_read_periodic_t header   = { .group     = (char *) data->group,
                                        .node      = (char *) data->driver,
                                        .timestamp = global_timestamp };
    neu_json_read_resp_t     json     = { 0 };

    if (0 != tag_values_to_json(tags, len, &json)) {
        plog_error(plugin, "tag_values_to_json fail");
        return NULL;
    }

    if (MQTT_UPLOAD_FORMAT_VALUES == format) { // values
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp1, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else if (MQTT_UPLOAD_FORMAT_TAGS == format) { // tags
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else {
        plog_warn(plugin, "invalid upload format: %d", format);
    }

    if (json.tags) {
        free(json.tags);
    }
    return json_str;
}

int handle_write_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                          neu_resp_error_t *data)
{
    (void) plugin;
    (void) mqtt_json;
    (void) data;
    return 0;
}

int handle_read_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                         neu_resp_read_group_t *data)
{
    (void) plugin;
    (void) mqtt_json;
    (void) data;
    return 0;
}

int handle_trans_data(neu_plugin_t *            plugin,
                      neu_reqresp_trans_data_t *trans_data)
{
    int rv = 0;

    if (NULL == plugin->client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    if (!neu_mqtt_client_is_connected(plugin->client)) {
        return NEU_ERR_MQTT_FAILURE;
    }

    char *json_str =
        generate_upload_json(plugin, trans_data, plugin->config.format);
    if (NULL == json_str) {
        plog_error(plugin, "generate upload json fail");
        return NEU_ERR_EINTERNAL;
    }

    const char *   topic = plugin->config.upload_topic;
    neu_mqtt_qos_e qos   = NEU_MQTT_QOS0;
    rv = neu_mqtt_client_publish(plugin->client, qos, topic, json_str,
                                 strlen(json_str));
    if (0 != rv) {
        plog_error(plugin, "upload pub [%s, QoS%d] error", topic, qos);
        free(json_str);
        return NEU_ERR_MQTT_PUBLISH_FAILURE;
    }

    free(json_str);
    return 0;
}

int handle_nodes_state(neu_plugin_t *plugin, neu_reqresp_nodes_state_t *states)
{
    int   rv       = 0;
    char *json_str = NULL;

    if (NULL == plugin->client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    if (!neu_mqtt_client_is_connected(plugin->client)) {
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    json_str = generate_heartbeat_json(plugin, states->states);
    if (NULL == json_str) {
        plog_error(plugin, "generate heartbeat json fail");
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    const char *   topic = plugin->config.heartbeat_topic;
    neu_mqtt_qos_e qos   = NEU_MQTT_QOS0;
    rv = neu_mqtt_client_publish(plugin->client, qos, topic, json_str,
                                 strlen(json_str));
    if (0 != rv) {
        plog_error(plugin, "heartbeat pub [%s, QoS%d] error", topic, qos);
        rv = NEU_ERR_MQTT_PUBLISH_FAILURE;
    }

end:
    free(json_str);
    utarray_free(states->states);

    return rv;
}
