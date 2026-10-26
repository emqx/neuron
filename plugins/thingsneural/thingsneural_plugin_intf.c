/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 **/

#include <stdlib.h>
#include <string.h>

#include "thingsneural_config.h"
#include "thingsneural_handle.h"
#include "thingsneural_plugin.h"
#include "thingsneural_plugin_intf.h"

extern const neu_plugin_module_t neu_plugin_module;

static void connect_cb(void *data)
{
    neu_plugin_t *plugin      = data;
    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    plog_notice(plugin, "plugin `%s` connected", neu_plugin_module.module_name);
    if (0 != tn_subscribe_downlink(plugin)) {
        plog_error(plugin, "subscribe property set topic fail");
    }
}

static void disconnect_cb(void *data)
{
    neu_plugin_t *plugin      = data;
    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    plog_notice(plugin, "plugin `%s` disconnected",
                neu_plugin_module.module_name);
}

static int open_client(neu_plugin_t *plugin, const tn_config_t *config)
{
    int                rv     = 0;
    neu_mqtt_client_t *client = neu_mqtt_client_new(config->version);

    if (NULL == client) {
        return -1;
    }

    rv = neu_mqtt_client_set_zlog_category(client, plugin->common.log);
    if (0 != rv) {
        goto error;
    }
    rv = neu_mqtt_client_set_addr(client, config->host, config->port);
    if (0 != rv) {
        goto error;
    }
    rv = neu_mqtt_client_set_id(client, config->client_id);
    if (0 != rv) {
        goto error;
    }
    rv = neu_mqtt_client_set_connect_cb(client, connect_cb, plugin);
    if (0 != rv) {
        goto error;
    }
    rv = neu_mqtt_client_set_disconnect_cb(client, disconnect_cb, plugin);
    if (0 != rv) {
        goto error;
    }
    if (NULL != config->username) {
        rv = neu_mqtt_client_set_user(client, config->username,
                                      config->password);
        if (0 != rv) {
            goto error;
        }
    }
    rv = neu_mqtt_client_set_tls(client, config->ssl, NULL, NULL, NULL, NULL);
    if (0 != rv) {
        goto error;
    }
    if (0 != neu_mqtt_client_open(client)) {
        goto error;
    }

    plugin->client = client;
    return 0;

error:
    neu_mqtt_client_free(client);
    return -1;
}

static void close_client(neu_plugin_t *plugin)
{
    if (NULL == plugin->client) {
        return;
    }
    if (NULL != plugin->config.set_req_topic) {
        neu_mqtt_client_unsubscribe(plugin->client,
                                    plugin->config.set_req_topic);
    }
    neu_mqtt_client_close(plugin->client);
    neu_mqtt_client_free(plugin->client);
    plugin->client            = NULL;
    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
}

neu_plugin_t *tn_plugin_open(void)
{
    neu_plugin_t *plugin = calloc(1, sizeof(neu_plugin_t));
    neu_plugin_common_init(&plugin->common);
    pthread_mutex_init(&plugin->lock, NULL);
    return plugin;
}

int tn_plugin_close(neu_plugin_t *plugin)
{
    pthread_mutex_destroy(&plugin->lock);
    free(plugin);
    return NEU_ERR_SUCCESS;
}

int tn_plugin_init(neu_plugin_t *plugin, bool load)
{
    (void) load;
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_BYTES_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_MSGS_TOTAL, 0);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 0);
    plog_notice(plugin, "initialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

int tn_plugin_uninit(neu_plugin_t *plugin)
{
    close_client(plugin);
    tn_config_fini(&plugin->config);
    pthread_mutex_lock(&plugin->lock);
    tn_points_free(&plugin->points);
    pthread_mutex_unlock(&plugin->lock);
    plog_notice(plugin, "uninitialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

int tn_plugin_config(neu_plugin_t *plugin, const char *setting)
{
    tn_config_t config = { 0 };
    tn_point_t *points = NULL;

    if (0 != tn_config_parse(plugin, setting, &config, &points)) {
        return NEU_ERR_NODE_SETTING_INVALID;
    }

    close_client(plugin);
    if (0 != open_client(plugin, &config)) {
        tn_config_fini(&config);
        tn_points_free(&points);
        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }

    tn_config_fini(&plugin->config);
    pthread_mutex_lock(&plugin->lock);
    tn_points_free(&plugin->points);
    plugin->points = points;
    pthread_mutex_unlock(&plugin->lock);
    plugin->config = config;

    if (0 != tn_subscribe_downlink(plugin)) {
        plog_error(plugin, "subscribe property set topic fail");
    }

    plog_notice(plugin, "config plugin `%s` success, report:%s",
                neu_plugin_module.module_name, plugin->config.report_topic);
    return NEU_ERR_SUCCESS;
}

int tn_plugin_start(neu_plugin_t *plugin)
{
    if (NULL == plugin->client) {
        return NEU_ERR_MQTT_IS_NULL;
    }
    if (0 != neu_mqtt_client_open(plugin->client)) {
        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }
    if (0 != tn_subscribe_downlink(plugin)) {
        plog_error(plugin, "subscribe property set topic fail");
    }
    plog_notice(plugin, "start plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

int tn_plugin_stop(neu_plugin_t *plugin)
{
    if (NULL != plugin->client && NULL != plugin->config.set_req_topic) {
        neu_mqtt_client_unsubscribe(plugin->client,
                                    plugin->config.set_req_topic);
        neu_mqtt_client_close(plugin->client);
    }
    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    plog_notice(plugin, "stop plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int tn_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                             void *data)
{
    switch (head->type) {
    case NEU_REQRESP_TRANS_DATA:
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 1, NULL);
        return tn_handle_trans_data(plugin, data);
    case NEU_REQ_SUBSCRIBE_GROUP:
        return tn_handle_subscribe_group(plugin, data);
    case NEU_REQ_UPDATE_SUBSCRIBE_GROUP:
        return tn_handle_update_subscribe(plugin, data);
    case NEU_REQ_UNSUBSCRIBE_GROUP:
        return tn_handle_unsubscribe_group(plugin, data);
    case NEU_REQ_UPDATE_GROUP:
        return tn_handle_update_group(plugin, data);
    case NEU_REQ_DEL_GROUP:
        return tn_handle_del_group(plugin, data);
    case NEU_REQ_UPDATE_NODE:
        return tn_handle_update_driver(plugin, data);
    case NEU_REQRESP_NODE_DELETED: {
        neu_reqresp_node_deleted_t *req = data;
        if (0 != strcmp(plugin->common.name, req->node)) {
            return tn_handle_del_driver(plugin, data);
        }
        return NEU_ERR_SUCCESS;
    }
    default:
        return NEU_ERR_PLUGIN_APP_NOT_RUNNING;
    }
}

const neu_plugin_intf_funs_t tn_plugin_intf_funs = {
    .open        = tn_plugin_open,
    .close       = tn_plugin_close,
    .init        = tn_plugin_init,
    .uninit      = tn_plugin_uninit,
    .start       = tn_plugin_start,
    .stop        = tn_plugin_stop,
    .setting     = tn_plugin_config,
    .request     = tn_plugin_request,
    .try_connect = NULL,
};
