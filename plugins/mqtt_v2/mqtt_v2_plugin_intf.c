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

#include <stdlib.h>
#include <string.h>

#include "utils/time.h"

#include "mqtt_v2_handle.h"
#include "mqtt_v2_plugin.h"
#include "mqtt_v2_plugin_intf.h"

#define STATE_POLL_INTERVAL_S 1
#define TAGS_REFRESH_TICKS 30

// lets the last messages leave before the connection closes
#define FLUSH_WAIT_MS 200
#define DISCONNECT_TIMEOUT_MS 1000

extern const neu_plugin_module_t neu_plugin_module;

static void connect_cb(void *data)
{
    neu_plugin_t *plugin      = data;
    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    plog_notice(plugin, "plugin `%s` connected", neu_plugin_module.module_name);

    pthread_mutex_lock(&plugin->mtx);
    mqtt_v2_connected_locked(plugin);
    pthread_mutex_unlock(&plugin->mtx);

    // device states are published with the response
    mqtt_v2_request_nodes_state(plugin);
}

static void disconnect_cb(void *data)
{
    neu_plugin_t *plugin      = data;
    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_DISCONNECTION_60S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_DISCONNECTION_600S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_DISCONNECTION_1800S, 1, NULL);
    plog_notice(plugin, "plugin `%s` disconnected",
                neu_plugin_module.module_name);

    pthread_mutex_lock(&plugin->mtx);
    mqtt_v2_disconnected_locked(plugin);
    pthread_mutex_unlock(&plugin->mtx);
}

static int timer_cb(void *data)
{
    neu_plugin_t *plugin = data;

    pthread_mutex_lock(&plugin->mtx);
    mqtt_v2_subscribe_missing_locked(plugin);
    pthread_mutex_unlock(&plugin->mtx);

    mqtt_v2_request_nodes_state(plugin);
    if (0 == ++plugin->ticks % TAGS_REFRESH_TICKS) {
        mqtt_v2_request_tags(plugin);
    }
    return 0;
}

static void timer_stop(neu_plugin_t *plugin)
{
    if (NULL != plugin->timer) {
        neu_event_del_timer(plugin->events, plugin->timer);
        plugin->timer = NULL;
    }
}

static neu_mqtt_client_t *client_new(neu_plugin_t *          plugin,
                                     const mqtt_v2_config_t *config,
                                     const char *            will_topic)
{
    char               will[] = "{\"online\": false}";
    neu_mqtt_client_t *client = neu_mqtt_client_new(config->version);

    if (NULL == client) {
        plog_error(plugin, "neu_mqtt_client_new fail");
        return NULL;
    }

    if (0 != neu_mqtt_client_set_zlog_category(client, plugin->common.log) ||
        0 != neu_mqtt_client_set_addr(client, config->host, config->port) ||
        0 != neu_mqtt_client_set_id(client, config->client_id) ||
        0 != neu_mqtt_client_set_connect_cb(client, connect_cb, plugin) ||
        0 != neu_mqtt_client_set_disconnect_cb(client, disconnect_cb, plugin) ||
        0 !=
            neu_mqtt_client_set_cache_size(client, config->cache_mem_size,
                                           config->cache_disk_size) ||
        0 !=
            neu_mqtt_client_set_cache_sync_interval(
                client, config->cache_sync_interval) ||
        (NULL != config->username &&
         0 !=
             neu_mqtt_client_set_user(
                 client, config->username,
                 NULL != config->password ? config->password : "")) ||
        0 !=
            neu_mqtt_client_set_tls(client, config->ssl, config->ca,
                                    config->cert, config->key, NULL) ||
        0 !=
            neu_mqtt_client_set_will_msg(client, will_topic, (uint8_t *) will,
                                         strlen(will), true, NEU_MQTT_QOS1)) {
        plog_error(plugin, "configure mqtt client fail");
        neu_mqtt_client_free(client);
        return NULL;
    }

    return client;
}

static void client_close(neu_mqtt_client_t *client)
{
    // without a DISCONNECT packet the broker would publish the will
    neu_mqtt_client_disconnect(client, DISCONNECT_TIMEOUT_MS);
    neu_mqtt_client_close(client);
}

static void client_free(neu_mqtt_client_t *client)
{
    if (NULL != client) {
        neu_mqtt_client_remove_cache_db(client);
        client_close(client);
        neu_mqtt_client_free(client);
    }
}

// replaces the client by one for the current config and topics
static int client_rebuild(neu_plugin_t *plugin)
{
    neu_mqtt_client_t *client     = NULL;
    char *             will_topic = NULL;
    bool               started    = false;

    pthread_mutex_lock(&plugin->mtx);
    client         = plugin->client;
    plugin->client = NULL;
    mqtt_v2_forget_subscriptions_locked(plugin);
    will_topic =
        NULL != plugin->state_topic ? strdup(plugin->state_topic) : NULL;
    started = plugin->started;
    pthread_mutex_unlock(&plugin->mtx);

    client_free(client);
    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;

    client = NULL != will_topic
        ? client_new(plugin, &plugin->config, will_topic)
        : NULL;
    free(will_topic);
    if (NULL == client) {
        return NEU_ERR_MQTT_INIT_FAILURE;
    }

    pthread_mutex_lock(&plugin->mtx);
    plugin->client = client;
    mqtt_v2_subscribe_all_locked(plugin);
    pthread_mutex_unlock(&plugin->mtx);

    if (started && 0 != neu_mqtt_client_open(client)) {
        plog_error(plugin, "neu_mqtt_client_open fail");
        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

static void app_renamed(neu_plugin_t *plugin)
{
    bool flushed = false;
    int  rv      = 0;

    plog_notice(plugin, "app `%s` renamed to `%s`, rebuild topics", plugin->app,
                plugin->common.name);

    pthread_mutex_lock(&plugin->mtx);
    flushed = mqtt_v2_retire_topics_locked(plugin);
    rv      = mqtt_v2_set_topics_locked(plugin);
    pthread_mutex_unlock(&plugin->mtx);

    if (0 != rv) {
        plog_error(plugin, "rebuild topics fail");
        return;
    }

    if (flushed) {
        neu_msleep(FLUSH_WAIT_MS);
    }

    if (NEU_ERR_SUCCESS != client_rebuild(plugin)) {
        plog_error(plugin, "rebuild mqtt client fail");
    }
}

static neu_plugin_t *mqtt_v2_plugin_open(void)
{
    neu_plugin_t *plugin = calloc(1, sizeof(neu_plugin_t));

    if (NULL == plugin) {
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);
    pthread_mutex_init(&plugin->life_mtx, NULL);
    pthread_mutex_init(&plugin->mtx, NULL);
    utarray_new(plugin->stale_topics, &ut_str_icd);
    return plugin;
}

static int mqtt_v2_plugin_close(neu_plugin_t *plugin)
{
    plog_notice(plugin, "success to free plugin:%s",
                neu_plugin_module.module_name);

    utarray_free(plugin->stale_topics);
    pthread_mutex_destroy(&plugin->mtx);
    pthread_mutex_destroy(&plugin->life_mtx);
    free(plugin);
    return NEU_ERR_SUCCESS;
}

static int mqtt_v2_plugin_init(neu_plugin_t *plugin, bool load)
{
    (void) load;

    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_CACHED_MSGS_NUM, 0);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_TRANS_DATA_30S, 30000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_TRANS_DATA_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_BYTES_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_BYTES_30S, 30000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_BYTES_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_BYTES_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_BYTES_30S, 30000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_BYTES_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_MSGS_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_MSGS_30S, 30000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_MSGS_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_DISCONNECTION_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_DISCONNECTION_600S, 600000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_DISCONNECTION_1800S, 1800000);

    plugin->events = neu_event_new(plugin->common.name);
    if (NULL == plugin->events) {
        plog_error(plugin, "neu_event_new fail");
        return NEU_ERR_EINTERNAL;
    }

    plog_notice(plugin, "initialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int plugin_uninit(neu_plugin_t *plugin)
{
    neu_mqtt_client_t *client  = NULL;
    bool               flushed = false;

    timer_stop(plugin);
    if (NULL != plugin->events) {
        neu_event_close(plugin->events);
        plugin->events = NULL;
    }

    pthread_mutex_lock(&plugin->mtx);
    if (plugin->deleted) {
        flushed = mqtt_v2_retire_topics_locked(plugin);
    } else if (plugin->started && NULL != plugin->client &&
               neu_mqtt_client_is_connected(plugin->client)) {
        // a clean disconnect discards the will
        mqtt_v2_publish_app_state_locked(plugin, false);
        flushed = true;
    }
    plugin->started = false;
    client          = plugin->client;
    plugin->client  = NULL;
    pthread_mutex_unlock(&plugin->mtx);

    if (flushed) {
        neu_msleep(FLUSH_WAIT_MS);
    }
    client_free(client);

    pthread_mutex_lock(&plugin->mtx);
    mqtt_v2_devices_free_locked(plugin);
    free(plugin->prefix);
    free(plugin->state_topic);
    plugin->prefix      = NULL;
    plugin->state_topic = NULL;
    pthread_mutex_unlock(&plugin->mtx);

    mqtt_v2_config_fini(&plugin->config);

    plog_notice(plugin, "uninitialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int plugin_config(neu_plugin_t *plugin, const char *setting)
{
    mqtt_v2_config_t config  = { 0 };
    mqtt_v2_config_t old     = { 0 };
    bool             flushed = false;
    int              rv      = 0;

    if (0 != mqtt_v2_config_parse(plugin, setting, &config)) {
        plog_error(plugin, "config plugin `%s` fail",
                   neu_plugin_module.module_name);
        return NEU_ERR_NODE_SETTING_INVALID;
    }

    pthread_mutex_lock(&plugin->mtx);
    // retained states under topics a new prefix abandons would go stale
    if (NULL != plugin->config.topic_prefix &&
        0 != strcmp(plugin->config.topic_prefix, config.topic_prefix)) {
        flushed = mqtt_v2_retire_topics_locked(plugin);
    }
    old            = plugin->config;
    plugin->config = config;
    rv             = mqtt_v2_set_topics_locked(plugin);
    pthread_mutex_unlock(&plugin->mtx);

    mqtt_v2_config_fini(&old);

    if (0 != rv) {
        plog_error(plugin, "build topics fail");
        return NEU_ERR_EINTERNAL;
    }

    if (flushed) {
        neu_msleep(FLUSH_WAIT_MS);
    }

    rv = client_rebuild(plugin);
    if (NEU_ERR_SUCCESS != rv) {
        plog_error(plugin, "config plugin `%s` fail, error:%d",
                   neu_plugin_module.module_name, rv);
        return rv;
    }

    plog_notice(plugin, "config plugin `%s` success, state topic: %s",
                neu_plugin_module.module_name, plugin->state_topic);
    return NEU_ERR_SUCCESS;
}

static int plugin_start(neu_plugin_t *plugin)
{
    neu_mqtt_client_t *client = NULL;

    pthread_mutex_lock(&plugin->mtx);
    client          = plugin->client;
    plugin->started = NULL != client;
    pthread_mutex_unlock(&plugin->mtx);

    if (NULL == client) {
        plog_error(plugin, "mqtt client is NULL due to init failure");
        return NEU_ERR_MQTT_IS_NULL;
    }

    if (NULL == plugin->timer) {
        neu_event_timer_param_t param = {
            .second      = STATE_POLL_INTERVAL_S,
            .millisecond = 0,
            .cb          = timer_cb,
            .usr_data    = plugin,
        };
        plugin->timer = neu_event_add_timer(plugin->events, param);
    }

    if (NULL == plugin->timer || 0 != neu_mqtt_client_open(client)) {
        plog_error(plugin, "start plugin `%s` fail",
                   neu_plugin_module.module_name);
        timer_stop(plugin);
        pthread_mutex_lock(&plugin->mtx);
        plugin->started = false;
        pthread_mutex_unlock(&plugin->mtx);
        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }

    mqtt_v2_request_tags(plugin);

    plog_notice(plugin, "start plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int plugin_stop(neu_plugin_t *plugin)
{
    neu_mqtt_client_t *client  = NULL;
    bool               flushed = false;

    timer_stop(plugin);

    pthread_mutex_lock(&plugin->mtx);
    plugin->started = false;
    client          = plugin->client;
    if (NULL != client && neu_mqtt_client_is_connected(client)) {
        // a clean disconnect discards the will
        mqtt_v2_publish_app_state_locked(plugin, false);
        flushed = true;
    }
    pthread_mutex_unlock(&plugin->mtx);

    if (flushed) {
        neu_msleep(FLUSH_WAIT_MS);
    }
    if (NULL != client) {
        client_close(client);
    }
    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;

    plog_notice(plugin, "stop plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                          void *data)
{
    int rv = NEU_ERR_SUCCESS;

    if ((global_timestamp - plugin->cache_metric_update_ts) >= 1000) {
        pthread_mutex_lock(&plugin->mtx);
        if (NULL != plugin->client) {
            NEU_PLUGIN_UPDATE_METRIC(
                plugin, NEU_METRIC_CACHED_MSGS_NUM,
                neu_mqtt_client_get_cached_msgs_num(plugin->client), NULL);
        }
        pthread_mutex_unlock(&plugin->mtx);
        plugin->cache_metric_update_ts = global_timestamp;
    }

    // renaming an app does not notify its plugin
    if (NULL != plugin->prefix &&
        0 != strcmp(plugin->app, plugin->common.name)) {
        app_renamed(plugin);
    }

    switch (head->type) {
    case NEU_REQRESP_TRANS_DATA:
        pthread_mutex_lock(&plugin->mtx);
        if (NULL != plugin->client && neu_mqtt_client_is_open(plugin->client)) {
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 1, NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_30S, 1,
                                     NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_60S, 1,
                                     NULL);
        }
        pthread_mutex_unlock(&plugin->mtx);
        rv = mqtt_v2_handle_trans_data(plugin, data);
        break;
    case NEU_REQ_SUBSCRIBE_GROUP:
        rv = mqtt_v2_handle_subscribe(plugin, data, false);
        break;
    case NEU_REQ_UPDATE_SUBSCRIBE_GROUP:
        rv = mqtt_v2_handle_subscribe(plugin, data, true);
        break;
    case NEU_REQ_UNSUBSCRIBE_GROUP: {
        const neu_req_unsubscribe_t *req = data;
        rv = mqtt_v2_handle_unsubscribe(plugin, req->driver, req->group);
        break;
    }
    case NEU_REQ_DEL_GROUP: {
        const neu_req_del_group_t *req = data;
        rv = mqtt_v2_handle_unsubscribe(plugin, req->driver, req->group);
        break;
    }
    case NEU_REQ_UPDATE_GROUP:
        rv = mqtt_v2_handle_update_group(plugin, data);
        break;
    case NEU_REQ_UPDATE_NODE:
        rv = mqtt_v2_handle_update_driver(plugin, data);
        break;
    case NEU_REQRESP_NODE_DELETED: {
        const neu_reqresp_node_deleted_t *req = data;
        if (0 == strcmp(plugin->common.name, req->node)) {
            pthread_mutex_lock(&plugin->mtx);
            plugin->deleted = true;
            pthread_mutex_unlock(&plugin->mtx);
        } else {
            rv = mqtt_v2_handle_del_driver(plugin, req->node);
        }
        break;
    }
    case NEU_RESP_GET_NODES_STATE:
        rv = mqtt_v2_handle_nodes_state(plugin, data);
        break;
    case NEU_RESP_GET_SUB_DRIVER_TAGS:
        rv = mqtt_v2_handle_sub_tags(plugin, data);
        break;
    case NEU_RESP_ERROR: {
        mqtt_v2_write_ctx_t *ctx = head->ctx;
        if (NULL != ctx && MQTT_V2_WRITE_CTX_MAGIC == ctx->magic) {
            const neu_resp_error_t *resp = data;
            rv = mqtt_v2_handle_write_resp(plugin, ctx, resp->error, NULL);
        }
        break;
    }
    case NEU_RESP_WRITE_TAGS: {
        neu_resp_write_tags_t *resp = data;
        mqtt_v2_write_ctx_t *  ctx  = head->ctx;
        if (NULL != ctx && MQTT_V2_WRITE_CTX_MAGIC == ctx->magic) {
            rv = mqtt_v2_handle_write_resp(plugin, ctx, 0, resp->tags);
        } else {
            utarray_free(resp->tags);
        }
        break;
    }
    default:
        rv = NEU_ERR_MQTT_FAILURE;
        break;
    }

    return rv;
}

static int mqtt_v2_plugin_uninit(neu_plugin_t *plugin)
{
    pthread_mutex_lock(&plugin->life_mtx);
    int rv = plugin_uninit(plugin);
    pthread_mutex_unlock(&plugin->life_mtx);
    return rv;
}

static int mqtt_v2_plugin_config(neu_plugin_t *plugin, const char *setting)
{
    pthread_mutex_lock(&plugin->life_mtx);
    int rv = plugin_config(plugin, setting);
    pthread_mutex_unlock(&plugin->life_mtx);
    return rv;
}

static int mqtt_v2_plugin_start(neu_plugin_t *plugin)
{
    pthread_mutex_lock(&plugin->life_mtx);
    int rv = plugin_start(plugin);
    pthread_mutex_unlock(&plugin->life_mtx);
    return rv;
}

static int mqtt_v2_plugin_stop(neu_plugin_t *plugin)
{
    pthread_mutex_lock(&plugin->life_mtx);
    int rv = plugin_stop(plugin);
    pthread_mutex_unlock(&plugin->life_mtx);
    return rv;
}

static int mqtt_v2_plugin_request(neu_plugin_t *      plugin,
                                  neu_reqresp_head_t *head, void *data)
{
    pthread_mutex_lock(&plugin->life_mtx);
    int rv = plugin_request(plugin, head, data);
    pthread_mutex_unlock(&plugin->life_mtx);
    return rv;
}

const neu_plugin_intf_funs_t mqtt_v2_plugin_intf_funs = {
    .open        = mqtt_v2_plugin_open,
    .close       = mqtt_v2_plugin_close,
    .init        = mqtt_v2_plugin_init,
    .uninit      = mqtt_v2_plugin_uninit,
    .start       = mqtt_v2_plugin_start,
    .stop        = mqtt_v2_plugin_stop,
    .setting     = mqtt_v2_plugin_config,
    .request     = mqtt_v2_plugin_request,
    .try_connect = NULL,
};
