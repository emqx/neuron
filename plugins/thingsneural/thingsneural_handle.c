/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 **/

#include <inttypes.h>
#include <jansson.h>
#include <math.h>
#include <string.h>

#include "connection/mqtt_client.h"
#include "utils/asprintf.h"
#include "utils/time.h"

#include "thingsneural_handle.h"
#include "thingsneural_plugin.h"

#define TN_METHOD_REPORT "nlink.thing.property.report"
#define TN_METHOD_SET "nlink.thing.property.set"
#define TN_CODE_OK 200
#define TN_CODE_BAD 400
#define TN_CODE_FAIL 500

static void publish_cb(int errcode, neu_mqtt_qos_e qos, char *topic,
                       uint8_t *payload, uint32_t len, void *data)
{
    neu_plugin_t *plugin = data;
    (void) topic;
    (void) len;

    if (0 == errcode) {
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSGS_TOTAL, 1, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_5S, len, NULL);
    } else {
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 1,
                                 NULL);
        plog_error(plugin, "publish qos:%d fail:%d", qos, errcode);
    }
    free(payload);
}

static int publish_json(neu_plugin_t *plugin, char *topic, char *payload)
{
    size_t len = strlen(payload);
    int rv = neu_mqtt_client_publish(plugin->client, plugin->config.qos, topic,
                                     (uint8_t *) payload, (uint32_t) len,
                                     plugin, publish_cb);
    if (0 != rv) {
        plog_error(plugin, "publish %s fail", topic);
        free(payload);
        return -1;
    }
    return 0;
}

static char *next_message_id(neu_plugin_t *plugin)
{
    char *id = NULL;
    neu_asprintf(&id, "%" PRIu64 "-%" PRIu64, (uint64_t) neu_time_ms(),
                 ++plugin->msg_seq);
    return id;
}

static json_t *dvalue_to_json(neu_dvalue_t *value)
{
    switch (value->type) {
    case NEU_TYPE_INT8:
        return json_integer(value->value.i8);
    case NEU_TYPE_UINT8:
    case NEU_TYPE_BIT:
        return json_integer(value->value.u8);
    case NEU_TYPE_INT16:
        return json_integer(value->value.i16);
    case NEU_TYPE_UINT16:
    case NEU_TYPE_WORD:
        return json_integer(value->value.u16);
    case NEU_TYPE_INT32:
        return json_integer(value->value.i32);
    case NEU_TYPE_UINT32:
    case NEU_TYPE_DWORD:
        return json_integer(value->value.u32);
    case NEU_TYPE_INT64:
        return json_integer(value->value.i64);
    case NEU_TYPE_UINT64:
    case NEU_TYPE_LWORD:
        return json_integer((json_int_t) value->value.u64);
    case NEU_TYPE_FLOAT:
        if (isnan(value->value.f32)) {
            return NULL;
        }
        return json_real(value->value.f32);
    case NEU_TYPE_DOUBLE:
        if (isnan(value->value.d64)) {
            return NULL;
        }
        return json_real(value->value.d64);
    case NEU_TYPE_BOOL:
        return json_boolean(value->value.boolean);
    case NEU_TYPE_STRING:
        return json_string(value->value.str);
    default:
        return NULL;
    }
}

static const tn_point_t *find_uplink(tn_point_t *points, const char *node,
                                     const char *group, const char *tag)
{
    tn_point_t *point = NULL;
    tn_point_t *tmp   = NULL;

    HASH_ITER(hh, points, point, tmp)
    {
        if (0 == strcmp(point->node, node) &&
            0 == strcmp(point->group, group) && 0 == strcmp(point->tag, tag)) {
            return point;
        }
    }
    return NULL;
}

static int json_to_dvalue(json_t *item, neu_dvalue_t *value)
{
    memset(value, 0, sizeof(*value));
    if (json_is_integer(item)) {
        value->type      = NEU_TYPE_INT64;
        value->value.i64 = (int64_t) json_integer_value(item);
        return 0;
    }
    if (json_is_real(item)) {
        value->type      = NEU_TYPE_DOUBLE;
        value->value.d64 = json_real_value(item);
        return 0;
    }
    if (json_is_boolean(item)) {
        value->type          = NEU_TYPE_BOOL;
        value->value.boolean = json_boolean_value(item);
        return 0;
    }
    if (json_is_string(item)) {
        value->type = NEU_TYPE_STRING;
        snprintf(value->value.str, sizeof(value->value.str), "%s",
                 json_string_value(item));
        return 0;
    }
    return -1;
}

static int write_point(neu_plugin_t *plugin, const tn_point_t *point,
                       json_t *item)
{
    neu_reqresp_head_t  header = { 0 };
    neu_req_write_tag_t cmd    = { 0 };

    if (0 != json_to_dvalue(item, &cmd.value)) {
        return -1;
    }

    header.type = NEU_REQ_WRITE_TAG;
    cmd.driver  = strdup(point->node);
    cmd.group   = strdup(point->group);
    cmd.tag     = strdup(point->tag);
    if (NULL == cmd.driver || NULL == cmd.group || NULL == cmd.tag) {
        free(cmd.driver);
        free(cmd.group);
        free(cmd.tag);
        return -1;
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "write %s/%s/%s fail", point->node, point->group,
                   point->tag);
        free(cmd.driver);
        free(cmd.group);
        free(cmd.tag);
        return -1;
    }
    return 0;
}

static void publish_set_response(neu_plugin_t *plugin, const char *id, int code)
{
    json_t *root = NULL;
    char *  text = NULL;

    if (NULL == plugin->client || NULL == plugin->config.set_resp_topic) {
        return;
    }

    root = json_pack("{s:s, s:i, s:s, s:s}", "id", id ? id : "", "code", code,
                     "method", TN_METHOD_SET, "version", "1.0");
    if (NULL == root) {
        return;
    }
    text = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    if (NULL != text) {
        publish_json(plugin, plugin->config.set_resp_topic, text);
    }
}

static void on_set_message(neu_mqtt_qos_e qos, const char *topic,
                           const uint8_t *payload, uint32_t len, void *data,
                           trace_w3c_t *trace)
{
    neu_plugin_t *plugin = data;
    char *        text   = NULL;

    (void) qos;
    (void) topic;
    (void) trace;

    text = calloc(1, (size_t) len + 1);
    if (NULL == text) {
        return;
    }
    memcpy(text, payload, len);
    tn_handle_set_payload(plugin, text);
    free(text);
}

int tn_subscribe_downlink(neu_plugin_t *plugin)
{
    if (NULL == plugin->client || NULL == plugin->config.set_req_topic) {
        return -1;
    }
    return neu_mqtt_client_subscribe(plugin->client, plugin->config.qos,
                                     plugin->config.set_req_topic, plugin,
                                     on_set_message);
}

int tn_handle_trans_data(neu_plugin_t *plugin, neu_reqresp_trans_data_t *data)
{
    json_t *values        = NULL;
    json_t *params        = NULL;
    json_t *root          = NULL;
    char *  text          = NULL;
    char *  id            = NULL;
    char    time_text[32] = { 0 };
    int     n_value       = 0;

    if (NULL == plugin->client ||
        !neu_mqtt_client_is_connected(plugin->client)) {
        return NEU_ERR_PLUGIN_NOT_RUNNING;
    }

    values = json_object();
    if (NULL == values) {
        return NEU_ERR_EINTERNAL;
    }

    pthread_mutex_lock(&plugin->lock);
    utarray_foreach(data->tags, neu_resp_tag_value_meta_t *, tv)
    {
        const tn_point_t *point = NULL;
        json_t *          item  = NULL;

        if (tv->value.type == NEU_TYPE_ERROR) {
            continue;
        }
        point = find_uplink(plugin->points, data->driver, data->group, tv->tag);
        if (NULL == point) {
            continue;
        }
        item = dvalue_to_json(&tv->value);
        if (NULL == item) {
            continue;
        }
        json_object_set_new(values, point->property, item);
        n_value += 1;
    }
    pthread_mutex_unlock(&plugin->lock);

    if (0 == n_value) {
        json_decref(values);
        return NEU_ERR_SUCCESS;
    }

    snprintf(time_text, sizeof(time_text), "%" PRId64, neu_time_ms());
    id     = next_message_id(plugin);
    params = json_object();
    root   = json_object();
    if (NULL == params || NULL == root) {
        json_decref(values);
        json_decref(params);
        json_decref(root);
        free(id);
        return NEU_ERR_EINTERNAL;
    }
    json_object_set_new(params, "time", json_string(time_text));
    json_object_set_new(params, "values", values);
    values = NULL;
    json_object_set_new(root, "id", json_string(id ? id : ""));
    json_object_set_new(root, "version", json_string("1.0"));
    json_object_set_new(root, "ack", json_integer(1));
    json_object_set_new(root, "method", json_string(TN_METHOD_REPORT));
    json_object_set_new(root, "params", params);
    params = NULL;
    free(id);

    text = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    if (NULL == text) {
        return NEU_ERR_EINTERNAL;
    }
    if (0 != publish_json(plugin, plugin->config.report_topic, text)) {
        return NEU_ERR_MQTT_PUBLISH_FAILURE;
    }
    return NEU_ERR_SUCCESS;
}

int tn_handle_set_payload(neu_plugin_t *plugin, const char *payload)
{
    json_error_t error;
    json_t *     root   = NULL;
    json_t *     params = NULL;
    json_t *     values = NULL;
    const char * id     = NULL;
    const char * method = NULL;
    const char * key    = NULL;
    json_t *     item   = NULL;
    int          code   = TN_CODE_OK;
    int          n_ok   = 0;

    root = json_loads(payload, 0, &error);
    if (NULL == root || !json_is_object(root)) {
        json_decref(root);
        return NEU_ERR_BODY_IS_WRONG;
    }

    id     = json_string_value(json_object_get(root, "id"));
    method = json_string_value(json_object_get(root, "method"));
    params = json_object_get(root, "params");
    values = json_is_object(params) ? json_object_get(params, "values") : NULL;
    if ((NULL != method && 0 != strcmp(method, TN_METHOD_SET)) ||
        !json_is_object(values)) {
        publish_set_response(plugin, id, TN_CODE_BAD);
        json_decref(root);
        return NEU_ERR_BODY_IS_WRONG;
    }

    json_object_foreach(values, key, item)
    {
        tn_point_t  point = { 0 };
        tn_point_t *found = NULL;
        int         rv    = 0;

        pthread_mutex_lock(&plugin->lock);
        HASH_FIND_STR(plugin->points, key, found);
        if (NULL != found) {
            point = *found;
        }
        pthread_mutex_unlock(&plugin->lock);

        if (NULL == found) {
            plog_notice(plugin, "property %s has no south mapping", key);
            code = TN_CODE_BAD;
            continue;
        }
        rv = write_point(plugin, &point, item);
        if (0 != rv) {
            code = TN_CODE_FAIL;
            continue;
        }
        n_ok += 1;
    }

    if (0 == n_ok && code == TN_CODE_OK) {
        code = TN_CODE_BAD;
    }
    publish_set_response(plugin, id, code);
    json_decref(root);
    return code == TN_CODE_OK ? NEU_ERR_SUCCESS : NEU_ERR_BODY_IS_WRONG;
}

int tn_handle_subscribe_group(neu_plugin_t *plugin, neu_req_subscribe_t *sub)
{
    plog_notice(plugin, "subscribe driver:%s group:%s", sub->driver,
                sub->group);
    free(sub->params);
    free(sub->static_tags);
    return NEU_ERR_SUCCESS;
}

int tn_handle_update_subscribe(neu_plugin_t *plugin, neu_req_subscribe_t *sub)
{
    plog_notice(plugin, "update subscribe driver:%s group:%s", sub->driver,
                sub->group);
    free(sub->params);
    free(sub->static_tags);
    return NEU_ERR_SUCCESS;
}

int tn_handle_unsubscribe_group(neu_plugin_t *         plugin,
                                neu_req_unsubscribe_t *unsub)
{
    plog_notice(plugin, "unsubscribe driver:%s group:%s", unsub->driver,
                unsub->group);
    return NEU_ERR_SUCCESS;
}

int tn_handle_update_group(neu_plugin_t *plugin, neu_req_update_group_t *req)
{
    pthread_mutex_lock(&plugin->lock);
    tn_points_rename_group(plugin->points, req->driver, req->group,
                           req->new_name);
    pthread_mutex_unlock(&plugin->lock);
    return NEU_ERR_SUCCESS;
}

int tn_handle_del_group(neu_plugin_t *plugin, neu_req_del_group_t *req)
{
    (void) plugin;
    (void) req;
    return NEU_ERR_SUCCESS;
}

int tn_handle_update_driver(neu_plugin_t *plugin, neu_req_update_node_t *req)
{
    pthread_mutex_lock(&plugin->lock);
    tn_points_rename_node(plugin->points, req->node, req->new_name);
    pthread_mutex_unlock(&plugin->lock);
    return NEU_ERR_SUCCESS;
}

int tn_handle_del_driver(neu_plugin_t *plugin, neu_reqresp_node_deleted_t *req)
{
    (void) plugin;
    (void) req;
    return NEU_ERR_SUCCESS;
}
