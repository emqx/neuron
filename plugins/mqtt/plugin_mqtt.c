/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
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
#include <pthread.h>
#include <stdlib.h>

#include <config.h>
#include <neuron.h>

#include "message_handle.h"

#include "option.h"
#include "paho_client.h"

#define UNUSED(x) (void) (x)

const neu_plugin_module_t neu_plugin_module;

struct neu_plugin {
    neu_plugin_common_t common;
    option_t            option;
    paho_client_t *     paho;
    neu_list            context_list;
};

static int plugin_subscribe(neu_plugin_t *plugin, const char *topic,
                            const int qos, subscribe_handle handle)
{
    client_error error =
        paho_client_subscribe(plugin->paho, topic, qos, handle);
    if (ClientSubscribeFailure == error ||
        ClientSubscribeAddListFailure == error) {
        neu_panic("Subscribe Failure");
    }
    return 0;
}

static void plugin_response_handle(const char *topic_name, size_t topic_len,
                                   void *payload, const size_t len,
                                   void *context)
{
    neu_plugin_t *plugin = (neu_plugin_t *) context;

    if (NULL == topic_name || NULL == payload) {
        return;
    }

    char *json_str = malloc(len + 1);
    if (NULL == json_str) {
        return;
    }

    memset(json_str, 0x00, len + 1);
    memcpy(json_str, payload, len);

    void *            result = NULL;
    neu_parse_mqtt_t *mqtt   = NULL;
    int               rc     = neu_parse_decode_mqtt_param(json_str, &mqtt);

    if (0 != rc) {
        log_error("JSON parsing matt failed");
        return;
    }

    switch (mqtt->function) {
    case NEU_MQTT_OP_GET_GROUP_CONFIG:
        rc = neu_parse_decode_get_group_config(
            json_str, (neu_parse_get_group_config_req_t **) &result);
        break;
    case NEU_MQTT_OP_ADD_GROUP_CONFIG:
        rc = neu_parse_decode_add_group_config(
            json_str, (neu_parse_add_group_config_req_t **) &result);
        break;
    case NEU_MQTT_OP_UPDATE_GROUP_CONFIG:
        rc = neu_parse_decode_update_group_config(
            json_str, (neu_parse_update_group_config_req_t **) &result);
        break;
    case NEU_MQTT_OP_DELETE_GROUP_CONFIG:
        rc = neu_parse_decode_del_group_config(
            json_str, (neu_parse_del_group_config_req_t **) &result);
        break;
    case NEU_MQTT_OP_ADD_DATATAG_IDS_CONFIG:
        rc = -1;
        break;
    case NEU_MQTT_OP_DELETE_DATATAG_IDS_CONFIG:
        rc = -1;
        break;
    case NEU_MQTT_OP_READ:
        rc = neu_parse_decode_read(json_str, (neu_parse_read_req_t **) &result);
        break;
    case NEU_MQTT_OP_WRITE:
        rc = neu_parse_decode_write(json_str,
                                    (neu_parse_write_req_t **) &result);
        break;
    case NEU_MQTT_OP_GET_TAGS:
        rc = neu_parse_decode_get_tags(json_str,
                                       (neu_parse_get_tags_req_t **) &result);
        break;
    case NEU_MQTT_OP_ADD_TAGS:
        rc = neu_parse_decode_add_tags(json_str,
                                       (neu_parse_add_tags_req_t **) &result);
        break;
    default:
        rc = -1;
        break;
    }

    if (0 != rc) {
        log_error("JSON parsing matt failed");
        return;
    }

    switch (mqtt->function) {
    case NEU_MQTT_OP_GET_GROUP_CONFIG:
        message_handle_get_group_config(
            plugin, mqtt, (neu_parse_get_group_config_req_t *) result);
        neu_parse_decode_get_group_config_free(result);
        break;
    case NEU_MQTT_OP_ADD_GROUP_CONFIG:
        message_handle_add_group_config(
            plugin, mqtt, (neu_parse_add_group_config_req_t *) result);
        neu_parse_decode_add_group_config_free(result);
        break;
    case NEU_MQTT_OP_UPDATE_GROUP_CONFIG:
        message_handle_update_group_config(
            plugin, mqtt, (neu_parse_update_group_config_req_t *) result);
        neu_parse_decode_update_group_config_free(result);
        break;
    case NEU_MQTT_OP_DELETE_GROUP_CONFIG:
        message_handle_delete_group_config(
            plugin, mqtt, (neu_parse_del_group_config_req_t *) result);
        neu_parse_decode_del_group_config_free(result);
        break;
    case NEU_MQTT_OP_ADD_DATATAG_IDS_CONFIG:
        // message_handle_add_datatag_ids(
        // plugin, (neu_parse_add_tag_ids_req_t *) result);
        break;
    case NEU_MQTT_OP_DELETE_DATATAG_IDS_CONFIG:
        // message_handle_delete_datatag_ids(
        // plugin, (struct neu_parse_delete_tag_ids_req *) result);
        break;
    case NEU_MQTT_OP_READ:
        message_handle_read(plugin, mqtt, (neu_parse_read_req_t *) result);
        neu_parse_decode_read_free(result);
        break;
    case NEU_MQTT_OP_WRITE:
        message_handle_write(plugin, mqtt, (neu_parse_write_req_t *) result);
        neu_parse_decode_write_free(result);
        break;
    case NEU_MQTT_OP_GET_TAGS:
        message_handle_get_tag_list(plugin, mqtt,
                                    (neu_parse_get_tags_req_t *) result);
        neu_parse_decode_get_tags_free(result);
        break;
    case NEU_MQTT_OP_ADD_TAGS:
        message_handle_add_tag(plugin, mqtt,
                               (neu_parse_add_tags_req_t *) result);
        neu_parse_decode_add_tags_free(result);
        break;
    default:
        break;
    }

    free(json_str);

    UNUSED(topic_len);
}

static neu_plugin_t *mqtt_plugin_open(neu_adapter_t *            adapter,
                                      const adapter_callbacks_t *callbacks)
{
    if (NULL == adapter || NULL == callbacks) {
        return NULL;
    }

    neu_plugin_t *plugin;
    plugin = (neu_plugin_t *) malloc(sizeof(neu_plugin_t));
    if (NULL == plugin) {
        log_error("Failed to allocate plugin %s",
                  neu_plugin_module.module_name);
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);
    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    log_info("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;
}

static int mqtt_plugin_close(neu_plugin_t *plugin)
{
    free(plugin);
    log_info("Success to free plugin:%s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_init(neu_plugin_t *plugin)
{
    const char *clientid =
        neu_config_get_value("./neuron.yaml", 2, "mqtt", "id");
    const char *mqtt_version =
        neu_config_get_value("./neuron.yaml", 2, "mqtt", "mqtt_version");
    const char *topic =
        neu_config_get_value("./neuron.yaml", 2, "mqtt", "topic");
    const char *qos = neu_config_get_value("./neuron.yaml", 2, "mqtt", "qos");
    const char *keepalive_interval =
        neu_config_get_value("./neuron.yaml", 2, "mqtt", "keepalive_interval");
    const char *clean_session =
        neu_config_get_value("./neuron.yaml", 2, "mqtt", "clean_session");
    const char *connection = neu_config_get_value("./neuron.yaml", 3, "mqtt",
                                                  "broker", "connection");
    const char *host =
        neu_config_get_value("./neuron.yaml", 3, "mqtt", "broker", "host");
    const char *port =
        neu_config_get_value("./neuron.yaml", 3, "mqtt", "broker", "port");

    // MQTT option
    plugin->option.clientid           = strdup(clientid);
    plugin->option.MQTT_version       = atoi(mqtt_version);
    plugin->option.topic              = strdup(topic);
    plugin->option.qos                = atoi(qos);
    plugin->option.connection         = strdup(connection);
    plugin->option.host               = strdup(host);
    plugin->option.port               = strdup(port);
    plugin->option.keepalive_interval = atoi(keepalive_interval);
    plugin->option.clean_session      = atoi(clean_session);

    // Paho mqtt client setup
    client_error error =
        paho_client_open(&plugin->option, plugin, &plugin->paho);
    if (ClientIsNULL == error) {
        return -1;
    }

    plugin_subscribe(plugin, "neuronlite/request", 0, plugin_response_handle);
    log_info("Initialize plugin: %s", neu_plugin_module.module_name);

    message_handle_set_paho_client(plugin->paho);
    return 0;
}

static int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    paho_client_close(plugin->paho);

    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    message_handle_init_tags(plugin);

    UNUSED(plugin);

    UNUSED(configs);

    log_info("config plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    int rv = 0;

    if (plugin == NULL || req == NULL) {
        log_warn("The plugin pointer or request is NULL");
        return (-1);
    }

    log_info("send request to plugin: %s", neu_plugin_module.module_name);

    switch (req->req_type) {
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_data_t *neu_data;
        assert(req->buf_len == sizeof(neu_reqresp_data_t));
        neu_data                 = (neu_reqresp_data_t *) req->buf;
        neu_variable_t *variable = NULL; // neu_data->data_var;
        size_t          count    = neu_variable_count(variable);
        log_debug("variable count: %ld", count);
        if (0 < count) {
            message_handle_read_result(plugin, neu_data->grp_config, variable);
        }
        break;
    }

    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_data_t *neu_data;
        assert(req->buf_len == sizeof(neu_reqresp_data_t));
        neu_data                 = (neu_reqresp_data_t *) req->buf;
        neu_variable_t *variable = NULL; // neu_data->data_var;
        size_t          count    = neu_variable_count(variable);
        if (0 < count) { }
        break;
    }

    default:
        break;
    }
    return rv;
}

static int mqtt_plugin_event_reply(neu_plugin_t *     plugin,
                                   neu_event_reply_t *reply)
{
    UNUSED(plugin);
    UNUSED(reply);
    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open        = mqtt_plugin_open,
    .close       = mqtt_plugin_close,
    .init        = mqtt_plugin_init,
    .uninit      = mqtt_plugin_uninit,
    .config      = mqtt_plugin_config,
    .request     = mqtt_plugin_request,
    .event_reply = mqtt_plugin_event_reply
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-mqtt-plugin",
    .module_descr = "Neuron northbound MQTT communication plugin",
    .intf_funs    = &plugin_intf_funs
};
