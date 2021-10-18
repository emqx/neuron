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

#include "command_datatag.h"
#include "command_group_config.h"
#include "command_node.h"
#include "command_rw.h"

#include "option.h"
#include "paho_client.h"

#define UNUSED(x) (void) (x)

#define MQTT_SEND(client, topic, qos, json_str)                           \
    {                                                                     \
        if (NULL != json_str) {                                           \
            client_error error = paho_client_publish(                     \
                client, topic, qos, (unsigned char *) json_str,           \
                strlen(json_str));                                        \
            log_debug("Publish error code:%d, json:%s", error, json_str); \
            free(json_str);                                               \
        }                                                                 \
    }

const neu_plugin_module_t neu_plugin_module;

struct context {
    neu_list_node    node;
    int              req_id;
    neu_parse_mqtt_t parse_header;
};

struct neu_plugin {
    neu_plugin_common_t common;
    option_t            option;
    paho_client_t *     paho;
    neu_list            context_list;
    neu_parse_mqtt_t *  parse_header;
};

struct context *context_create()
{
    struct context *ctx = NULL;
    ctx                 = malloc(sizeof(struct context));
    if (NULL != ctx) {
        memset(ctx, 0, sizeof(struct context));
    }
    return ctx;
}

void context_list_add(neu_list *list, struct context *ctx, int req_id,
                      neu_parse_mqtt_t *parse_header)
{
    ctx->req_id                = req_id;
    ctx->parse_header.function = parse_header->function;
    ctx->parse_header.uuid     = strdup(parse_header->uuid);

    NEU_LIST_NODE_INIT(&ctx->node);
    neu_list_append(list, ctx);
}

struct context *context_list_find(neu_list *list, const int id)
{
    struct context *item;
    NEU_LIST_FOREACH(list, item)
    {
        if (id == item->req_id) {
            return item;
        }
    }
    return NULL;
}

void context_destroy(struct context *ctx)
{
    if (NULL != ctx) {
        if (NULL != ctx->parse_header.uuid) {
            free(ctx->parse_header.uuid);
        }
        free(ctx);
    }
}

void context_list_remove(neu_list *list, struct context *ctx)
{
    UNUSED(list);
    UNUSED(ctx);
}

void context_list_destroy(neu_list *list)
{
    while (!neu_list_empty(list)) {
        struct context *ctx = neu_list_first(list);
        neu_list_remove(list, ctx);
        context_destroy(ctx);
    }
}

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

    neu_parse_mqtt_t *mqtt    = NULL;
    char *            ret_str = NULL;
    int               rc      = neu_parse_decode_mqtt_param(json_str, &mqtt);

    if (0 != rc) {
        log_error("JSON parsing mqtt failed");
        return;
    }

    plugin->parse_header = mqtt;

    switch (mqtt->function) {
    case NEU_MQTT_OP_GET_GROUP_CONFIG: {
        neu_parse_get_group_config_req_t *req = NULL;
        rc = neu_parse_decode_get_group_config(json_str, &req);
        if (0 == rc) {
            ret_str = command_get_group_configs(plugin, mqtt, req);
            neu_parse_decode_get_group_config_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_ADD_GROUP_CONFIG: {
        neu_parse_add_group_config_req_t *req = NULL;
        rc = neu_parse_decode_add_group_config(json_str, &req);
        if (0 == rc) {
            ret_str = command_add_group_config(plugin, mqtt, req);
            neu_parse_decode_add_group_config_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_UPDATE_GROUP_CONFIG: {
        neu_parse_update_group_config_req_t *req = NULL;
        rc = neu_parse_decode_update_group_config(json_str, &req);
        if (0 == rc) {
            ret_str = command_update_group_config(plugin, mqtt, req);
            neu_parse_decode_update_group_config_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_DELETE_GROUP_CONFIG: {
        neu_parse_del_group_config_req_t *req;
        rc = neu_parse_decode_del_group_config(json_str, &req);
        if (0 == rc) {
            ret_str = command_delete_group_config(plugin, mqtt, req);
            neu_parse_decode_del_group_config_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_READ: {
        neu_parse_read_req_t *req = NULL;
        rc                        = neu_parse_decode_read(json_str, &req);
        if (0 == rc) {
            int req_id = command_read_once_request(plugin, mqtt, req);
            UNUSED(req_id);
            neu_parse_decode_read_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_WRITE: {
        neu_parse_write_req_t *req = NULL;
        rc                         = neu_parse_decode_write(json_str, &req);
        if (0 == rc) {
            int req_id = command_write_request(plugin, mqtt, req);
            UNUSED(req_id);
            neu_parse_decode_write_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_GET_TAGS: {
        neu_parse_get_tags_req_t *req;
        rc = neu_parse_decode_get_tags(json_str, &req);
        if (0 == rc) {
            ret_str = command_get_tags(plugin, mqtt, req);
            neu_parse_decode_get_tags_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_ADD_TAGS: {
        neu_parse_add_tags_req_t *req = NULL;
        rc = neu_parse_decode_add_tags(json_str, &req);
        if (0 == rc) {
            ret_str = command_add_tags(plugin, mqtt, req);
            neu_parse_decode_add_tags_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_UPDATE_TAGS: {
        neu_parse_update_tags_req_t *req = NULL;
        rc = neu_parse_decode_update_tags(json_str, &req);
        if (0 == rc) {
            ret_str = command_update_tags(plugin, mqtt, req);
            neu_parse_decode_update_tags_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_DELETE_TAGS: {
        neu_parse_del_tags_req_t *req = NULL;
        rc = neu_parse_decode_del_tags(json_str, &req);
        if (0 == rc) {
            ret_str = command_delete_tags(plugin, mqtt, req);
            neu_parse_decode_del_tags_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_GET_NODES: {
        neu_parse_get_nodes_req_t *req = NULL;
        rc = neu_parse_decode_get_nodes(json_str, &req);
        if (0 == rc) {
            ret_str = command_get_nodes(plugin, mqtt, req);
            neu_parse_decode_get_nodes_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_ADD_NODES: {
        neu_parse_add_node_req_t *req = NULL;
        rc = neu_parse_decode_add_node(json_str, &req);
        if (0 == rc) {
            ret_str = command_add_node(plugin, mqtt, req);
            neu_parse_decode_add_node_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_UPDATE_NODES: {
        neu_parse_update_node_req_t *req = NULL;
        rc = neu_parse_decode_update_node(json_str, &req);
        if (0 == rc) {
            ret_str = command_update_node(plugin, mqtt, req);
            neu_parse_decode_update_node_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_DELETE_NODES: {
        neu_parse_del_node_req_t *req = NULL;
        rc = neu_parse_decode_del_node(json_str, &req);
        if (0 == rc) {
            ret_str = command_delete_node(plugin, mqtt, req);
            neu_parse_decode_del_node_free(req);
        }
        break;
    }
    default:
        break;
    }

    MQTT_SEND(plugin->paho, "neuronlite/response", 0, ret_str);

    // if (NULL != mqtt) {
    //     if (NULL != mqtt->uuid) {
    //         free(mqtt->uuid);
    //     }
    //     free(mqtt);
    // }

    if (NULL != json_str) {
        free(json_str);
    }

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

    plugin->parse_header = NULL;

    NEU_LIST_INIT(&plugin->context_list, struct context, node);

    // Paho mqtt client setup
    client_error error =
        paho_client_open(&plugin->option, plugin, &plugin->paho);
    if (ClientIsNULL == error) {
        return -1;
    }

    plugin_subscribe(plugin, "neuronlite/request", 0, plugin_response_handle);
    log_info("Initialize plugin: %s", neu_plugin_module.module_name);

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
    case NEU_REQRESP_READ_RESP: {
        neu_reqresp_read_resp_t *read_resp;
        read_resp      = (neu_reqresp_read_resp_t *) req->buf;
        char *json_str = command_read_once_response(
            plugin, plugin->parse_header, read_resp->data_val);
        MQTT_SEND(plugin->paho, "neuronlite/response", 0, json_str);
        break;
    }
    case NEU_REQRESP_WRITE_RESP: {
        neu_reqresp_write_resp_t *write_resp;
        write_resp = (neu_reqresp_write_resp_t *) req->buf;
        char *json_str =
            command_write_response(plugin, "", write_resp->data_val);
        MQTT_SEND(plugin->paho, "neuronlite/response", 0, json_str);
        break;
    }
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_data_t *neu_data;
        neu_data = (neu_reqresp_data_t *) req->buf;
        char *json_str =
            command_read_cycle_response(plugin, neu_data->data_val);
        MQTT_SEND(plugin->paho, "neuronlite/response", 0, json_str);
        break;
    }
    case NEU_REQRESP_WRITE_DATA: {
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
