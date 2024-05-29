/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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

#include "utils/asprintf.h"
#include "utils/time.h"

#include "mqtt_config.h"
#include "mqtt_handle.h"
#include "mqtt_plugin.h"

extern const neu_plugin_module_t neu_plugin_module;

static int subscribe(neu_plugin_t *plugin, const mqtt_config_t *config);
static int unsubscribe(neu_plugin_t *plugin, const mqtt_config_t *config);

static void connect_cb(void *data)
{
    neu_plugin_t *plugin      = data;
    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    plog_notice(plugin, "plugin `%s` connected", neu_plugin_module.module_name);
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
}

neu_plugin_t *mqtt_plugin_open(void)
{
    neu_plugin_t *plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    neu_plugin_common_init(&plugin->common);
    plugin->parse_config = mqtt_config_parse;
    plugin->subscribe    = subscribe;
    plugin->unsubscribe  = unsubscribe;
    return plugin;
}

int mqtt_plugin_close(neu_plugin_t *plugin)
{
    const char *name = neu_plugin_module.module_name;
    plog_notice(plugin, "success to free plugin:%s", name);

    free(plugin);
    return NEU_ERR_SUCCESS;
}

int mqtt_plugin_init(neu_plugin_t *plugin, bool load)
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

    plog_notice(plugin, "initialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    mqtt_config_fini(&plugin->config);
    if (plugin->client) {
        neu_mqtt_client_close(plugin->client);
        neu_mqtt_client_free(plugin->client);
        plugin->client = NULL;
    }

    free(plugin->read_req_topic);
    plugin->read_req_topic = NULL;
    free(plugin->read_resp_topic);
    plugin->read_resp_topic = NULL;
    free(plugin->upload_topic);
    plugin->upload_topic = NULL;

    route_tbl_free(plugin->route_tbl);

    plog_notice(plugin, "uninitialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int config_mqtt_client(neu_plugin_t *plugin, neu_mqtt_client_t *client,
                              const mqtt_config_t *config)
{
    int rv = 0;

    if (NULL == client) {
        return 0;
    }

    // set log category as soon as possible to ease debugging
    rv = neu_mqtt_client_set_zlog_category(client, plugin->common.log);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_zlog_category fail");
        return -1;
    }

    rv = neu_mqtt_client_set_addr(client, config->host, config->port);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_host fail");
        return -1;
    }

    rv = neu_mqtt_client_set_id(client, config->client_id);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_id fail");
        return -1;
    }

    rv = neu_mqtt_client_set_connect_cb(client, connect_cb, plugin);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_connect_cb fail");
        return -1;
    }

    rv = neu_mqtt_client_set_disconnect_cb(client, disconnect_cb, plugin);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_disconnect_cb fail");
        return -1;
    }

    rv = neu_mqtt_client_set_cache_size(client, config->cache_mem_size,
                                        config->cache_disk_size);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_msg_cache_limit fail");
        return -1;
    }

    rv = neu_mqtt_client_set_cache_sync_interval(client,
                                                 config->cache_sync_interval);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_msg_cache_sync_interval fail");
        return -1;
    }

    if (NULL != config->username) {
        rv = neu_mqtt_client_set_user(client, config->username,
                                      config->password);
        if (0 != rv) {
            plog_error(plugin, "neu_mqtt_client_set_user fail");
        }
    }

    rv = neu_mqtt_client_set_tls(client, config->ssl, config->ca, config->cert,
                                 config->key, config->keypass);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_tsl fail");
        return -1;
    }

    return rv;
}

static int create_topic(neu_plugin_t *plugin)
{
    if (plugin->read_req_topic) {
        // topics already created
        return 0;
    }

    neu_asprintf(&plugin->read_req_topic, "/neuron/%s/read/req",
                 plugin->common.name);
    if (NULL == plugin->read_req_topic) {
        return -1;
    }

    neu_asprintf(&plugin->read_resp_topic, "/neuron/%s/read/resp",
                 plugin->common.name);
    if (NULL == plugin->read_resp_topic) {
        free(plugin->read_req_topic);
        plugin->read_req_topic = NULL;
        return -1;
    }

    return 0;
}

static int subscribe(neu_plugin_t *plugin, const mqtt_config_t *config)
{
    if (0 != create_topic(plugin)) {
        plog_error(plugin, "create topics fail");
        return NEU_ERR_EINTERNAL;
    }

    if (0 !=
        neu_mqtt_client_subscribe(plugin->client, config->qos,
                                  plugin->read_req_topic, plugin,
                                  handle_read_req)) {
        plog_error(plugin, "subscribe [%s] fail", plugin->read_req_topic);
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    if (0 !=
        neu_mqtt_client_subscribe(plugin->client, config->qos,
                                  config->write_req_topic, plugin,
                                  handle_write_req)) {
        plog_error(plugin, "subscribe [%s] fail",
                   plugin->config.write_req_topic);
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    return 0;
}

static int unsubscribe(neu_plugin_t *plugin, const mqtt_config_t *config)
{
    neu_mqtt_client_unsubscribe(plugin->client, plugin->read_req_topic);
    neu_mqtt_client_unsubscribe(plugin->client, config->write_req_topic);
    neu_msleep(100); // wait for message completion
    return 0;
}

int mqtt_plugin_config(neu_plugin_t *plugin, const char *setting)
{
    int           rv          = 0;
    const char *  plugin_name = neu_plugin_module.module_name;
    mqtt_config_t config      = { 0 };
    bool          started     = false;

    rv = plugin->parse_config(plugin, setting, &config);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_config_parse fail");
        return NEU_ERR_NODE_SETTING_INVALID;
    }

    if (NULL == plugin->client) {
        plugin->client = neu_mqtt_client_new(NEU_MQTT_VERSION_V311);
        if (NULL == plugin->client) {
            plog_error(plugin, "neu_mqtt_client_new fail");
            rv = NEU_ERR_EINTERNAL;
            goto error;
        }
    } else if (neu_mqtt_client_is_open(plugin->client)) {
        started = true;
        plugin->unsubscribe(plugin, &plugin->config);
        rv = neu_mqtt_client_close(plugin->client);
        if (0 != rv) {
            plog_error(plugin, "neu_mqtt_client_close fail");
            rv = NEU_ERR_EINTERNAL;
            goto error;
        }
    }

    rv = config_mqtt_client(plugin, plugin->client, &config);
    if (0 != rv) {
        rv = NEU_ERR_MQTT_INIT_FAILURE;
        goto error;
    }

    if (started) {
        if (0 != neu_mqtt_client_open(plugin->client)) {
            plog_error(plugin, "neu_mqtt_client_open fail");
            rv = NEU_ERR_MQTT_CONNECT_FAILURE;
            goto error;
        }
        if (0 != (rv = plugin->subscribe(plugin, &config))) {
            goto error;
        }
    }

    if (plugin->config.host) {
        // already configured
        mqtt_config_fini(&plugin->config);
    }
    memmove(&plugin->config, &config, sizeof(config));

    plog_notice(plugin, "config plugin `%s` success", plugin_name);
    return 0;

error:
    plog_error(plugin, "config plugin `%s` fail", plugin_name);
    mqtt_config_fini(&config);
    return rv;
}

int mqtt_plugin_start(neu_plugin_t *plugin)
{
    int         rv          = 0;
    const char *plugin_name = neu_plugin_module.module_name;

    if (NULL == plugin->client) {
        plog_error(plugin, "mqtt client is NULL due to init failure");
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    if (0 != neu_mqtt_client_open(plugin->client)) {
        plog_error(plugin, "neu_mqtt_client_open fail");
        rv = NEU_ERR_MQTT_CONNECT_FAILURE;
        goto end;
    }

    rv = plugin->subscribe(plugin, &plugin->config);

end:
    if (0 == rv) {
        plog_notice(plugin, "start plugin `%s` success", plugin_name);
    } else {
        plog_error(plugin, "start plugin `%s` failed, error %d", plugin_name,
                   rv);
        neu_mqtt_client_close(plugin->client);
    }
    return rv;
}

int mqtt_plugin_stop(neu_plugin_t *plugin)
{
    if (plugin->client) {
        plugin->unsubscribe(plugin, &plugin->config);
        neu_mqtt_client_close(plugin->client);
        plog_notice(plugin, "mqtt client closed");
    }

    plog_notice(plugin, "stop plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

int mqtt_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                        void *data)
{
    neu_err_code_e error = NEU_ERR_SUCCESS;

    // update cached messages number per seconds
    if (NULL != plugin->client &&
        (global_timestamp - plugin->cache_metric_update_ts) >= 1000) {
        NEU_PLUGIN_UPDATE_METRIC(
            plugin, NEU_METRIC_CACHED_MSGS_NUM,
            neu_mqtt_client_get_cached_msgs_num(plugin->client), NULL);
        plugin->cache_metric_update_ts = global_timestamp;
    }

    switch (head->type) {
    case NEU_RESP_ERROR:
        error = handle_write_response(plugin, head->ctx, data);
        break;
    case NEU_RESP_READ_GROUP:
        error = handle_read_response(plugin, head->ctx, data);
        break;
    case NEU_REQRESP_TRANS_DATA: {
        if (plugin->client && neu_mqtt_client_is_open(plugin->client)) {
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 1, NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_30S, 1,
                                     NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_60S, 1,
                                     NULL);
        }
        error = handle_trans_data(plugin, data);
        break;
    }
    case NEU_REQ_SUBSCRIBE_GROUP:
        error = handle_subscribe_group(plugin, data);
        break;
    case NEU_REQ_UPDATE_SUBSCRIBE_GROUP:
        error = handle_update_subscribe(plugin, data);
        break;
    case NEU_REQ_UNSUBSCRIBE_GROUP:
        error = handle_unsubscribe_group(plugin, data);
        break;
    case NEU_REQ_UPDATE_GROUP:
        error = handle_update_group(plugin, data);
        break;
    case NEU_REQ_DEL_GROUP:
        error = handle_del_group(plugin, data);
        break;
    case NEU_REQ_UPDATE_NODE:
        error = handle_update_driver(plugin, data);
        break;
    case NEU_REQRESP_NODE_DELETED:
        error = handle_del_driver(plugin, data);
        break;
    default:
        error = NEU_ERR_MQTT_FAILURE;
        break;
    }

    return error;
}

const neu_plugin_intf_funs_t mqtt_plugin_intf_funs = {
    .open    = mqtt_plugin_open,
    .close   = mqtt_plugin_close,
    .init    = mqtt_plugin_init,
    .uninit  = mqtt_plugin_uninit,
    .start   = mqtt_plugin_start,
    .stop    = mqtt_plugin_stop,
    .setting = mqtt_plugin_config,
    .request = mqtt_plugin_request,
};
