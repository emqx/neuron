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

#include "connection/mqtt_client.h"
#include "errcodes.h"
#include "otel/otel_manager.h"
#include "utils/asprintf.h"
#include "version.h"
#include "json/neu_json_driver.h"
#include "json/neu_json_mqtt.h"
#include "json/neu_json_rw.h"

#include "mqtt_handle.h"
#include "mqtt_plugin.h"

static void to_traceparent(uint8_t *trace_id, char *span_id, char *out)
{
    int size = 0;
    size     = sprintf(out, "00-");
    for (size_t i = 0; i < 16; i++) {
        size += sprintf(out + size, "%02x", trace_id[i]);
    }

    sprintf(out + size, "-%s-01", span_id);
}

static int tag_values_to_json(UT_array *tags, neu_json_read_resp_t *json)
{
    int index = 0;

    if (0 == utarray_len(tags)) {
        return 0;
    }

    json->n_tag = utarray_len(tags);
    json->tags  = (neu_json_read_resp_tag_t *) calloc(
        json->n_tag, sizeof(neu_json_read_resp_tag_t));
    if (NULL == json->tags) {
        return -1;
    }

    utarray_foreach(tags, neu_resp_tag_value_meta_t *, tag_value)
    {
        neu_tag_value_to_json(tag_value, &json->tags[index]);
        index += 1;
    }

    return 0;
}

char *generate_upload_json(neu_plugin_t *plugin, neu_reqresp_trans_data_t *data,
                           mqtt_upload_format_e format)
{
    char *                   json_str = NULL;
    neu_json_read_periodic_t header   = { .group     = (char *) data->group,
                                        .node      = (char *) data->driver,
                                        .timestamp = global_timestamp };
    neu_json_read_resp_t     json     = { 0 };

    if (0 != tag_values_to_json(data->tags, &json)) {
        plog_error(plugin, "tag_values_to_json fail");
        return NULL;
    }

    if (MQTT_UPLOAD_FORMAT_VALUES == format) { // values
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp1, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else if (MQTT_UPLOAD_FORMAT_TAGS == format) { // tags
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp2, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else {
        plog_warn(plugin, "invalid upload format: %d", format);
    }

    for (int i = 0; i < json.n_tag; i++) {
        if (json.tags[i].n_meta > 0) {
            free(json.tags[i].metas);
        }
    }

    if (json.tags) {
        free(json.tags);
    }
    return json_str;
}

static char *generate_read_resp_json(neu_plugin_t *         plugin,
                                     neu_json_mqtt_t *      mqtt,
                                     neu_resp_read_group_t *data)
{
    // neu_resp_tag_value_meta_t *tags     = data->tags;
    // uint16_t                   len      = data->n_tag;
    char *               json_str = NULL;
    neu_json_read_resp_t json     = { 0 };

    if (0 != tag_values_to_json(data->tags, &json)) {
        plog_error(plugin, "tag_values_to_json fail");
        return NULL;
    }

    neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp, mqtt,
                              neu_json_encode_mqtt_resp, &json_str);

    if (json.tags) {
        free(json.tags);
    }
    return json_str;
}

static char *generate_write_resp_json(neu_plugin_t *    plugin,
                                      neu_json_mqtt_t * mqtt,
                                      neu_resp_error_t *data)
{
    (void) plugin;

    neu_json_error_resp_t error    = { .error = data->error };
    char *                json_str = NULL;

    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &json_str);

    return json_str;
}

static char *generate_driver_action_resp_json(neu_plugin_t *            plugin,
                                              neu_json_mqtt_t *         mqtt,
                                              neu_resp_driver_action_t *data)
{
    (void) plugin;

    neu_json_error_resp_t error    = { .error = data->error };
    char *                json_str = NULL;

    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &json_str);

    return json_str;
}

static char *generate_heartbeat_json(neu_plugin_t *plugin, UT_array *states,
                                     bool *drv_none)
{
    (void) plugin;
    neu_json_states_head_t header   = { .timpstamp = global_timestamp };
    neu_json_states_t      json     = { 0 };
    char *                 json_str = NULL;

    json.states = calloc(utarray_len(states), sizeof(neu_json_node_state_t));
    if (json.states == NULL) {
        return NULL;
    }

    int index = 0;
    utarray_foreach(states, neu_nodes_state_t *, state)
    {
        if (!state->is_driver) {
            continue;
        }
        json.states[index].node    = state->node;
        json.states[index].link    = state->state.link;
        json.states[index].running = state->state.running;
        index++;
    }

    json.n_state = index;

    if (index == 0) {
        *drv_none = true;
        free(json.states);
        return NULL;
    }

    neu_json_encode_with_mqtt(&json, neu_json_encode_states_resp, &header,
                              neu_json_encode_state_header_resp, &json_str);

    free(json.states);
    return json_str;
}

static inline int send_driver_action(neu_plugin_t *            plugin,
                                     neu_json_driver_action_t *req)
{
    plog_notice(plugin, "driver action, driver:%s, action:%s", req->driver,
                req->action);
    neu_reqresp_head_t header = { 0 };
    header.ctx                = NULL;

    neu_req_driver_action_t action = { 0 };
    strncpy(action.driver, req->driver, NEU_NODE_NAME_LEN);
    action.action = strdup(req->action);

    if (0 != neu_plugin_op(plugin, header, &action)) {
        free(action.action);
        plog_error(plugin, "neu_plugin_op(NEU_REQ_DRIVER_ACTION) fail");
        return -1;
    }

    return 0;
}

static inline int send_read_req(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                                neu_json_read_req_t *req)
{
    plog_notice(plugin, "read uuid:%s, group:%s, node:%s", mqtt->uuid,
                req->group, req->node);

    if (mqtt->traceparent && mqtt->tracestate) {
        plog_notice(plugin, "read, traceparent:%s, tracestate:%s",
                    mqtt->traceparent, mqtt->tracestate);
    }

    neu_reqresp_head_t header = { 0 };
    header.ctx                = mqtt;
    header.type               = NEU_REQ_READ_GROUP;
    // header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_MQTT;
    neu_req_read_group_t cmd = { 0 };
    cmd.driver               = req->node;
    cmd.group                = req->group;
    cmd.sync                 = req->sync;
    cmd.n_tag                = req->n_tags;
    cmd.tags                 = req->tags;

    req->node   = NULL; // ownership moved
    req->group  = NULL; // ownership moved
    req->n_tags = 0;
    req->tags   = NULL;

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        neu_req_read_group_fini(&cmd);
        plog_error(plugin, "neu_plugin_op(NEU_REQ_READ_GROUP) fail");
        return -1;
    }

    return 0;
}

static int json_value_to_tag_value(union neu_json_value *req,
                                   enum neu_json_type t, neu_dvalue_t *value)
{
    switch (t) {
    case NEU_JSON_INT:
        value->type      = NEU_TYPE_INT64;
        value->value.u64 = req->val_int;
        break;
    case NEU_JSON_STR:
        value->type = NEU_TYPE_STRING;
        strncpy(value->value.str, req->val_str, sizeof(value->value.str));
        break;
    case NEU_JSON_DOUBLE:
        value->type      = NEU_TYPE_DOUBLE;
        value->value.d64 = req->val_double;
        break;
    case NEU_JSON_BOOL:
        value->type          = NEU_TYPE_BOOL;
        value->value.boolean = req->val_bool;
        break;
    case NEU_JSON_ARRAY_BOOL:
        value->type               = NEU_TYPE_ARRAY_BOOL;
        value->value.bools.length = req->val_array_bool.length;
        for (int i = 0; i < req->val_array_bool.length; i++) {
            value->value.bools.bools[i] = req->val_array_bool.bools[i];
        }
        break;
    case NEU_JSON_ARRAY_DOUBLE:
        value->type              = NEU_TYPE_ARRAY_DOUBLE;
        value->value.f64s.length = req->val_array_double.length;
        for (int i = 0; i < req->val_array_double.length; i++) {
            value->value.f64s.f64s[i] = req->val_array_double.f64s[i];
        }
        break;
    case NEU_JSON_ARRAY_INT64:
        value->type              = NEU_TYPE_ARRAY_INT64;
        value->value.i64s.length = req->val_array_int64.length;
        for (int i = 0; i < req->val_array_int64.length; i++) {
            value->value.i64s.i64s[i] = req->val_array_int64.i64s[i];
        }
        break;
    case NEU_JSON_ARRAY_STR:
        value->type              = NEU_TYPE_ARRAY_STRING;
        value->value.strs.length = req->val_array_str.length;
        for (int i = 0; i < req->val_array_str.length; i++) {
            value->value.strs.strs[i] = req->val_array_str.p_strs[i];
        }
        break;
    case NEU_JSON_OBJECT:
        value->type       = NEU_TYPE_CUSTOM;
        value->value.json = req->val_object;
        break;
    default:
        return -1;
    }
    return 0;
}

static int send_write_tag_req(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                              neu_json_write_req_t *req)
{
    plog_notice(plugin, "write tag, uuid:%s, group:%s, node:%s", mqtt->uuid,
                req->group, req->node);

    if (mqtt->traceparent && mqtt->tracestate) {
        plog_notice(plugin, "write tag, traceparent:%s, tracestate:%s",
                    mqtt->traceparent, mqtt->tracestate);
    }

    neu_reqresp_head_t  header = { 0 };
    neu_req_write_tag_t cmd    = { 0 };

    header.ctx             = mqtt;
    header.type            = NEU_REQ_WRITE_TAG;
    header.otel_trace_type = NEU_OTEL_TRACE_TYPE_MQTT;

    cmd.driver = req->node;
    cmd.group  = req->group;
    cmd.tag    = req->tag;

    if (0 != json_value_to_tag_value(&req->value, req->t, &cmd.value)) {
        plog_error(plugin, "invalid tag value type: %d", req->t);
        return -1;
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_WRITE_TAG) fail");
        return -1;
    }

    req->node  = NULL; // ownership moved
    req->group = NULL; // ownership moved
    req->tag   = NULL; // ownership moved
    return 0;
}

static int send_write_tags_req(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                               neu_json_write_tags_req_t *req)
{
    plog_notice(plugin, "write tags uuid:%s, group:%s, node:%s", mqtt->uuid,
                req->group, req->node);

    if (mqtt->traceparent && mqtt->tracestate) {
        plog_notice(plugin, "write tag, traceparent:%s, tracestate:%s",
                    mqtt->traceparent, mqtt->tracestate);
    }

    for (int i = 0; i < req->n_tag; i++) {
        if (req->tags[i].t == NEU_JSON_STR) {
            if (strlen(req->tags[i].value.val_str) >= NEU_VALUE_SIZE) {
                return -1;
            }
        }
    }

    neu_reqresp_head_t header = {
        .ctx             = mqtt,
        .type            = NEU_REQ_WRITE_TAGS,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_MQTT,
    };

    neu_req_write_tags_t cmd = { 0 };
    cmd.driver               = req->node;
    cmd.group                = req->group;
    cmd.n_tag                = req->n_tag;
    cmd.tags                 = calloc(cmd.n_tag, sizeof(neu_resp_tag_value_t));
    if (NULL == cmd.tags) {
        return -1;
    }

    for (int i = 0; i < cmd.n_tag; i++) {
        strcpy(cmd.tags[i].tag, req->tags[i].tag);
        if (0 !=
            json_value_to_tag_value(&req->tags[i].value, req->tags[i].t,
                                    &cmd.tags[i].value)) {
            plog_error(plugin, "invalid tag value type: %d", req->tags[i].t);
            free(cmd.tags);
            return -1;
        }
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_WRITE_TAGS) fail");
        free(cmd.tags);
        return -1;
    }

    req->node  = NULL; // ownership moved
    req->group = NULL; // ownership moved

    return 0;
}

static void publish_cb(int errcode, neu_mqtt_qos_e qos, char *topic,
                       uint8_t *payload, uint32_t len, void *data)
{
    (void) qos;
    (void) topic;
    (void) len;

    neu_plugin_t *plugin = data;

    if (0 == errcode) {
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSGS_TOTAL, 1, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_5S, len, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_30S, len, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_60S, len, NULL);
    } else {
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 1,
                                 NULL);
    }

    free(payload);
}

int publish(neu_plugin_t *plugin, neu_mqtt_qos_e qos, char *topic,
            char *payload, size_t payload_len)
{

    int rv =
        neu_mqtt_client_publish(plugin->client, qos, topic, (uint8_t *) payload,
                                (uint32_t) payload_len, plugin, publish_cb);
    if (0 != rv) {
        plog_error(plugin, "pub [%s, QoS%d] fail", topic, qos);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 1,
                                 NULL);
        free(payload);
        rv = NEU_ERR_MQTT_PUBLISH_FAILURE;
    }

    return rv;
}

int publish_with_trace(neu_plugin_t *plugin, neu_mqtt_qos_e qos, char *topic,
                       char *payload, size_t payload_len,
                       const char *traceparent)
{
    int rv = neu_mqtt_client_publish_with_trace(
        plugin->client, qos, topic, (uint8_t *) payload, (uint32_t) payload_len,
        plugin, publish_cb, traceparent);
    if (0 != rv) {
        plog_error(plugin, "pub [%s, QoS%d] fail", topic, qos);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 1,
                                 NULL);
        free(payload);
        rv = NEU_ERR_MQTT_PUBLISH_FAILURE;
    }

    return rv;
}

void handle_write_req(neu_mqtt_qos_e qos, const char *topic,
                      const uint8_t *payload, uint32_t len, void *data,
                      trace_w3c_t *trace_w3c)
{
    int               rv     = 0;
    neu_plugin_t *    plugin = data;
    neu_json_write_t *req    = NULL;

    (void) qos;
    (void) topic;

    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_TOTAL, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_5S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_30S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_60S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_5S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_30S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_60S, 1, NULL);

    char *json_str = malloc(len + 1);
    if (NULL == json_str) {
        return;
    }

    memcpy(json_str, payload, len);
    json_str[len] = '\0';

    neu_json_mqtt_t *mqtt = NULL;
    rv                    = neu_json_decode_mqtt_req(json_str, &mqtt);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_mqtt_req failed");
        free(json_str);
        return;
    }

    if (trace_w3c && trace_w3c->traceparent) {
        mqtt->traceparent = strdup(trace_w3c->traceparent);
        mqtt->payload     = json_str;
    }

    if (trace_w3c && trace_w3c->tracestate) {
        mqtt->tracestate = strdup(trace_w3c->tracestate);
    }

    rv = neu_json_decode_write(json_str, &req);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_write fail");
        neu_json_decode_mqtt_req_free(mqtt);
        free(json_str);
        return;
    }

    if (req->singular) {
        rv = send_write_tag_req(plugin, mqtt, &req->single);
    } else {
        rv = send_write_tags_req(plugin, mqtt, &req->plural);
    }
    if (0 != rv) {
        neu_json_decode_mqtt_req_free(mqtt);
    }

    neu_json_decode_write_free(req);
    free(json_str);
}

int handle_write_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                          neu_resp_error_t *data, void *trace_scope,
                          void *trace_ctx, char *span_id)
{
    int   rv       = 0;
    char *json_str = NULL;

    if (NULL == plugin->client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    if (0 == plugin->config.cache &&
        !neu_mqtt_client_is_connected(plugin->client)) {
        // cache disable and we are disconnected
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    json_str = generate_write_resp_json(plugin, mqtt_json, data);
    if (NULL == json_str) {
        plog_error(plugin, "generate write resp json fail, uuid:%s",
                   mqtt_json->uuid);
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    if (trace_scope) {
        neu_otel_scope_add_span_attr_string(trace_scope, "playload", json_str);
    }

    char trace_parent[128] = { 0 };

    if (trace_ctx && span_id) {
        uint8_t *trace_id = neu_otel_get_trace_id(trace_ctx);
        to_traceparent(trace_id, span_id, trace_parent);
    }

    char *         topic = plugin->config.write_resp_topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    if (trace_ctx != 0 && strlen(trace_parent) != 0) {
        rv = publish_with_trace(plugin, qos, topic, json_str, strlen(json_str),
                                trace_parent);
    } else {
        rv = publish(plugin, qos, topic, json_str, strlen(json_str));
    }
    json_str = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    return rv;
}

int handle_driver_action_response(neu_plugin_t *            plugin,
                                  neu_json_mqtt_t *         mqtt_json,
                                  neu_resp_driver_action_t *data)
{
    int   rv       = 0;
    char *json_str = NULL;

    if (NULL == plugin->client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    if (0 == plugin->config.cache &&
        !neu_mqtt_client_is_connected(plugin->client)) {
        // cache disable and we are disconnected
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    json_str = generate_driver_action_resp_json(plugin, mqtt_json, data);
    if (NULL == json_str) {
        plog_error(plugin, "generate driver action resp json fail, uuid:%s",
                   mqtt_json->uuid);
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    char *         topic = plugin->config.driver_action_resp_topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    return rv;
}

void handle_driver_action_req(neu_mqtt_qos_e qos, const char *topic,
                              const uint8_t *payload, uint32_t len, void *data,
                              trace_w3c_t *trace_w3c)
{
    (void) qos;
    (void) topic;
    (void) trace_w3c;
    neu_plugin_t *            plugin = data;
    neu_json_driver_action_t *req    = NULL;

    char *json_str = calloc(len + 1, sizeof(char));
    memcpy(json_str, payload, len);

    neu_json_mqtt_t *mqtt = NULL;
    int              rv   = neu_json_decode_mqtt_req(json_str, &mqtt);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_mqtt_req failed");
        free(json_str);
        return;
    }

    rv = neu_json_decode_driver_action_req(json_str, &req);
    if (rv != 0) {
        plog_error(plugin, "neu_json_decode_driver_action_req failed");
        free(json_str);
        return;
    }

    neu_reqresp_head_t      header = { 0 };
    neu_req_driver_action_t cmd    = { 0 };

    header.ctx  = mqtt;
    header.type = NEU_REQ_DRIVER_ACTION;

    strcpy(cmd.driver, req->driver);
    cmd.action = strdup(req->action);

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_DRIVER_ACTION) fail");
        return;
    }

    neu_json_decode_driver_action_req_free(req);
    free(json_str);
}

void handle_read_req(neu_mqtt_qos_e qos, const char *topic,
                     const uint8_t *payload, uint32_t len, void *data,
                     trace_w3c_t *trace_w3c)
{
    int                  rv     = 0;
    neu_plugin_t *       plugin = data;
    neu_json_read_req_t *req    = NULL;

    (void) qos;
    (void) topic;

    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_TOTAL, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_5S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_30S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_60S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_5S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_30S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_60S, 1, NULL);

    char *json_str = malloc(len + 1);
    if (NULL == json_str) {
        return;
    }

    memcpy(json_str, payload, len);
    json_str[len] = '\0';

    neu_json_mqtt_t *mqtt = NULL;
    rv                    = neu_json_decode_mqtt_req(json_str, &mqtt);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_mqtt_req failed");
        free(json_str);
        return;
    }

    if (trace_w3c && trace_w3c->traceparent) {
        mqtt->traceparent = strdup(trace_w3c->traceparent);
        mqtt->payload     = json_str;
    }

    if (trace_w3c && trace_w3c->tracestate) {
        mqtt->tracestate = strdup(trace_w3c->tracestate);
    }

    rv = neu_json_decode_read_req(json_str, &req);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_read_req fail");
        neu_json_decode_mqtt_req_free(mqtt);
        free(json_str);
        return;
    }

    rv = send_read_req(plugin, mqtt, req);
    if (0 != rv) {
        neu_json_decode_mqtt_req_free(mqtt);
    }

    neu_json_decode_read_req_free(req);
    free(json_str);
}

int handle_read_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                         neu_resp_read_group_t *data)
{
    int   rv       = 0;
    char *json_str = NULL;

    if (NULL == plugin->client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    if (0 == plugin->config.cache &&
        !neu_mqtt_client_is_connected(plugin->client)) {
        // cache disable and we are disconnected
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    json_str = generate_read_resp_json(plugin, mqtt_json, data);
    if (NULL == json_str) {
        plog_error(plugin, "generate read resp json fail");
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    char *         topic = plugin->read_resp_topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    return rv;
}

int handle_trans_data(neu_plugin_t *            plugin,
                      neu_reqresp_trans_data_t *trans_data)
{
    int rv = 0;

    neu_otel_trace_ctx trans_trace       = NULL;
    neu_otel_scope_ctx trans_scope       = NULL;
    uint8_t *          trace_id          = NULL;
    char               trace_parent[128] = { 0 };
    if (neu_otel_data_is_started() && trans_data->trace_ctx) {
        trans_trace = neu_otel_find_trace(trans_data->trace_ctx);
        if (trans_trace) {
            char new_span_id[36] = { 0 };
            neu_otel_new_span_id(new_span_id);
            trans_scope =
                neu_otel_add_span2(trans_trace, "mqtt publish", new_span_id);
            neu_otel_scope_add_span_attr_int(trans_scope, "thread id",
                                             (int64_t)(pthread_self()));
            neu_otel_scope_set_span_start_time(trans_scope, neu_time_ns());

            trace_id = neu_otel_get_trace_id(trans_trace);

            to_traceparent(trace_id, new_span_id, trace_parent);
        }
    }

    do {

        if (NULL == plugin->client) {
            rv = NEU_ERR_MQTT_IS_NULL;
            break;
        }

        if (0 == plugin->config.cache &&
            !neu_mqtt_client_is_connected(plugin->client)) {
            // cache disable and we are disconnected
            rv = NEU_ERR_MQTT_FAILURE;
            break;
        }

        const route_entry_t *route = route_tbl_get(
            &plugin->route_tbl, trans_data->driver, trans_data->group);
        if (NULL == route) {
            plog_error(plugin, "no route for driver:%s group:%s",
                       trans_data->driver, trans_data->group);
            rv = NEU_ERR_GROUP_NOT_SUBSCRIBE;
            break;
        }

        char *json_str =
            generate_upload_json(plugin, trans_data, plugin->config.format);
        if (NULL == json_str) {
            plog_error(plugin, "generate upload json fail");
            rv = NEU_ERR_EINTERNAL;
            break;
        }

        char *         topic = route->topic;
        neu_mqtt_qos_e qos   = plugin->config.qos;

        if (plugin->config.version == NEU_MQTT_VERSION_V5 && trans_trace) {
            rv = publish_with_trace(plugin, qos, topic, json_str,
                                    strlen(json_str), trace_parent);
        } else {
            rv = publish(plugin, qos, topic, json_str, strlen(json_str));
        }

        json_str = NULL;
    } while (0);

    if (trans_trace) {
        if (rv == NEU_ERR_SUCCESS) {
            neu_otel_scope_set_status_code2(trans_scope, NEU_OTEL_STATUS_OK, 0);
        } else {
            neu_otel_scope_set_status_code2(trans_scope, NEU_OTEL_STATUS_ERROR,
                                            rv);
        }
        neu_otel_scope_set_span_end_time(trans_scope, neu_time_ns());
        neu_otel_trace_set_final(trans_trace);
    }

    return rv;
}

static inline char *default_upload_topic(neu_req_subscribe_t *info)
{
    char *t = NULL;
    neu_asprintf(&t, "/neuron/%s/%s/%s", info->app, info->driver, info->group);
    return t;
}

int handle_subscribe_group(neu_plugin_t *plugin, neu_req_subscribe_t *sub_info)
{
    int rv = 0;

    neu_json_elem_t topic = { .name = "topic", .t = NEU_JSON_STR };
    if (NULL == sub_info->params) {
        // no parameters, try default topic
        topic.v.val_str = default_upload_topic(sub_info);
        if (NULL == topic.v.val_str) {
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }
    } else if (0 != neu_parse_param(sub_info->params, NULL, 1, &topic)) {
        plog_error(plugin, "parse `%s` for topic fail", sub_info->params);
        rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        goto end;
    }

    rv = route_tbl_add_new(&plugin->route_tbl, sub_info->driver,
                           sub_info->group, topic.v.val_str);
    // topic.v.val_str ownership moved
    if (0 != rv) {
        plog_error(plugin, "route driver:%s group:%s fail, `%s`",
                   sub_info->driver, sub_info->group, sub_info->params);
        goto end;
    }

    plog_notice(plugin, "route driver:%s group:%s to topic:%s",
                sub_info->driver, sub_info->group, topic.v.val_str);

end:
    free(sub_info->params);
    return rv;
}

int handle_update_subscribe(neu_plugin_t *plugin, neu_req_subscribe_t *sub_info)
{
    int rv = 0;

    if (NULL == sub_info->params) {
        rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        goto end;
    }

    neu_json_elem_t topic = { .name = "topic", .t = NEU_JSON_STR };
    if (0 != neu_parse_param(sub_info->params, NULL, 1, &topic)) {
        plog_error(plugin, "parse `%s` for topic fail", sub_info->params);
        rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        goto end;
    }

    rv = route_tbl_update(&plugin->route_tbl, sub_info->driver, sub_info->group,
                          topic.v.val_str);
    // topic.v.val_str ownership moved
    if (0 != rv) {
        plog_error(plugin, "route driver:%s group:%s fail, `%s`",
                   sub_info->driver, sub_info->group, sub_info->params);
        goto end;
    }

    plog_notice(plugin, "route driver:%s group:%s to topic:%s",
                sub_info->driver, sub_info->group, topic.v.val_str);

end:
    free(sub_info->params);
    return rv;
}

int handle_unsubscribe_group(neu_plugin_t *         plugin,
                             neu_req_unsubscribe_t *unsub_info)
{
    route_tbl_del(&plugin->route_tbl, unsub_info->driver, unsub_info->group);
    plog_notice(plugin, "del route driver:%s group:%s", unsub_info->driver,
                unsub_info->group);
    return 0;
}

int handle_del_group(neu_plugin_t *plugin, neu_req_del_group_t *req)
{
    route_tbl_del(&plugin->route_tbl, req->driver, req->group);
    plog_notice(plugin, "del route driver:%s group:%s", req->driver,
                req->group);
    return 0;
}

int handle_update_group(neu_plugin_t *plugin, neu_req_update_group_t *req)
{
    route_tbl_update_group(&plugin->route_tbl, req->driver, req->group,
                           req->new_name);
    plog_notice(plugin, "update route driver:%s group:%s to %s", req->driver,
                req->group, req->new_name);
    return 0;
}

int handle_update_driver(neu_plugin_t *plugin, neu_req_update_node_t *req)
{
    route_tbl_update_driver(&plugin->route_tbl, req->node, req->new_name);
    plog_notice(plugin, "update route driver:%s to %s", req->node,
                req->new_name);
    return 0;
}

int handle_del_driver(neu_plugin_t *plugin, neu_reqresp_node_deleted_t *req)
{
    route_tbl_del_driver(&plugin->route_tbl, req->node);
    plog_notice(plugin, "delete route driver:%s", req->node);
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
        // cache disable and we are disconnected
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    bool driver_none = false;
    json_str = generate_heartbeat_json(plugin, states->states, &driver_none);
    if (driver_none == true) {
        plog_notice(plugin, "no driver found");
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }
    if (NULL == json_str) {
        plog_error(plugin, "generate heartbeat json fail");
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    char *         topic = plugin->config.heartbeat_topic;
    neu_mqtt_qos_e qos   = NEU_MQTT_QOS0;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

end:
    utarray_free(states->states);

    return rv;
}