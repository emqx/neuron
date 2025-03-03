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

#include "ptformat.pb-c.h"

static void to_traceparent(uint8_t *trace_id, char *span_id, char *out)
{
    int size = 0;
    size     = sprintf(out, "00-");
    for (size_t i = 0; i < 16; i++) {
        size += sprintf(out + size, "%02x", trace_id[i]);
    }

    sprintf(out + size, "-%s-01", span_id);
}

static int tag_values_to_json(UT_array *tags, mqtt_static_vt_t *s_tags,
                              size_t n_s_tags, neu_json_read_resp_t *json)
{
    int index = 0;

    if (0 == utarray_len(tags)) {
        return 0;
    }

    json->n_tag = utarray_len(tags) + n_s_tags;
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

    if (s_tags != NULL) {
        for (size_t i = 0; i < n_s_tags; i++) {
            neu_json_read_resp_tag_t *tag = &json->tags[index];
            tag->name                     = s_tags[i].name;
            tag->t                        = s_tags[i].jtype;
            tag->value                    = s_tags[i].jvalue;
            index += 1;
        }
    }

    return 0;
}

void filter_error_tags(neu_reqresp_trans_data_t *data)
{
    if (!data || !data->tags) {
        return;
    }

    UT_array *filtered_tags;
    utarray_new(filtered_tags, neu_resp_tag_value_meta_icd());

    neu_resp_tag_value_meta_t *tag_ptr = NULL;
    while ((tag_ptr = (neu_resp_tag_value_meta_t *) utarray_next(data->tags,
                                                                 tag_ptr))) {
        if (tag_ptr->value.type != NEU_TYPE_ERROR) {
            utarray_push_back(filtered_tags, tag_ptr);
        }
    }

    utarray_free(data->tags);
    data->tags = filtered_tags;
}

char *generate_upload_json(neu_plugin_t *plugin, neu_reqresp_trans_data_t *data,
                           mqtt_upload_format_e format, mqtt_schema_vt_t *vts,
                           size_t n_vts, mqtt_static_vt_t *s_tags,
                           size_t n_s_tags, bool *skip)
{
    char *                   json_str = NULL;
    neu_json_read_periodic_t header   = { .group     = (char *) data->group,
                                        .node      = (char *) data->driver,
                                        .timestamp = global_timestamp };
    neu_json_read_resp_t     json     = { 0 };

    if (!plugin->config.upload_err && skip != NULL) {
        filter_error_tags(data);

        if (utarray_len(data->tags) == 0) {
            *skip = true;
            return NULL;
        }
    }

    if (format == MQTT_UPLOAD_FORMAT_CUSTOM) {
        if (0 != tag_values_to_json(data->tags, NULL, 0, &json)) {
            plog_error(plugin, "tag_values_to_json fail");
            return NULL;
        }
    } else {
        if (0 != tag_values_to_json(data->tags, s_tags, n_s_tags, &json)) {
            plog_error(plugin, "tag_values_to_json fail");
            return NULL;
        }
    }

    int ret;

    switch (format) {
    case MQTT_UPLOAD_FORMAT_VALUES:
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp1, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
        break;
    case MQTT_UPLOAD_FORMAT_TAGS:
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp2, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
        break;
    case MQTT_UPLOAD_FORMAT_ECP:
        ret = neu_json_encode_with_mqtt_ecp(
            &json, neu_json_encode_read_resp_ecp, &header,
            neu_json_encode_read_periodic_resp, &json_str);
        if (ret == -2) {
            *skip = true;
            plog_warn(plugin, "driver:%s group:%s, no valid tags", data->driver,
                      data->group);
        }
        break;
    case MQTT_UPLOAD_FORMAT_CUSTOM: {
        ret = mqtt_schema_encode(data->driver, data->group, &json, vts, n_vts,
                                 s_tags, n_s_tags, &json_str);
        break;
    }
    case MQTT_UPLOAD_FORMAT_PROTOBUF:
        break;
    default:
        plog_warn(plugin, "invalid upload format: %d", format);
        break;
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

    if (0 != tag_values_to_json(data->tags, NULL, 0, &json)) {
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
    int               rv       = 0;
    neu_plugin_t *    plugin   = data;
    neu_json_write_t *req      = NULL;
    neu_json_mqtt_t * mqtt     = NULL;
    char *            json_str = NULL;

    (void) qos;
    (void) topic;

    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_TOTAL, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_5S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_30S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_60S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_5S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_30S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_60S, 1, NULL);

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__WriteRequest *wr =
            model__write_request__unpack(NULL, len, payload);
        if (wr == NULL) {
            plog_error(plugin, "model__write_request__unpack fail");
            return;
        }

        mqtt              = calloc(1, sizeof(neu_json_mqtt_t));
        mqtt->uuid        = strdup(wr->uuid);
        mqtt->payload     = NULL;
        mqtt->traceparent = NULL;
        mqtt->tracestate  = NULL;

        neu_reqresp_head_t   header = { 0 };
        neu_req_write_tags_t cmd    = { 0 };

        header.type = NEU_REQ_WRITE_TAGS;
        header.ctx  = mqtt;
        cmd.driver  = strdup(wr->node);
        cmd.group   = strdup(wr->group);
        cmd.n_tag   = wr->n_tags;
        if (cmd.n_tag > 0) {
            cmd.tags = calloc(cmd.n_tag, sizeof(neu_resp_tag_value_t));
        }

        for (int i = 0; i < cmd.n_tag; i++) {
            strcpy(cmd.tags[i].tag, wr->tags[i]->name);
            switch (wr->tags[i]->value->value_case) {
            case MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE:
                cmd.tags[i].value.type      = NEU_TYPE_INT64;
                cmd.tags[i].value.value.i64 = wr->tags[i]->value->int_value;
                break;
            case MODEL__DATA_ITEM_VALUE__VALUE_FLOAT_VALUE:
                cmd.tags[i].value.type      = NEU_TYPE_DOUBLE;
                cmd.tags[i].value.value.d64 = wr->tags[i]->value->float_value;
                break;
            case MODEL__DATA_ITEM_VALUE__VALUE_BOOL_VALUE:
                cmd.tags[i].value.type = NEU_TYPE_BOOL;
                cmd.tags[i].value.value.boolean =
                    wr->tags[i]->value->bool_value;
                break;
            case MODEL__DATA_ITEM_VALUE__VALUE_STRING_VALUE:
                cmd.tags[i].value.type = NEU_TYPE_STRING;
                strcpy(cmd.tags[i].value.value.str,
                       wr->tags[i]->value->string_value);
                break;
            default:
                break;
            }
        }

        model__write_request__free_unpacked(wr, NULL);

        if (0 != neu_plugin_op(plugin, header, &cmd)) {
            plog_error(plugin, "neu_plugin_op(NEU_REQ_WRITE_TAGS) fail");
            free(cmd.tags);
            return;
        }
    } else {
        json_str = calloc(1, len + 1);
        if (NULL == json_str) {
            return;
        }

        memcpy(json_str, payload, len);

        rv = neu_json_decode_mqtt_req(json_str, &mqtt);
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
}

int handle_write_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                          neu_resp_error_t *data, void *trace_scope,
                          void *trace_ctx, char *span_id)
{
    int    rv                = 0;
    char * json_str          = NULL;
    size_t size              = 0;
    char   trace_parent[128] = { 0 };

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

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__WriteResponse resp = MODEL__WRITE_RESPONSE__INIT;

        resp.uuid      = mqtt_json->uuid;
        resp.n_errors  = 1;
        resp.errors    = calloc(1, sizeof(Model__WriteResponseItem *));
        resp.errors[0] = calloc(1, sizeof(Model__WriteResponseItem));
        model__write_response_item__init(resp.errors[0]);
        // TODO
        resp.errors[0]->name  = strdup("");
        resp.errors[0]->error = data->error;

        size     = model__write_response__get_packed_size(&resp);
        json_str = malloc(size);
        model__write_response__pack(&resp, (uint8_t *) json_str);

        for (size_t i = 0; i < resp.n_errors; i++) {
            free(resp.errors[i]->name);
            free(resp.errors[i]);
        }
        free(resp.errors);
    } else {
        json_str = generate_write_resp_json(plugin, mqtt_json, data);
        if (NULL == json_str) {
            plog_error(plugin, "generate write resp json fail, uuid:%s",
                       mqtt_json->uuid);
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }

        if (trace_scope) {
            neu_otel_scope_add_span_attr_string(trace_scope, "playload",
                                                json_str);
        }

        if (trace_ctx && span_id) {
            uint8_t *trace_id = neu_otel_get_trace_id(trace_ctx);
            to_traceparent(trace_id, span_id, trace_parent);
        }

        size = strlen(json_str);
    }

    char *         topic = plugin->config.write_resp_topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    if (trace_ctx != 0 && strlen(trace_parent) != 0) {
        rv = publish_with_trace(plugin, qos, topic, json_str, size,
                                trace_parent);
    } else {
        rv = publish(plugin, qos, topic, json_str, size);
    }
    json_str = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    return rv;
}

int handle_write_tags_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                               neu_resp_write_tags_t *data)
{
    int    rv       = 0;
    char * json_str = NULL;
    size_t size     = 0;

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

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__WriteResponse resp = MODEL__WRITE_RESPONSE__INIT;

        resp.uuid     = mqtt_json->uuid;
        resp.n_errors = utarray_len(data->tags);
        resp.errors = calloc(resp.n_errors, sizeof(Model__WriteResponseItem *));
        int index   = 0;

        utarray_foreach(data->tags, neu_resp_write_tags_ele_t *, tag)
        {
            resp.errors[index] = calloc(1, sizeof(Model__WriteResponseItem));
            model__write_response_item__init(resp.errors[index]);
            resp.errors[index]->name  = strdup(tag->tag);
            resp.errors[index]->error = tag->error;

            index++;
        }

        size     = model__write_response__get_packed_size(&resp);
        json_str = malloc(size);
        model__write_response__pack(&resp, (uint8_t *) json_str);

        for (size_t i = 0; i < resp.n_errors; i++) {
            free(resp.errors[i]->name);
            free(resp.errors[i]);
        }
        free(resp.errors);
    } else {
        neu_json_write_tags_resp_t write_resp = { 0 };
        write_resp.tags                       = data->tags;

        neu_json_encode_with_mqtt(&write_resp, neu_json_encode_write_tags_resp,
                                  mqtt_json, neu_json_encode_mqtt_resp,
                                  &json_str);
        if (NULL == json_str) {
            plog_error(plugin,
                       "generate driver write tags resp json fail, uuid:%s",
                       mqtt_json->uuid);
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }

        size = strlen(json_str);
    }

    char *         topic = plugin->config.write_resp_topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv                   = publish(plugin, qos, topic, json_str, size);
    json_str             = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    utarray_free(data->tags);
    return rv;
}

int handle_driver_action_response(neu_plugin_t *            plugin,
                                  neu_json_mqtt_t *         mqtt_json,
                                  neu_resp_driver_action_t *data)
{
    int    rv       = 0;
    char * json_str = NULL;
    size_t size     = 0;

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

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__DriverActionResponse resp = MODEL__DRIVER_ACTION_RESPONSE__INIT;

        resp.uuid  = mqtt_json->uuid;
        resp.error = data->error;

        size     = model__driver_action_response__get_packed_size(&resp);
        json_str = malloc(size);
        model__driver_action_response__pack(&resp, (uint8_t *) json_str);
    } else {
        json_str = generate_driver_action_resp_json(plugin, mqtt_json, data);
        if (NULL == json_str) {
            plog_error(plugin, "generate driver action resp json fail, uuid:%s",
                       mqtt_json->uuid);
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }
        size = strlen(json_str);
    }

    char *         topic = plugin->config.driver_topic.action_resp;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv                   = publish(plugin, qos, topic, json_str, size);
    json_str             = NULL;

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
    neu_plugin_t *plugin = data;

    neu_reqresp_head_t        header   = { 0 };
    neu_req_driver_action_t   cmd      = { 0 };
    neu_json_driver_action_t *req      = NULL;
    neu_json_mqtt_t *         mqtt     = NULL;
    char *                    json_str = NULL;

    header.type = NEU_REQ_DRIVER_ACTION;

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__DriverActionRequest *dar =
            model__driver_action_request__unpack(NULL, len, payload);
        if (NULL == dar) {
            plog_error(plugin, "model__driver_action_request__unpack failed");
            return;
        }

        mqtt              = calloc(1, sizeof(neu_json_mqtt_t));
        mqtt->uuid        = strdup(dar->uuid);
        mqtt->payload     = NULL;
        mqtt->traceparent = NULL;
        mqtt->tracestate  = NULL;

        header.ctx = mqtt;
        strcpy(cmd.driver, dar->node);
        cmd.action = strdup(dar->action);

        model__driver_action_request__free_unpacked(dar, NULL);
    } else {
        json_str = calloc(len + 1, sizeof(char));
        memcpy(json_str, payload, len);

        int rv = neu_json_decode_mqtt_req(json_str, &mqtt);
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

        header.ctx = mqtt;
        strcpy(cmd.driver, req->driver);
        cmd.action = strdup(req->action);
        neu_json_decode_driver_action_req_free(req);
        free(json_str);
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_DRIVER_ACTION) fail");
        return;
    }
}

void handle_read_req(neu_mqtt_qos_e qos, const char *topic,
                     const uint8_t *payload, uint32_t len, void *data,
                     trace_w3c_t *trace_w3c)
{
    int           rv     = 0;
    neu_plugin_t *plugin = data;

    neu_json_read_req_t *req      = NULL;
    neu_json_mqtt_t *    mqtt     = NULL;
    char *               json_str = NULL;

    (void) qos;
    (void) topic;

    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_TOTAL, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_5S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_30S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_60S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_5S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_30S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_60S, 1, NULL);

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__ReadRequest *read_req =
            model__read_request__unpack(NULL, len, payload);
        if (NULL == read_req) {
            plog_error(plugin, "model__read_request__unpack failed");
            return;
        }

        neu_reqresp_head_t header = { 0 };

        mqtt              = calloc(1, sizeof(neu_json_mqtt_t));
        mqtt->uuid        = strdup(read_req->uuid);
        mqtt->payload     = NULL;
        mqtt->traceparent = NULL;
        mqtt->tracestate  = NULL;

        header.type = NEU_REQ_READ_GROUP;
        header.ctx  = mqtt;

        neu_req_read_group_t cmd = { 0 };

        cmd.driver = strdup(read_req->node);
        cmd.group  = strdup(read_req->group);
        cmd.sync   = false;
        cmd.n_tag  = read_req->n_tags;
        if (cmd.n_tag > 0) {
            cmd.tags = calloc(cmd.n_tag, sizeof(char *));
        }
        for (int i = 0; i < cmd.n_tag; i++) {
            cmd.tags[i] = strdup(read_req->tags[i]);
        }

        if (0 != neu_plugin_op(plugin, header, &cmd)) {
            neu_req_read_group_fini(&cmd);
            plog_error(plugin, "neu_plugin_op(NEU_REQ_READ_GROUP) fail");
            return;
        }

        model__read_request__free_unpacked(read_req, NULL);
    } else {
        json_str = calloc(1, len + 1);
        if (NULL == json_str) {
            return;
        }

        memcpy(json_str, payload, len);

        rv = neu_json_decode_mqtt_req(json_str, &mqtt);
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
}

int handle_read_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                         neu_resp_read_group_t *data)
{
    int    rv       = 0;
    char * json_str = NULL;
    size_t size     = 0;

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

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__ReadResponse resp = MODEL__READ_RESPONSE__INIT;

        resp.uuid   = mqtt_json->uuid;
        resp.n_tags = utarray_len(data->tags);

        Model__DataItem **tags = calloc(resp.n_tags, sizeof(Model__DataItem *));

        int index = 0;
        utarray_foreach(data->tags, neu_resp_tag_value_meta_t *, tag_value)
        {
            Model__DataItem *tag = calloc(1, sizeof(Model__DataItem));
            model__data_item__init(tag);
            tag->name = tag_value->tag;
            switch (tag_value->value.type) {
            case NEU_TYPE_ERROR:
                tag->item_case = MODEL__DATA_ITEM__ITEM_ERROR;
                tag->error     = tag_value->value.value.i32;
                break;
            case NEU_TYPE_UINT8:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                tag->value->int_value = tag_value->value.value.u8;
                break;
            case NEU_TYPE_INT8:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                tag->value->int_value = tag_value->value.value.i8;
                break;
            case NEU_TYPE_INT16:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                tag->value->int_value = tag_value->value.value.i16;
                break;
            case NEU_TYPE_WORD:
            case NEU_TYPE_UINT16:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                tag->value->int_value = tag_value->value.value.u16;
                break;
            case NEU_TYPE_INT32:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                tag->value->int_value = tag_value->value.value.i32;
                break;
            case NEU_TYPE_DWORD:
            case NEU_TYPE_UINT32:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                tag->value->int_value = tag_value->value.value.u32;
                break;
            case NEU_TYPE_INT64:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                tag->value->int_value = tag_value->value.value.i64;
                break;
            case NEU_TYPE_LWORD:
            case NEU_TYPE_UINT64:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                tag->value->int_value = tag_value->value.value.u64;
                break;
            case NEU_TYPE_FLOAT:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_FLOAT_VALUE;
                tag->value->float_value = tag_value->value.value.f32;
                break;
            case NEU_TYPE_DOUBLE:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_FLOAT_VALUE;
                tag->value->float_value = tag_value->value.value.d64;
                break;
            case NEU_TYPE_BOOL:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_BOOL_VALUE;
                tag->value->bool_value = tag_value->value.value.boolean;
                break;
            case NEU_TYPE_STRING:
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                tag->value->value_case =
                    MODEL__DATA_ITEM_VALUE__VALUE_STRING_VALUE;
                tag->value->string_value = tag_value->value.value.str;
                break;
            default:
                break;
            }

            for (int i = 0; i < NEU_TAG_META_SIZE; i++) {
                if (strlen(tag_value->metas[i].name) > 0) {
                    if (strncmp(tag_value->metas[i].name, "q", 1) == 0) {
                        tag->has_q = true;
                        tag->q     = tag_value->metas[i].value.value.i32;
                    }

                    if (strncmp(tag_value->metas[i].name, "t", 1) == 0) {
                        tag->has_t = true;
                        tag->t     = tag_value->metas[i].value.value.i64;
                    }
                } else {
                    break;
                }
            }

            tags[index] = tag;
            index++;
        }

        resp.tags = tags;

        size     = model__read_response__get_packed_size(&resp);
        json_str = malloc(size);
        model__read_response__pack(&resp, (uint8_t *) json_str);

        for (size_t i = 0; i < resp.n_tags; i++) {
            if (resp.tags[i]->item_case == MODEL__DATA_ITEM__ITEM_VALUE) {
                free(resp.tags[i]->value);
            }
            free(resp.tags[i]);
        }
        free(resp.tags);
    } else {
        json_str = generate_read_resp_json(plugin, mqtt_json, data);
        if (NULL == json_str) {
            plog_error(plugin, "generate read resp json fail");
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }

        size = strlen(json_str);
    }

    char *         topic = plugin->read_resp_topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv                   = publish(plugin, qos, topic, json_str, size);
    json_str             = NULL;

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
    char *             json_str          = NULL;
    size_t             size              = 0;

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

        bool              skip_none   = false;
        size_t            n_satic_tag = 0;
        mqtt_static_vt_t *static_tags = NULL;
        if (route->static_tags != NULL && strlen(route->static_tags) > 0) {
            mqtt_static_validate(route->static_tags, &static_tags,
                                 &n_satic_tag);
        }

        if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
            Model__DataReport data_report = MODEL__DATA_REPORT__INIT;

            data_report.node      = trans_data->driver;
            data_report.group     = trans_data->group;
            data_report.timestamp = global_timestamp;
            data_report.n_tags    = utarray_len(trans_data->tags) + n_satic_tag;
            data_report.tags =
                calloc(data_report.n_tags, sizeof(Model__DataItem *));

            int index = 0;
            utarray_foreach(trans_data->tags, neu_resp_tag_value_meta_t *,
                            tag_value)
            {
                Model__DataItem *tag = calloc(1, sizeof(Model__DataItem));
                model__data_item__init(tag);
                tag->name = tag_value->tag;
                switch (tag_value->value.type) {
                case NEU_TYPE_ERROR:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_ERROR;
                    tag->error     = tag_value->value.value.i32;
                    break;
                case NEU_TYPE_UINT8:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                    tag->value->int_value = tag_value->value.value.u8;
                    break;
                case NEU_TYPE_INT8:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                    tag->value->int_value = tag_value->value.value.i8;
                    break;
                case NEU_TYPE_INT16:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                    tag->value->int_value = tag_value->value.value.i16;
                    break;
                case NEU_TYPE_WORD:
                case NEU_TYPE_UINT16:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                    tag->value->int_value = tag_value->value.value.u16;
                    break;
                case NEU_TYPE_INT32:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                    tag->value->int_value = tag_value->value.value.i32;
                    break;
                case NEU_TYPE_DWORD:
                case NEU_TYPE_UINT32:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                    tag->value->int_value = tag_value->value.value.u32;
                    break;
                case NEU_TYPE_INT64:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                    tag->value->int_value = tag_value->value.value.i64;
                    break;
                case NEU_TYPE_FLOAT:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_FLOAT_VALUE;
                    tag->value->float_value = tag_value->value.value.f32;
                    break;
                case NEU_TYPE_DOUBLE:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_FLOAT_VALUE;
                    tag->value->float_value = tag_value->value.value.d64;
                    break;
                case NEU_TYPE_BOOL:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_BOOL_VALUE;
                    tag->value->bool_value = tag_value->value.value.boolean;
                    break;
                case NEU_TYPE_STRING:
                    tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                    tag->value     = calloc(1, sizeof(Model__DataItemValue));
                    model__data_item_value__init(tag->value);
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_STRING_VALUE;
                    tag->value->string_value = tag_value->value.value.str;
                    break;
                default:
                    break;
                }

                for (int i = 0; i < NEU_TAG_META_SIZE; i++) {
                    if (strlen(tag_value->metas[i].name) > 0) {
                        if (strncmp(tag_value->metas[i].name, "q", 1) == 0) {
                            tag->has_q = true;
                            tag->q     = tag_value->metas[i].value.value.i32;
                        }

                        if (strncmp(tag_value->metas[i].name, "t", 1) == 0) {
                            tag->has_t = true;
                            tag->t     = tag_value->metas[i].value.value.i64;
                        }
                    } else {
                        break;
                    }
                }

                data_report.tags[index] = tag;
                index++;
            }

            for (size_t i = 0; i < n_satic_tag; i++) {
                Model__DataItem *tag = malloc(sizeof(Model__DataItem));
                model__data_item__init(tag);
                tag->name      = static_tags[i].name;
                tag->item_case = MODEL__DATA_ITEM__ITEM_VALUE;
                tag->value     = calloc(1, sizeof(Model__DataItemValue));
                model__data_item_value__init(tag->value);
                switch (static_tags[i].jtype) {
                case NEU_TYPE_INT64:
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                    tag->value->int_value = static_tags[i].jvalue.val_int;
                    break;
                case NEU_TYPE_DOUBLE:
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_FLOAT_VALUE;
                    tag->value->float_value = static_tags[i].jvalue.val_double;
                    break;
                case NEU_TYPE_BOOL:
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_BOOL_VALUE;
                    tag->value->bool_value = static_tags[i].jvalue.val_bool;
                    break;
                case NEU_TYPE_STRING:
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_STRING_VALUE;
                    tag->value->string_value = static_tags[i].jvalue.val_str;
                    break;
                default:
                    tag->value->value_case =
                        MODEL__DATA_ITEM_VALUE__VALUE_INT_VALUE;
                    tag->value->int_value = 0;
                    break;
                }
                data_report.tags[index] = tag;
                index++;
            }

            size     = model__data_report__get_packed_size(&data_report);
            json_str = malloc(size);

            model__data_report__pack(&data_report, (uint8_t *) json_str);
            for (size_t i = 0; i < data_report.n_tags; i++) {
                if (data_report.tags[i]->item_case ==
                    MODEL__DATA_ITEM__ITEM_VALUE) {
                    free(data_report.tags[i]->value);
                }
                free(data_report.tags[i]);
            }
            free(data_report.tags);
        } else {
            json_str = generate_upload_json(
                plugin, trans_data, plugin->config.format,
                plugin->config.schema_vts, plugin->config.n_schema_vt,
                static_tags, n_satic_tag, &skip_none);
            size = strlen(json_str);
        }
        if (n_satic_tag > 0) {
            mqtt_static_free(static_tags, n_satic_tag);
        }

        if (skip_none) {
            break;
        }
        if (NULL == json_str) {
            plog_error(plugin, "generate upload json fail");
            rv = NEU_ERR_EINTERNAL;
            break;
        }

        char *         topic = route->topic;
        neu_mqtt_qos_e qos   = plugin->config.qos;

        if (plugin->config.version == NEU_MQTT_VERSION_V5 && trans_trace) {
            rv = publish_with_trace(plugin, qos, topic, json_str, size,
                                    trace_parent);
        } else {
            rv = publish(plugin, qos, topic, json_str, size);
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

    rv =
        route_tbl_add_new(&plugin->route_tbl, sub_info->driver, sub_info->group,
                          topic.v.val_str, sub_info->static_tags);
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
                          topic.v.val_str, sub_info->static_tags);
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
    int    rv          = 0;
    char * json_str    = NULL;
    size_t size        = 0;
    bool   driver_none = false;

    if (NULL == plugin->client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    if (!neu_mqtt_client_is_connected(plugin->client)) {
        // cache disable and we are disconnected
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__NodeStateReport nsr = MODEL__NODE_STATE_REPORT__INIT;

        nsr.timestamp = global_timestamp;
        nsr.n_nodes   = utarray_len(states->states);

        Model__NodeState **node_states =
            calloc(nsr.n_nodes, sizeof(Model__NodeState *));
        if (NULL == node_states) {
            plog_error(plugin, "malloc fail");
            goto end;
        }

        utarray_foreach(states->states, neu_nodes_state_t *, state)
        {
            Model__NodeState *ns = calloc(1, sizeof(Model__NodeState));
            if (NULL == ns) {
                plog_error(plugin, "malloc fail");
                goto end;
            }
            model__node_state__init(ns);
            ns->node    = state->node;
            ns->link    = state->state.link;
            ns->running = state->state.running;

            node_states[utarray_eltidx(states->states, state)] = ns;
        }

        nsr.nodes = node_states;
        size      = model__node_state_report__get_packed_size(&nsr);
        json_str  = malloc(size);
        model__node_state_report__pack(&nsr, (uint8_t *) json_str);

        for (size_t i = 0; i < nsr.n_nodes; i++) {
            free(node_states[i]);
        }
        free(node_states);
    } else {
        json_str =
            generate_heartbeat_json(plugin, states->states, &driver_none);
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

        size = strlen(json_str);
    }

    char *         topic = plugin->config.heartbeat_topic;
    neu_mqtt_qos_e qos   = NEU_MQTT_QOS0;
    rv                   = publish(plugin, qos, topic, json_str, size);
    json_str             = NULL;

end:
    utarray_free(states->states);

    return rv;
}

void handle_driver_directory_req(neu_mqtt_qos_e qos, const char *topic,
                                 const uint8_t *payload, uint32_t len,
                                 void *data, trace_w3c_t *trace_w3c)
{
    (void) qos;
    (void) topic;
    (void) trace_w3c;

    neu_reqresp_head_t         header = { 0 };
    neu_req_driver_directory_t cmd    = { 0 };

    neu_plugin_t *                   plugin   = data;
    neu_json_driver_directory_req_t *req      = NULL;
    neu_json_mqtt_t *                mqtt     = NULL;
    char *                           json_str = NULL;

    header.type = NEU_REQ_DRIVER_DIRECTORY;

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__FileListRequest *flr =
            model__file_list_request__unpack(NULL, len, payload);
        if (NULL == flr) {
            plog_error(plugin, "model__file_list_request__unpack failed");
            return;
        }

        mqtt              = calloc(1, sizeof(neu_json_mqtt_t));
        mqtt->uuid        = strdup(flr->uuid);
        mqtt->payload     = NULL;
        mqtt->traceparent = NULL;
        mqtt->tracestate  = NULL;

        header.ctx = mqtt;
        strcpy(cmd.driver, flr->node);
        strcpy(cmd.path, flr->path);

        model__file_list_request__free_unpacked(flr, NULL);
    } else {
        json_str = calloc(len + 1, sizeof(char));
        memcpy(json_str, payload, len);

        int rv = neu_json_decode_mqtt_req(json_str, &mqtt);
        if (0 != rv) {
            plog_error(plugin, "neu_json_decode_mqtt_req failed");
            free(json_str);
            return;
        }

        rv = neu_json_decode_driver_directory_req(json_str, &req);
        if (rv != 0) {
            plog_error(plugin, "neu_json_decode_driver_action_req failed");
            free(json_str);
            return;
        }

        header.ctx = mqtt;

        strcpy(cmd.driver, req->driver);
        strcpy(cmd.path, req->path);

        neu_json_decode_driver_directory_req_free(req);
        free(json_str);
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_DRIVER_DIRECTORY) fail");
        return;
    }
}

int handle_driver_directory_response(neu_plugin_t *               plugin,
                                     neu_json_mqtt_t *            mqtt_json,
                                     neu_resp_driver_directory_t *data)
{
    int    rv       = 0;
    char * json_str = NULL;
    size_t size     = 0;

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

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__FileListResponse flr = MODEL__FILE_LIST_RESPONSE__INIT;

        flr.uuid  = mqtt_json->uuid;
        flr.error = data->error;
        if (data->error == 0) {
            flr.n_files = utarray_len(data->files);
            int index   = 0;

            flr.files = calloc(flr.n_files, sizeof(Model__FileItem *));
            if (NULL == flr.files) {
                plog_error(plugin, "malloc fail");
                goto end;
            }

            utarray_foreach(data->files, neu_resp_driver_directory_file_t *,
                            file)
            {
                Model__FileItem *fi = calloc(1, sizeof(Model__FileItem));
                if (NULL == fi) {
                    plog_error(plugin, "malloc fail");
                    goto end;
                }

                model__file_item__init(fi);
                fi->type = file->ftype;
                fi->name = file->name;
                fi->size = file->size;
                fi->t    = file->timestamp;

                flr.files[index] = fi;
                index++;
            }
        } else {
            flr.n_files = 0;
        }

        size     = model__file_list_response__get_packed_size(&flr);
        json_str = malloc(size);
        model__file_list_response__pack(&flr, (uint8_t *) json_str);

        if (data->error == 0) {
            for (size_t i = 0; i < flr.n_files; i++) {
                free(flr.files[i]);
            }
            free(flr.files);
        }
    } else {
        neu_json_driver_directory_resp_t directory = { 0 };
        directory.error                            = data->error;

        if (directory.error == 0) {
            directory.n_files = utarray_len(data->files);
            directory.files   = calloc(directory.n_files,
                                     sizeof(neu_json_driver_directory_file_t));

            utarray_foreach(data->files, neu_resp_driver_directory_file_t *,
                            file)
            {
                neu_json_driver_directory_file_t *f =
                    &directory.files[utarray_eltidx(data->files, file)];

                f->ftype     = file->ftype;
                f->name      = file->name;
                f->size      = file->size;
                f->timestamp = file->timestamp;
            }
        }
        neu_json_encode_with_mqtt(
            &directory, neu_json_encode_driver_directory_resp, mqtt_json,
            neu_json_encode_mqtt_resp, &json_str);
        if (NULL == json_str) {
            plog_error(plugin,
                       "generate driver directory resp json fail, uuid:%s",
                       mqtt_json->uuid);
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }

        size = strlen(json_str);
        if (directory.error == 0) {
            free(directory.files);
        }
    }

    char *         topic = plugin->config.driver_topic.files_resp;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv                   = publish(plugin, qos, topic, json_str, size);
    json_str             = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);

    if (data->error == 0) {
        utarray_free(data->files);
    }

    return rv;
}

void handle_driver_fup_open_req(neu_mqtt_qos_e qos, const char *topic,
                                const uint8_t *payload, uint32_t len,
                                void *data, trace_w3c_t *trace_w3c)
{
    (void) qos;
    (void) topic;
    (void) trace_w3c;
    neu_plugin_t *plugin = data;

    neu_reqresp_head_t              header   = { 0 };
    neu_req_fup_open_t              cmd      = { 0 };
    neu_json_driver_fup_open_req_t *req      = NULL;
    neu_json_mqtt_t *               mqtt     = NULL;
    char *                          json_str = NULL;

    header.type = NEU_REQ_FUP_OPEN;

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__FileUploadRequest *fur =
            model__file_upload_request__unpack(NULL, len, payload);
        if (NULL == fur) {
            plog_error(plugin, "model__file_upload_request__unpack failed");
            return;
        }

        mqtt              = calloc(1, sizeof(neu_json_mqtt_t));
        mqtt->uuid        = strdup(fur->uuid);
        mqtt->payload     = NULL;
        mqtt->traceparent = NULL;
        mqtt->tracestate  = NULL;

        header.ctx = mqtt;
        strcpy(cmd.driver, fur->node);
        strcpy(cmd.path, fur->path);

        model__file_upload_request__free_unpacked(fur, NULL);
    } else {
        json_str = calloc(len + 1, sizeof(char));
        memcpy(json_str, payload, len);

        int rv = neu_json_decode_mqtt_req(json_str, &mqtt);
        if (0 != rv) {
            plog_error(plugin, "neu_json_decode_mqtt_req failed");
            free(json_str);
            return;
        }

        rv = neu_json_decode_driver_fup_open_req(json_str, &req);
        if (rv != 0) {
            plog_error(plugin, "neu_json_decode_driver_fup_open_req failed");
            free(json_str);
            return;
        }
        header.ctx = mqtt;
        strcpy(cmd.driver, req->driver);
        strcpy(cmd.path, req->path);

        neu_json_decode_driver_fup_open_req_free(req);
        free(json_str);
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_DRIVER_FUP_OPEN) fail");
        return;
    }
}

int handle_driver_fup_open_response(neu_plugin_t *       plugin,
                                    neu_json_mqtt_t *    mqtt_json,
                                    neu_resp_fup_open_t *data)
{
    int    rv       = 0;
    char * json_str = NULL;
    size_t size     = 0;

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

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__FileUploadResponse fur = MODEL__FILE_UPLOAD_RESPONSE__INIT;

        fur.uuid  = mqtt_json->uuid;
        fur.error = data->error;

        size     = model__file_upload_response__get_packed_size(&fur);
        json_str = malloc(size);
        model__file_upload_response__pack(&fur, (uint8_t *) json_str);
    } else {
        neu_json_driver_fup_open_resp_t resp = { 0 };

        resp.error = data->error;
        resp.size  = data->size;

        neu_json_encode_with_mqtt(&resp, neu_json_encode_driver_fup_open_resp,
                                  mqtt_json, neu_json_encode_mqtt_resp,
                                  &json_str);
        if (NULL == json_str) {
            plog_error(plugin,
                       "generate driver fup open resp json fail, uuid:%s",
                       mqtt_json->uuid);
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }
        size = strlen(json_str);
    }

    char *         topic = plugin->config.driver_topic.file_up_resp;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv                   = publish(plugin, qos, topic, json_str, size);
    json_str             = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    return rv;
}

void handle_driver_fup_data_req(neu_mqtt_qos_e qos, const char *topic,
                                const uint8_t *payload, uint32_t len,
                                void *data, trace_w3c_t *trace_w3c)
{
    (void) qos;
    (void) topic;
    (void) trace_w3c;
    neu_plugin_t *plugin = data;

    neu_reqresp_head_t              header   = { 0 };
    neu_req_fup_data_t              cmd      = { 0 };
    neu_json_driver_fup_data_req_t *req      = NULL;
    neu_json_mqtt_t *               mqtt     = NULL;
    char *                          json_str = NULL;

    header.type = NEU_REQ_FUP_DATA;

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__FileUploadDataRequest *fudr =
            model__file_upload_data_request__unpack(NULL, len, payload);
        if (NULL == fudr) {
            plog_error(plugin,
                       "model__file_upload_data_request__unpack failed");
            return;
        }

        mqtt              = calloc(1, sizeof(neu_json_mqtt_t));
        mqtt->uuid        = strdup(fudr->uuid);
        mqtt->payload     = NULL;
        mqtt->traceparent = NULL;
        mqtt->tracestate  = NULL;

        header.ctx = mqtt;

        strcpy(cmd.driver, fudr->node);
        strcpy(cmd.path, fudr->path);

        model__file_upload_data_request__free_unpacked(fudr, NULL);
    } else {
        json_str = calloc(len + 1, sizeof(char));
        memcpy(json_str, payload, len);

        int rv = neu_json_decode_mqtt_req(json_str, &mqtt);
        if (0 != rv) {
            plog_error(plugin, "neu_json_decode_mqtt_req failed");
            free(json_str);
            return;
        }

        rv = neu_json_decode_driver_fup_data_req(json_str, &req);
        if (rv != 0) {
            plog_error(plugin, "neu_json_decode_driver_fup_data_req failed");
            free(json_str);
            return;
        }

        header.ctx = mqtt;

        strcpy(cmd.driver, req->driver);
        strcpy(cmd.path, req->path);

        neu_json_decode_driver_fup_data_req_free(req);
        free(json_str);
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_DRIVER_FUP_DATA) fail");
        return;
    }
}

int handle_driver_fup_data_response(neu_plugin_t *       plugin,
                                    neu_json_mqtt_t *    mqtt_json,
                                    neu_resp_fup_data_t *data)
{
    int    rv       = 0;
    char * json_str = NULL;
    size_t size     = 0;

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

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__FileUploadDataResponse fudr =
            MODEL__FILE_UPLOAD_DATA_RESPONSE__INIT;

        fudr.uuid      = mqtt_json->uuid;
        fudr.error     = data->error;
        fudr.more      = data->more;
        fudr.data.len  = data->len;
        fudr.data.data = data->data;

        size     = model__file_upload_data_response__get_packed_size(&fudr);
        json_str = malloc(size);
        model__file_upload_data_response__pack(&fudr, (uint8_t *) json_str);
    } else {
        neu_json_driver_fup_data_resp_t resp = { 0 };
        resp.error                           = data->error;
        resp.more                            = data->more;
        resp.len                             = data->len;
        resp.data                            = data->data;

        neu_json_encode_with_mqtt(&resp, neu_json_encode_driver_fup_data_resp,
                                  mqtt_json, neu_json_encode_mqtt_resp,
                                  &json_str);
        if (NULL == json_str) {
            plog_error(plugin,
                       "generate driver fup data resp json fail, uuid:%s",
                       mqtt_json->uuid);
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }

        size = strlen(json_str);
    }

    char *         topic = plugin->config.driver_topic.file_up_data_resp;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv                   = publish(plugin, qos, topic, json_str, size);
    json_str             = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    free(data->data);
    return rv;
}

void handle_driver_fdown_open_req(neu_mqtt_qos_e qos, const char *topic,
                                  const uint8_t *payload, uint32_t len,
                                  void *data, trace_w3c_t *trace_w3c)
{
    (void) qos;
    (void) topic;
    (void) trace_w3c;
    neu_plugin_t *plugin = data;

    neu_reqresp_head_t                header   = { 0 };
    neu_req_fdown_open_t              cmd      = { 0 };
    neu_json_driver_fdown_open_req_t *req      = NULL;
    neu_json_mqtt_t *                 mqtt     = NULL;
    char *                            json_str = NULL;

    header.type = NEU_REQ_FDOWN_OPEN;

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__FileDownloadRequest *fdr =
            model__file_download_request__unpack(NULL, len, payload);
        if (NULL == fdr) {
            plog_error(plugin, "model__file_download_request__unpack failed");
            return;
        }

        mqtt              = calloc(1, sizeof(neu_json_mqtt_t));
        mqtt->uuid        = strdup(fdr->uuid);
        mqtt->payload     = NULL;
        mqtt->traceparent = NULL;
        mqtt->tracestate  = NULL;

        header.ctx = mqtt;
        strcpy(cmd.driver, fdr->node);
        strcpy(cmd.src_path, fdr->src_path);
        strcpy(cmd.dst_path, fdr->dst_path);
        cmd.size = fdr->size;

        model__file_download_request__free_unpacked(fdr, NULL);
    } else {
        json_str = calloc(len + 1, sizeof(char));
        memcpy(json_str, payload, len);

        int rv = neu_json_decode_mqtt_req(json_str, &mqtt);
        if (0 != rv) {
            plog_error(plugin, "neu_json_decode_mqtt_req failed");
            free(json_str);
            return;
        }

        rv = neu_json_decode_driver_fdown_open_req(json_str, &req);
        if (rv != 0) {
            plog_error(plugin, "neu_json_decode_driver_fdown_open_req failed");
            free(json_str);
            return;
        }
        header.ctx = mqtt;
        strcpy(cmd.driver, req->driver);
        strcpy(cmd.src_path, req->src_path);
        strcpy(cmd.dst_path, req->dst_path);
        cmd.size = req->size;

        neu_json_decode_driver_fdown_open_req_free(req);
        free(json_str);
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_DRIVER_FDOWN_OPEN) fail");
        return;
    }
}

int handle_driver_fdown_open_response(neu_plugin_t *         plugin,
                                      neu_json_mqtt_t *      mqtt_json,
                                      neu_resp_fdown_open_t *data)
{
    int    rv       = 0;
    char * json_str = NULL;
    size_t size     = 0;

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

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__FileDownloadResponse fdr = MODEL__FILE_DOWNLOAD_RESPONSE__INIT;

        fdr.uuid  = mqtt_json->uuid;
        fdr.error = data->error;

        size     = model__file_download_response__get_packed_size(&fdr);
        json_str = malloc(size);
        model__file_download_response__pack(&fdr, (uint8_t *) json_str);
    } else {
        neu_json_driver_fdown_open_resp_t resp = { 0 };
        resp.error                             = data->error;

        neu_json_encode_with_mqtt(&resp, neu_json_encode_driver_fdown_open_resp,
                                  mqtt_json, neu_json_encode_mqtt_resp,
                                  &json_str);
        if (NULL == json_str) {
            plog_error(plugin,
                       "generate driver fdown open resp json fail, uuid:%s",
                       mqtt_json->uuid);
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }
        size = strlen(json_str);
    }

    char *         topic = plugin->config.driver_topic.file_down_resp;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv                   = publish(plugin, qos, topic, json_str, size);
    json_str             = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    return rv;
}

void handle_driver_fdown_data_req(neu_mqtt_qos_e qos, const char *topic,
                                  const uint8_t *payload, uint32_t len,
                                  void *data, trace_w3c_t *trace_w3c)
{
    (void) qos;
    (void) topic;
    (void) trace_w3c;
    neu_plugin_t *plugin = data;

    neu_reqresp_head_t                header   = { 0 };
    neu_resp_fdown_data_t             cmd      = { 0 };
    neu_json_driver_fdown_data_req_t *req      = NULL;
    neu_json_mqtt_t *                 mqtt     = NULL;
    char *                            json_str = NULL;

    header.type = NEU_RESP_FDOWN_DATA;

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__FileDownloadDataResponse *fddr =
            model__file_download_data_response__unpack(NULL, len, payload);
        if (NULL == fddr) {
            plog_error(plugin,
                       "model__file_download_data_response__unpack failed");
            return;
        }

        mqtt              = calloc(1, sizeof(neu_json_mqtt_t));
        mqtt->uuid        = NULL;
        mqtt->payload     = NULL;
        mqtt->traceparent = NULL;
        mqtt->tracestate  = NULL;

        header.ctx = mqtt;
        strcpy(cmd.driver, fddr->node);
        strcpy(cmd.src_path, fddr->path);
        cmd.more = fddr->more;
        cmd.len  = fddr->data.len;
        cmd.data = calloc(fddr->data.len, sizeof(uint8_t));
        memcpy(cmd.data, fddr->data.data, fddr->data.len);

        model__file_download_data_response__free_unpacked(fddr, NULL);
    } else {
        json_str = calloc(len + 1, sizeof(char));
        memcpy(json_str, payload, len);

        int rv = neu_json_decode_mqtt_req(json_str, &mqtt);
        if (0 != rv) {
            plog_error(plugin, "neu_json_decode_mqtt_req failed");
            free(json_str);
            return;
        }

        rv = neu_json_decode_driver_fdown_data_req(json_str, &req);
        if (rv != 0) {
            plog_error(plugin, "neu_json_decode_driver_fdown_data_req failed");
            free(json_str);
            return;
        }
        header.ctx = mqtt;

        strcpy(cmd.driver, req->driver);
        strcpy(cmd.src_path, req->src_path);
        cmd.more = req->more;
        cmd.len  = req->len;
        cmd.data = calloc(req->len, sizeof(uint8_t));
        memcpy(cmd.data, req->data, req->len);

        neu_json_decode_driver_fdown_data_req_free(req);
        free(json_str);
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_DRIVER_FDOWN_DATA) fail");
        return;
    }

    return;
}

int handle_driver_fdown_data_response(neu_plugin_t *        plugin,
                                      neu_json_mqtt_t *     mqtt_json,
                                      neu_req_fdown_data_t *data)
{
    int    rv       = 0;
    char * json_str = NULL;
    size_t size     = 0;

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

    if (plugin->config.format == MQTT_UPLOAD_FORMAT_PROTOBUF) {
        Model__FileDownloadDataRequest fddr =
            MODEL__FILE_DOWNLOAD_DATA_REQUEST__INIT;

        fddr.node = data->driver;
        fddr.path = data->src_path;

        size     = model__file_download_data_request__get_packed_size(&fddr);
        json_str = malloc(size);
        model__file_download_data_request__pack(&fddr, (uint8_t *) json_str);
    } else {
        neu_json_driver_fdown_data_resp_t resp = { 0 };
        resp.driver                            = data->driver;
        resp.src_path                          = data->src_path;

        neu_json_encode_with_mqtt(&resp, neu_json_encode_driver_fdown_data_resp,
                                  NULL, NULL, &json_str);
        if (NULL == json_str) {
            plog_error(plugin,
                       "generate driver fdown data resp json fail, uuid:%s",
                       mqtt_json->uuid);
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }

        size = strlen(json_str);
    }

    char *         topic = plugin->config.driver_topic.file_down_data_req;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv                   = publish(plugin, qos, topic, json_str, size);
    json_str             = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    return rv;
}