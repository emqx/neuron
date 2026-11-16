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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "utils/asprintf.h"
#include "utils/time.h"
#include "utils/utlist.h"

#include "mqtt_v2_device.h"
#include "mqtt_v2_handle.h"
#include "mqtt_v2_params.h"

#define MQTT_V2_WRITE_PENDING 1

// a write naming unknown tags waits for fresh tag metadata, unless the cache
// was refreshed within this interval
#define TAGS_REFRESH_MIN_INTERVAL_MS 1000

static void process_write(neu_plugin_t *plugin, mqtt_v2_write_ctx_t *ctx);
static void write_ctx_free(mqtt_v2_write_ctx_t *ctx);
static void handle_app_state(neu_mqtt_qos_e qos, const char *topic,
                             const uint8_t *payload, uint32_t len, void *data,
                             trace_w3c_t *trace_w3c);
static void handle_write_req(neu_mqtt_qos_e qos, const char *topic,
                             const uint8_t *payload, uint32_t len, void *data,
                             trace_w3c_t *trace_w3c);

static bool connected_locked(neu_plugin_t *plugin)
{
    return NULL != plugin->client &&
        neu_mqtt_client_is_connected(plugin->client);
}

static mqtt_v2_device_t *find_device_locked(neu_plugin_t *plugin,
                                            const char *  name)
{
    mqtt_v2_device_t *device = NULL;

    HASH_FIND_STR(plugin->devices, name, device);
    return device;
}

static void publish_cb(int errcode, neu_mqtt_qos_e qos, char *topic,
                       uint8_t *payload, uint32_t len, void *data)
{
    neu_plugin_t *plugin = data;

    (void) qos;

    if (0 == errcode) {
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSGS_TOTAL, 1, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_5S, len, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_30S, len, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_60S, len, NULL);
    } else {
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 1,
                                 NULL);
    }

    free(topic);
    free(payload);
}

// takes ownership of `payload`
static int publish_locked(neu_plugin_t *plugin, const char *topic,
                          char *payload, size_t len, neu_mqtt_qos_e qos,
                          bool retain)
{
    char *dup = NULL;

    if (NULL == plugin->client || NULL == topic) {
        free(payload);
        return NEU_ERR_MQTT_IS_NULL;
    }

    // the client refers to the topic until the publish completes
    dup = strdup(topic);
    if (NULL == dup) {
        free(payload);
        return NEU_ERR_EINTERNAL;
    }

    if (0 !=
        neu_mqtt_client_publish_with_retain(plugin->client, qos, dup,
                                            (uint8_t *) payload, (uint32_t) len,
                                            retain, plugin, publish_cb)) {
        plog_error(plugin, "pub [%s, QoS%d] fail", topic, qos);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 1,
                                 NULL);
        free(dup);
        free(payload);
        return NEU_ERR_MQTT_PUBLISH_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

static void clear_topic_locked(neu_plugin_t *plugin, const char *topic)
{
    // an empty retained message deletes the one the broker keeps
    char *payload = calloc(1, 1);

    if (NULL != payload) {
        publish_locked(plugin, topic, payload, 0, NEU_MQTT_QOS1, true);
    }
}

static void retire_topic_locked(neu_plugin_t *plugin, const char *topic)
{
    if (NULL == topic) {
        return;
    }

    if (connected_locked(plugin)) {
        clear_topic_locked(plugin, topic);
    } else {
        utarray_push_back(plugin->stale_topics, &topic);
    }
}

static void publish_device_state_locked(neu_plugin_t *    plugin,
                                        mqtt_v2_device_t *device)
{
    char *payload = NULL;

    // never through the offline cache, which would replay stale states
    if (!device->state_known || NULL == device->state_topic ||
        !connected_locked(plugin)) {
        return;
    }

    payload = mqtt_v2_device_state_payload(device, global_timestamp);
    if (NULL != payload &&
        NEU_ERR_SUCCESS ==
            publish_locked(plugin, device->state_topic, payload,
                           strlen(payload), NEU_MQTT_QOS1, true)) {
        device->state_published = true;
    }
}

static void unsubscribe_device_locked(neu_plugin_t *    plugin,
                                      mqtt_v2_device_t *device)
{
    if (NULL != plugin->client && NULL != device->write_sub_topic) {
        neu_mqtt_client_unsubscribe(plugin->client, device->write_sub_topic);
    }
    free(device->write_sub_topic);
    device->write_sub_topic = NULL;
}

static void subscribe_device_locked(neu_plugin_t *    plugin,
                                    mqtt_v2_device_t *device)
{
    char *topic = NULL;

    if (NULL == plugin->client || NULL == device->write_req_topic) {
        return;
    }

    if (NULL != device->write_sub_topic &&
        0 != strcmp(device->write_sub_topic, device->write_req_topic)) {
        unsubscribe_device_locked(plugin, device);
    }

    topic = strdup(device->write_req_topic);
    if (NULL == topic ||
        0 !=
            neu_mqtt_client_subscribe(plugin->client, plugin->config.qos, topic,
                                      plugin, handle_write_req)) {
        // retried by mqtt_v2_subscribe_missing_locked
        plog_error(plugin, "subscribe [%s] fail", device->write_req_topic);
        free(topic);
        return;
    }

    free(device->write_sub_topic);
    device->write_sub_topic = topic;
}

static void remove_device_locked(neu_plugin_t *plugin, mqtt_v2_device_t *device)
{
    plog_notice(plugin, "remove device:%s", device->name);
    retire_topic_locked(plugin, device->state_topic);
    unsubscribe_device_locked(plugin, device);
    HASH_DEL(plugin->devices, device);
    mqtt_v2_device_free(device);
}

int mqtt_v2_set_topics_locked(neu_plugin_t *plugin)
{
    char *            prefix = NULL;
    char *            state  = NULL;
    mqtt_v2_device_t *device = NULL, *tmp = NULL;

    snprintf(plugin->app, sizeof(plugin->app), "%s", plugin->common.name);

    prefix = mqtt_v2_topic_render(plugin->config.topic_prefix, plugin->app,
                                  NULL, NULL);
    if (NULL != prefix) {
        neu_asprintf(&state, "%s/state", prefix);
    }
    if (NULL == prefix || NULL == state) {
        free(prefix);
        free(state);
        return -1;
    }

    free(plugin->prefix);
    free(plugin->state_topic);
    plugin->prefix      = prefix;
    plugin->state_topic = state;

    HASH_ITER(hh, plugin->devices, device, tmp)
    {
        if (0 != mqtt_v2_device_set_topics(device, plugin->app, prefix)) {
            return -1;
        }
    }

    return 0;
}

void mqtt_v2_subscribe_all_locked(neu_plugin_t *plugin)
{
    mqtt_v2_device_t *device = NULL, *tmp = NULL;

    if (NULL == plugin->client) {
        return;
    }

    // watching the own state allows restoring it, see
    // handle_app_state
    if (NULL != plugin->state_topic &&
        0 !=
            neu_mqtt_client_subscribe(plugin->client, NEU_MQTT_QOS1,
                                      plugin->state_topic, plugin,
                                      handle_app_state)) {
        plog_error(plugin, "subscribe [%s] fail", plugin->state_topic);
    }

    HASH_ITER(hh, plugin->devices, device, tmp)
    {
        subscribe_device_locked(plugin, device);
    }
}

void mqtt_v2_subscribe_missing_locked(neu_plugin_t *plugin)
{
    mqtt_v2_device_t *device = NULL, *tmp = NULL;

    if (!connected_locked(plugin)) {
        return;
    }

    HASH_ITER(hh, plugin->devices, device, tmp)
    {
        if (NULL == device->write_sub_topic) {
            subscribe_device_locked(plugin, device);
        }
    }
}

void mqtt_v2_forget_subscriptions_locked(neu_plugin_t *plugin)
{
    mqtt_v2_device_t *device = NULL, *tmp = NULL;

    HASH_ITER(hh, plugin->devices, device, tmp)
    {
        free(device->write_sub_topic);
        device->write_sub_topic = NULL;
    }
}

void mqtt_v2_publish_app_state_locked(neu_plugin_t *plugin, bool online)
{
    char *payload = NULL;

    if (NULL == plugin->state_topic || !connected_locked(plugin)) {
        return;
    }

    neu_asprintf(&payload, "{\"online\": %s, \"timestamp\": %" PRId64 "}",
                 online ? "true" : "false", global_timestamp);
    if (NULL != payload) {
        publish_locked(plugin, plugin->state_topic, payload, strlen(payload),
                       NEU_MQTT_QOS1, true);
    }
}

bool mqtt_v2_retire_topics_locked(neu_plugin_t *plugin)
{
    mqtt_v2_device_t *device = NULL, *tmp = NULL;
    bool              now = connected_locked(plugin);

    retire_topic_locked(plugin, plugin->state_topic);
    HASH_ITER(hh, plugin->devices, device, tmp)
    {
        retire_topic_locked(plugin, device->state_topic);
        device->state_published = false;
    }

    return now;
}

void mqtt_v2_connected_locked(neu_plugin_t *plugin)
{
    mqtt_v2_device_t *device = NULL, *tmp = NULL;

    // the client would only resend the subscriptions on its next timer tick
    mqtt_v2_subscribe_all_locked(plugin);

    utarray_foreach(plugin->stale_topics, char **, topic)
    {
        clear_topic_locked(plugin, *topic);
    }
    utarray_clear(plugin->stale_topics);

    HASH_ITER(hh, plugin->devices, device, tmp)
    {
        device->state_published = false;
    }

    if (plugin->started) {
        mqtt_v2_publish_app_state_locked(plugin, true);
    }
}

void mqtt_v2_disconnected_locked(neu_plugin_t *plugin)
{
    mqtt_v2_device_t *device = NULL, *tmp = NULL;

    HASH_ITER(hh, plugin->devices, device, tmp)
    {
        device->state_published = false;
    }
}

void mqtt_v2_devices_free_locked(neu_plugin_t *plugin)
{
    mqtt_v2_device_t *   device = NULL, *dtmp = NULL;
    mqtt_v2_write_ctx_t *ctx = NULL, *ctmp = NULL;

    HASH_ITER(hh, plugin->devices, device, dtmp)
    {
        HASH_DEL(plugin->devices, device);
        mqtt_v2_device_free(device);
    }

    LL_FOREACH_SAFE(plugin->pending, ctx, ctmp)
    {
        LL_DELETE(plugin->pending, ctx);
        write_ctx_free(ctx);
    }
}

int mqtt_v2_request_nodes_state(neu_plugin_t *plugin)
{
    neu_reqresp_head_t        header = { .type = NEU_REQ_GET_NODES_STATE };
    neu_req_get_nodes_state_t cmd    = {};

    return neu_plugin_op(plugin, header, &cmd);
}

int mqtt_v2_request_tags(neu_plugin_t *plugin)
{
    neu_reqresp_head_t header         = { .type = NEU_REQ_GET_SUB_DRIVER_TAGS };
    neu_req_get_sub_driver_tags_t cmd = { 0 };

    strncpy(cmd.app, plugin->common.name, sizeof(cmd.app) - 1);
    return neu_plugin_op(plugin, header, &cmd);
}

static void handle_app_state(neu_mqtt_qos_e qos, const char *topic,
                             const uint8_t *payload, uint32_t len, void *data,
                             trace_w3c_t *trace_w3c)
{
    neu_plugin_t *plugin = data;
    json_t *      root   = json_loadb((const char *) payload, len, 0, NULL);
    bool          online = json_is_true(json_object_get(root, "online"));

    (void) qos;
    (void) trace_w3c;
    json_decref(root);

    if (online) {
        return;
    }

    // e.g. the broker publishes the will of a connection taken over by this
    // one after this one has published the online state
    pthread_mutex_lock(&plugin->mtx);
    if (plugin->started && NULL != plugin->state_topic &&
        0 == strcmp(topic, plugin->state_topic)) {
        plog_notice(plugin, "online state on %s overwritten, publish again",
                    topic);
        mqtt_v2_publish_app_state_locked(plugin, true);
    }
    pthread_mutex_unlock(&plugin->mtx);
}

static bool tag_info(void *ctx, const char *name, mqtt_v2_tag_info_t *info)
{
    const mqtt_v2_tag_t *tag = mqtt_v2_group_find_tag(ctx, name);

    if (NULL == tag) {
        return false;
    }

    info->description = tag->description;
    info->unit        = tag->unit;
    return true;
}

static int publish_data_locked(neu_plugin_t *plugin, mqtt_v2_device_t *device,
                               mqtt_v2_group_t *group, UT_array *tags)
{
    const char *topic =
        NULL != group->topic ? group->topic : device->data_topic;
    char *payload = NULL;

    if (NULL == topic) {
        return NEU_ERR_MQTT_TOPIC_EMPTY;
    }

    mqtt_v2_render_ctx_t ctx = {
        .app          = plugin->app,
        .device       = device->name,
        .group        = group->name,
        .sub_topic    = topic,
        .timestamp    = global_timestamp,
        .tags         = tags,
        .upload_err   = plugin->config.upload_err,
        .tag_tmpl     = plugin->config.tag_tmpl,
        .tag_info     = tag_info,
        .tag_info_ctx = group,
    };

    payload = mqtt_v2_tmpl_render(plugin->config.data_tmpl, &ctx);
    if (NULL == payload) {
        plog_error(plugin, "render data of device:%s group:%s fail",
                   device->name, group->name);
        return NEU_ERR_EINTERNAL;
    }

    return publish_locked(plugin, topic, payload, strlen(payload),
                          plugin->config.qos, false);
}

int mqtt_v2_handle_trans_data(neu_plugin_t *            plugin,
                              neu_reqresp_trans_data_t *data)
{
    int               rv     = NEU_ERR_SUCCESS;
    mqtt_v2_device_t *device = NULL;
    mqtt_v2_group_t * group  = NULL;

    if (0 == mqtt_v2_tags_count(data->tags, plugin->config.upload_err)) {
        plog_debug(plugin, "driver:%s group:%s, no tags to upload",
                   data->driver, data->group);
        return NEU_ERR_SUCCESS;
    }

    pthread_mutex_lock(&plugin->mtx);
    if (NULL == plugin->client) {
        rv = NEU_ERR_MQTT_IS_NULL;
    } else if (!plugin->config.cache &&
               !neu_mqtt_client_is_connected(plugin->client)) {
        rv = NEU_ERR_MQTT_FAILURE;
    } else {
        device = find_device_locked(plugin, data->driver);
        group  = NULL != device ? mqtt_v2_device_find_group(device, data->group)
                               : NULL;
        if (NULL == group) {
            plog_error(plugin, "driver:%s group:%s not subscribed",
                       data->driver, data->group);
            rv = NEU_ERR_GROUP_NOT_SUBSCRIBE;
        } else {
            rv = publish_data_locked(plugin, device, group, data->tags);
        }
    }
    pthread_mutex_unlock(&plugin->mtx);

    return rv;
}

static int parse_sub_params(neu_plugin_t *plugin, const char *params,
                            char **topic)
{
    json_t *root  = NULL;
    json_t *value = NULL;

    if (NULL == params) {
        return NEU_ERR_SUCCESS;
    }

    if (NEU_ERR_SUCCESS != mqtt_v2_params_check(params)) {
        plog_error(plugin, "invalid subscription params `%s`", params);
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    root  = json_loads(params, 0, NULL);
    value = json_object_get(json_object_get(root, "params"), "topic");
    if (json_is_string(value) && 0 < json_string_length(value)) {
        *topic = strdup(json_string_value(value));
    }

    json_decref(root);
    return NEU_ERR_SUCCESS;
}

int mqtt_v2_handle_subscribe(neu_plugin_t *plugin, neu_req_subscribe_t *req,
                             bool update)
{
    char *            topic  = NULL;
    mqtt_v2_device_t *device = NULL;
    mqtt_v2_group_t * group  = NULL;
    bool              poll   = false;
    int               rv     = NEU_ERR_SUCCESS;

    rv = parse_sub_params(plugin, req->params, &topic);
    free(req->params);
    free(req->static_tags);
    if (NEU_ERR_SUCCESS != rv) {
        return rv;
    }

    pthread_mutex_lock(&plugin->mtx);
    device = find_device_locked(plugin, req->driver);
    if (NULL == device) {
        device = mqtt_v2_device_new(req->driver, plugin->app, plugin->prefix);
        if (NULL != device) {
            HASH_ADD_STR(plugin->devices, name, device);
            subscribe_device_locked(plugin, device);
            poll = true;
        }
    }

    if (NULL != device) {
        group = mqtt_v2_device_find_group(device, req->group);
        if (NULL == group) {
            group = mqtt_v2_device_add_group(device, req->group);
        }
    }

    if (NULL == group ||
        0 != mqtt_v2_group_set_topic(group, topic, plugin->app, device->name)) {
        if (NULL == group) {
            free(topic);
        }
        rv = NEU_ERR_EINTERNAL;
    } else {
        plog_notice(plugin, "%s device:%s group:%s, data topic:%s",
                    update ? "update subscription of" : "subscribe",
                    device->name, group->name,
                    NULL != group->topic
                        ? group->topic
                        : (NULL != device->data_topic ? device->data_topic
                                                      : "(unconfigured)"));
    }
    pthread_mutex_unlock(&plugin->mtx);

    if (!update) {
        mqtt_v2_request_tags(plugin);
    }
    if (poll) {
        mqtt_v2_request_nodes_state(plugin);
    }
    return rv;
}

int mqtt_v2_handle_unsubscribe(neu_plugin_t *plugin, const char *driver,
                               const char *group)
{
    mqtt_v2_device_t *device = NULL;
    mqtt_v2_group_t * grp    = NULL;

    pthread_mutex_lock(&plugin->mtx);
    device = find_device_locked(plugin, driver);
    if (NULL != device) {
        grp = mqtt_v2_device_find_group(device, group);
    }

    if (NULL != grp) {
        plog_notice(plugin, "unsubscribe device:%s group:%s", driver, group);
        mqtt_v2_device_del_group(device, grp);
        if (NULL == device->groups) {
            remove_device_locked(plugin, device);
        }
    }
    pthread_mutex_unlock(&plugin->mtx);

    return NEU_ERR_SUCCESS;
}

int mqtt_v2_handle_update_group(neu_plugin_t *          plugin,
                                neu_req_update_group_t *req)
{
    mqtt_v2_device_t *device = NULL;
    mqtt_v2_group_t * group  = NULL;

    if ('\0' == req->new_name[0] || 0 == strcmp(req->group, req->new_name)) {
        return NEU_ERR_SUCCESS;
    }

    pthread_mutex_lock(&plugin->mtx);
    device = find_device_locked(plugin, req->driver);
    if (NULL != device) {
        group = mqtt_v2_device_find_group(device, req->group);
    }

    if (NULL != group &&
        NULL == mqtt_v2_device_find_group(device, req->new_name)) {
        plog_notice(plugin, "rename device:%s group:%s to %s", req->driver,
                    req->group, req->new_name);
        mqtt_v2_device_rename_group(device, group, req->new_name, plugin->app);
    }
    pthread_mutex_unlock(&plugin->mtx);

    return NEU_ERR_SUCCESS;
}

int mqtt_v2_handle_update_driver(neu_plugin_t *         plugin,
                                 neu_req_update_node_t *req)
{
    mqtt_v2_device_t *device  = NULL;
    bool              renamed = false;

    pthread_mutex_lock(&plugin->mtx);
    device = find_device_locked(plugin, req->node);
    if (NULL != device && NULL == find_device_locked(plugin, req->new_name)) {
        plog_notice(plugin, "rename device:%s to %s", req->node, req->new_name);

        retire_topic_locked(plugin, device->state_topic);
        unsubscribe_device_locked(plugin, device);
        HASH_DEL(plugin->devices, device);

        memset(device->name, 0, sizeof(device->name));
        strncpy(device->name, req->new_name, sizeof(device->name) - 1);
        if (NULL != plugin->prefix) {
            mqtt_v2_device_set_topics(device, plugin->app, plugin->prefix);
        }
        device->state_published = false;

        HASH_ADD_STR(plugin->devices, name, device);
        subscribe_device_locked(plugin, device);
        renamed = true;
    }
    pthread_mutex_unlock(&plugin->mtx);

    if (renamed) {
        mqtt_v2_request_nodes_state(plugin);
    }
    return NEU_ERR_SUCCESS;
}

int mqtt_v2_handle_del_driver(neu_plugin_t *plugin, const char *driver)
{
    mqtt_v2_device_t *device = NULL;

    pthread_mutex_lock(&plugin->mtx);
    device = find_device_locked(plugin, driver);
    if (NULL != device) {
        remove_device_locked(plugin, device);
    }
    pthread_mutex_unlock(&plugin->mtx);

    return NEU_ERR_SUCCESS;
}

int mqtt_v2_handle_nodes_state(neu_plugin_t *              plugin,
                               neu_resp_get_nodes_state_t *resp)
{
    pthread_mutex_lock(&plugin->mtx);
    utarray_foreach(resp->states, neu_nodes_state_t *, state)
    {
        mqtt_v2_device_t *device = NULL;

        if (!state->is_driver) {
            continue;
        }

        device = find_device_locked(plugin, state->node);
        if (NULL == device) {
            continue;
        }

        if (!device->state_known || device->state.link != state->state.link ||
            device->state.running != state->state.running) {
            device->state_known     = true;
            device->state.link      = state->state.link;
            device->state.running   = state->state.running;
            device->state_published = false;
        }

        if (!device->state_published) {
            publish_device_state_locked(plugin, device);
        }
    }
    pthread_mutex_unlock(&plugin->mtx);

    utarray_free(resp->states);
    return NEU_ERR_SUCCESS;
}

static void settle_pending(neu_plugin_t *plugin)
{
    mqtt_v2_write_ctx_t *list = NULL, *ctx = NULL, *tmp = NULL;

    pthread_mutex_lock(&plugin->mtx);
    list                    = plugin->pending;
    plugin->pending         = NULL;
    plugin->tags_refreshing = false;
    pthread_mutex_unlock(&plugin->mtx);

    LL_FOREACH_SAFE(list, ctx, tmp)
    {
        LL_DELETE(list, ctx);
        ctx->retried = true;
        process_write(plugin, ctx);
    }
}

int mqtt_v2_handle_sub_tags(neu_plugin_t *                  plugin,
                            neu_resp_get_sub_driver_tags_t *resp)
{
    pthread_mutex_lock(&plugin->mtx);
    utarray_foreach(resp->infos, neu_resp_get_sub_driver_tags_info_t *, info)
    {
        mqtt_v2_device_t *device = find_device_locked(plugin, info->driver);
        mqtt_v2_group_t * group  = NULL != device
            ? mqtt_v2_device_find_group(device, info->group)
            : NULL;
        if (NULL != group) {
            mqtt_v2_group_set_tags(group, info->tags);
        }
    }
    plugin->tags_refresh_ts = neu_time_ms();
    pthread_mutex_unlock(&plugin->mtx);

    utarray_foreach(resp->infos, neu_resp_get_sub_driver_tags_info_t *, info)
    {
        utarray_free(info->tags);
    }
    utarray_free(resp->infos);

    settle_pending(plugin);
    return NEU_ERR_SUCCESS;
}

static void write_ctx_free(mqtt_v2_write_ctx_t *ctx)
{
    mqtt_v2_write_req_fini(&ctx->req);
    free(ctx->resp_topic);
    free(ctx->groups);
    free(ctx);
}

static void respond_write(neu_plugin_t *plugin, mqtt_v2_write_ctx_t *ctx,
                          int error)
{
    mqtt_v2_write_req_t *req = &ctx->req;
    char *payload = mqtt_v2_write_resp_encode(req->id, req->version, error,
                                              req->values, req->n_value);

    pthread_mutex_lock(&plugin->mtx);
    if (NULL == payload) {
        plog_error(plugin, "encode write response of device:%s fail",
                   ctx->device);
    } else if (!plugin->config.cache && !connected_locked(plugin)) {
        plog_warn(plugin, "drop write response of device:%s, disconnected",
                  ctx->device);
        free(payload);
    } else {
        publish_locked(plugin, ctx->resp_topic, payload, strlen(payload),
                       plugin->config.qos, false);
    }
    pthread_mutex_unlock(&plugin->mtx);

    write_ctx_free(ctx);
}

static bool tag_writable(const mqtt_v2_tag_t *tag)
{
    return NEU_ATTRIBUTE_WRITE & tag->attribute;
}

static int resolve_locked(const neu_plugin_t *plugin, mqtt_v2_device_t *device,
                          mqtt_v2_write_ctx_t *ctx)
{
    mqtt_v2_write_req_t *req     = &ctx->req;
    mqtt_v2_group_t *    only    = NULL;
    bool                 missing = false;
    int                  rv      = NEU_ERR_SUCCESS;

    if (NULL == ctx->groups) {
        ctx->groups = calloc(req->n_value, sizeof(*ctx->groups));
        if (NULL == ctx->groups) {
            return NEU_ERR_EINTERNAL;
        }
    }

    if (NULL != req->group) {
        only = mqtt_v2_device_find_group(device, req->group);
        if (NULL == only) {
            return NEU_ERR_GROUP_NOT_SUBSCRIBE;
        }
    } else if (1 == HASH_COUNT(device->groups)) {
        only = device->groups;
    }

    for (size_t i = 0; i < req->n_value; ++i) {
        mqtt_v2_write_value_t *value    = &req->values[i];
        const mqtt_v2_group_t *found    = NULL;
        bool                   readonly = false;

        if (NULL != only) {
            // the driver checks the tags a stale cache does not know
            const mqtt_v2_tag_t *tag = mqtt_v2_group_find_tag(only, value->tag);
            readonly                 = NULL != tag && !tag_writable(tag);
            found                    = only;
        } else {
            mqtt_v2_group_t *group = NULL, *tmp = NULL;
            HASH_ITER(hh, device->groups, group, tmp)
            {
                const mqtt_v2_tag_t *tag =
                    mqtt_v2_group_find_tag(group, value->tag);
                if (NULL == tag) {
                    continue;
                }
                if (tag_writable(tag)) {
                    found    = group;
                    readonly = false;
                    break;
                }
                readonly = true;
            }
        }

        if (readonly) {
            value->error = NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE;
        } else if (NULL != found) {
            snprintf(ctx->groups[i], sizeof(ctx->groups[i]), "%s", found->name);
        } else {
            missing = true;
        }
    }

    if (missing && !ctx->retried &&
        (plugin->tags_refreshing ||
         neu_time_ms() - plugin->tags_refresh_ts >=
             TAGS_REFRESH_MIN_INTERVAL_MS)) {
        return MQTT_V2_WRITE_PENDING;
    }

    for (size_t i = 0; i < req->n_value; ++i) {
        mqtt_v2_write_value_t *value = &req->values[i];
        if ('\0' == ctx->groups[i][0] && NEU_ERR_SUCCESS == value->error) {
            value->error = NEU_ERR_TAG_NOT_EXIST;
        }
        if (NEU_ERR_SUCCESS == rv) {
            rv = value->error;
        }
    }

    return rv;
}

// on success the write request owns the value buffers
static int send_write_tag(neu_plugin_t *plugin, mqtt_v2_write_ctx_t *ctx)
{
    mqtt_v2_write_value_t *value  = &ctx->req.values[0];
    neu_reqresp_head_t     header = { .type = NEU_REQ_WRITE_TAG, .ctx = ctx };
    neu_req_write_tag_t    cmd    = {
        .driver = strdup(ctx->device),
        .group  = strdup(ctx->groups[0]),
        .tag    = strdup(value->tag),
        .value  = value->value,
    };

    memset(&value->value, 0, sizeof(value->value));

    if (NULL == cmd.driver || NULL == cmd.group || NULL == cmd.tag ||
        0 != neu_plugin_op(plugin, header, &cmd)) {
        neu_req_write_tag_fini(&cmd);
        neu_free_dvalue(&cmd.value);
        return NEU_ERR_EINTERNAL;
    }

    return NEU_ERR_SUCCESS;
}

static int send_write_tags(neu_plugin_t *plugin, mqtt_v2_write_ctx_t *ctx)
{
    mqtt_v2_write_req_t *req    = &ctx->req;
    neu_reqresp_head_t   header = { .type = NEU_REQ_WRITE_TAGS, .ctx = ctx };
    neu_req_write_tags_t cmd    = {
        .driver = strdup(ctx->device),
        .group  = strdup(ctx->groups[0]),
        .n_tag  = (int) req->n_value,
        .tags   = calloc(req->n_value, sizeof(neu_resp_tag_value_t)),
    };

    if (NULL == cmd.driver || NULL == cmd.group || NULL == cmd.tags) {
        neu_req_write_tags_fini(&cmd);
        return NEU_ERR_EINTERNAL;
    }

    for (size_t i = 0; i < req->n_value; ++i) {
        snprintf(cmd.tags[i].tag, sizeof(cmd.tags[i].tag), "%s",
                 req->values[i].tag);
        cmd.tags[i].value = req->values[i].value;
        memset(&req->values[i].value, 0, sizeof(req->values[i].value));
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        for (int i = 0; i < cmd.n_tag; ++i) {
            neu_free_dvalue(&cmd.tags[i].value);
        }
        neu_req_write_tags_fini(&cmd);
        return NEU_ERR_EINTERNAL;
    }

    return NEU_ERR_SUCCESS;
}

static int send_write_gtags(neu_plugin_t *plugin, mqtt_v2_write_ctx_t *ctx)
{
    mqtt_v2_write_req_t * req    = &ctx->req;
    neu_reqresp_head_t    header = { .type = NEU_REQ_WRITE_GTAGS, .ctx = ctx };
    neu_req_write_gtags_t cmd    = { 0 };

    cmd.driver = strdup(ctx->device);
    cmd.groups = calloc(req->n_value, sizeof(neu_req_gtag_group_t));
    if (NULL == cmd.driver || NULL == cmd.groups) {
        goto error;
    }

    for (size_t i = 0; i < req->n_value; ++i) {
        neu_req_gtag_group_t *group = NULL;

        for (int k = 0; k < cmd.n_group; ++k) {
            if (0 == strcmp(cmd.groups[k].group, ctx->groups[i])) {
                group = &cmd.groups[k];
                break;
            }
        }

        if (NULL == group) {
            group        = &cmd.groups[cmd.n_group++];
            group->group = strdup(ctx->groups[i]);
            group->tags  = calloc(req->n_value, sizeof(neu_resp_tag_value_t));
            if (NULL == group->group || NULL == group->tags) {
                goto error;
            }
        }

        neu_resp_tag_value_t *tv = &group->tags[group->n_tag++];
        snprintf(tv->tag, sizeof(tv->tag), "%s", req->values[i].tag);
        tv->value = req->values[i].value;
        memset(&req->values[i].value, 0, sizeof(req->values[i].value));
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        goto error;
    }

    return NEU_ERR_SUCCESS;

error:
    for (int k = 0; NULL != cmd.groups && k < cmd.n_group; ++k) {
        for (int j = 0; j < cmd.groups[k].n_tag; ++j) {
            neu_free_dvalue(&cmd.groups[k].tags[j].value);
        }
    }
    neu_req_write_gtags_fini(&cmd);
    return NEU_ERR_EINTERNAL;
}

static void dispatch_write(neu_plugin_t *plugin, mqtt_v2_write_ctx_t *ctx)
{
    mqtt_v2_write_req_t *req       = &ctx->req;
    bool                 one_group = true;
    int                  rv        = NEU_ERR_SUCCESS;

    for (size_t i = 1; i < req->n_value; ++i) {
        if (0 != strcmp(ctx->groups[i], ctx->groups[0])) {
            one_group = false;
            break;
        }
    }

    plog_notice(plugin, "write device:%s, %zu tags", ctx->device, req->n_value);

    // the response may arrive and free `ctx` as soon as a request is sent
    if (1 == req->n_value) {
        rv = send_write_tag(plugin, ctx);
    } else if (one_group) {
        rv = send_write_tags(plugin, ctx);
    } else {
        rv = send_write_gtags(plugin, ctx);
    }

    if (NEU_ERR_SUCCESS != rv) {
        plog_error(plugin, "send write request of device:%s fail", ctx->device);
        mqtt_v2_write_values_fail(req->values, req->n_value, rv);
        respond_write(plugin, ctx, rv);
    }
}

static void process_write(neu_plugin_t *plugin, mqtt_v2_write_ctx_t *ctx)
{
    mqtt_v2_device_t *device  = NULL;
    bool              refresh = false;
    int               rv      = NEU_ERR_SUCCESS;

    pthread_mutex_lock(&plugin->mtx);
    device = find_device_locked(plugin, ctx->device);
    rv     = NULL == device ? NEU_ERR_GROUP_NOT_SUBSCRIBE
                        : resolve_locked(plugin, device, ctx);
    if (MQTT_V2_WRITE_PENDING == rv) {
        LL_APPEND(plugin->pending, ctx);
        refresh                 = !plugin->tags_refreshing;
        plugin->tags_refreshing = true;
    }
    pthread_mutex_unlock(&plugin->mtx);

    if (MQTT_V2_WRITE_PENDING == rv) {
        if (refresh && 0 != mqtt_v2_request_tags(plugin)) {
            // nothing will answer, settle the writes with what is known
            settle_pending(plugin);
        }
        return;
    }

    if (NEU_ERR_SUCCESS != rv) {
        plog_warn(plugin, "reject write request of device:%s, error:%d",
                  ctx->device, rv);
        mqtt_v2_write_values_fail(ctx->req.values, ctx->req.n_value, rv);
        respond_write(plugin, ctx, rv);
        return;
    }

    dispatch_write(plugin, ctx);
}

static const mqtt_v2_device_t *
find_write_device_locked(const neu_plugin_t *plugin, const char *topic)
{
    mqtt_v2_device_t *device = NULL, *tmp = NULL;

    HASH_ITER(hh, plugin->devices, device, tmp)
    {
        if (NULL != device->write_req_topic &&
            0 == strcmp(device->write_req_topic, topic)) {
            return device;
        }
    }
    return NULL;
}

static void handle_write_req(neu_mqtt_qos_e qos, const char *topic,
                             const uint8_t *payload, uint32_t len, void *data,
                             trace_w3c_t *trace_w3c)
{
    neu_plugin_t *          plugin = data;
    mqtt_v2_write_ctx_t *   ctx    = NULL;
    const mqtt_v2_device_t *device = NULL;
    int                     rv     = NEU_ERR_SUCCESS;

    (void) qos;
    (void) trace_w3c;

    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_TOTAL, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_5S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_30S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_60S, len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_5S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_30S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_60S, 1, NULL);

    ctx = calloc(1, sizeof(*ctx));
    if (NULL == ctx) {
        return;
    }
    ctx->magic = MQTT_V2_WRITE_CTX_MAGIC;
    rv         = mqtt_v2_write_req_decode(payload, len, &ctx->req);

    pthread_mutex_lock(&plugin->mtx);
    device = find_write_device_locked(plugin, topic);
    if (NULL != device) {
        snprintf(ctx->device, sizeof(ctx->device), "%s", device->name);
        ctx->resp_topic = strdup(device->write_resp_topic);
    }
    pthread_mutex_unlock(&plugin->mtx);

    if (NULL == ctx->resp_topic) {
        plog_warn(plugin, "no device for write request on %s", topic);
        write_ctx_free(ctx);
        return;
    }

    if (NEU_ERR_SUCCESS != rv) {
        plog_warn(plugin, "decode write request of device:%s fail, error:%d",
                  ctx->device, rv);
        mqtt_v2_write_values_fail(ctx->req.values, ctx->req.n_value, rv);
        respond_write(plugin, ctx, rv);
        return;
    }

    process_write(plugin, ctx);
}

int mqtt_v2_handle_write_resp(neu_plugin_t *plugin, mqtt_v2_write_ctx_t *ctx,
                              int error, UT_array *tags)
{
    mqtt_v2_write_req_t *req = &ctx->req;

    if (NULL != tags) {
        error = NEU_ERR_SUCCESS;
        utarray_foreach(tags, neu_resp_write_tags_ele_t *, ele)
        {
            for (size_t i = 0; i < req->n_value; ++i) {
                if (0 == strcmp(req->values[i].tag, ele->tag) &&
                    0 == strcmp(ctx->groups[i], ele->group)) {
                    req->values[i].error = ele->error;
                }
            }
            if (NEU_ERR_SUCCESS == error) {
                error = ele->error;
            }
        }
        utarray_free(tags);
    } else {
        for (size_t i = 0; i < req->n_value; ++i) {
            req->values[i].error = error;
        }
    }

    plog_notice(plugin, "write response of device:%s, error:%d", ctx->device,
                error);
    respond_write(plugin, ctx, error);
    return NEU_ERR_SUCCESS;
}
