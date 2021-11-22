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

#include "command/command.h"
#include "mqttc_client.h"
#include "mqttc_util.h"
#include <neuron.h>

#define UNUSED(x) (void) (x)

#define MQTT_SEND(client, topic, qos, json_str)                           \
    {                                                                     \
        if (NULL != json_str) {                                           \
            client_error_e error = mqttc_client_publish(                  \
                client, topic, qos, (unsigned char *) json_str,           \
                strlen(json_str));                                        \
            log_debug("Publish error code:%d, json:%s", error, json_str); \
            free(json_str);                                               \
        }                                                                 \
    }

const neu_plugin_module_t neu_plugin_module;

struct context {
    neu_list_node   node;
    int             req_id;
    neu_json_mqtt_t parse_header;
};

struct neu_plugin {
    neu_plugin_common_t common;
    option_t            option;
    neu_list            context_list;
    int                 mqtt_client_type;
    void *              mqtt_client;
};

static void context_list_init(neu_list *list)
{
    NEU_LIST_INIT(list, struct context, node);
}

static struct context *context_create()
{
    struct context *ctx = NULL;
    ctx                 = malloc(sizeof(struct context));
    if (NULL != ctx) {
        memset(ctx, 0, sizeof(struct context));
    }

    return ctx;
}

static void context_list_add(neu_list *list, int req_id,
                             neu_json_mqtt_t *parse_header)
{
    struct context *ctx = context_create();
    if (NULL == ctx) {
        return;
    }

    ctx->req_id                = req_id;
    ctx->parse_header.function = parse_header->function;
    ctx->parse_header.uuid     = strdup(parse_header->uuid);

    NEU_LIST_NODE_INIT(&ctx->node);
    neu_list_append(list, ctx);
}

static struct context *context_list_find(neu_list *list, const int id)
{
    struct context *item = NULL;
    NEU_LIST_FOREACH(list, item)
    {
        if (id == item->req_id) {
            return item;
        }
    }

    return NULL;
}

static void context_destroy(struct context *ctx)
{
    if (NULL != ctx) {
        if (NULL != ctx->parse_header.uuid) {
            free(ctx->parse_header.uuid);
        }
        free(ctx);
    }
}

static void context_list_remove(neu_list *list, struct context *ctx)
{
    neu_list_remove(list, ctx);
    context_destroy(ctx);
}

static void context_list_destroy(neu_list *list)
{
    while (!neu_list_empty(list)) {
        struct context *ctx = neu_list_first(list);
        neu_list_remove(list, ctx);
        context_destroy(ctx);
    }
}

static void mqtt_send(neu_plugin_t *plugin, char *json_str)
{
    MQTT_SEND(plugin->mqtt_client, "neuronlite/response", 0, json_str);
}

static void context_add(neu_plugin_t *plugin, uint32_t req_id,
                        neu_json_mqtt_t *parse_header)
{
    context_list_add(&plugin->context_list, req_id, parse_header);
}

static void plugin_response_handle(const char *topic_name, size_t topic_len,
                                   void *payload, const size_t len,
                                   void *context)
{
    mqtt_response_t response = { .topic_name  = topic_name,
                                 .topic_len   = topic_len,
                                 .payload     = payload,
                                 .len         = len,
                                 .context     = context,
                                 .mqtt_send   = mqtt_send,
                                 .context_add = context_add };
    command_response_handle(&response);
    return;
}

static neu_plugin_t *mqtt_plugin_open(neu_adapter_t *            adapter,
                                      const adapter_callbacks_t *callbacks)
{
    if (NULL == adapter || NULL == callbacks) {
        log_error("Open plugin with NULL adapter or callbacks");
        return NULL;
    }

    neu_plugin_t *plugin = NULL;
    plugin               = (neu_plugin_t *) malloc(sizeof(neu_plugin_t));
    if (NULL == plugin) {
        log_error("Failed to allocate plugin %s",
                  neu_plugin_module.module_name);
        return NULL;
    }
    memset(plugin, 0, sizeof(neu_plugin_t));

    neu_plugin_common_init(&plugin->common);
    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    plugin->common.link_state        = NEU_PLUGIN_LINK_STATE_CONNECTING;

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
    // Context list init
    context_list_init(&plugin->context_list);

    int rc = mqtt_option_init(&plugin->option);
    if (0 != rc) {
        log_error("MQTT option init fail:%d, initialize plugin failed: %s", rc,
                  neu_plugin_module.module_name);
        return -1;
    }

    // MQTT-C client setup
    client_error_e error = mqttc_client_open(
        &plugin->option, plugin, (mqttc_client_t **) &plugin->mqtt_client);
    error = mqttc_client_subscribe(plugin->mqtt_client, "neuronlite/request", 0,
                                   plugin_response_handle);
    if (MQTTC_IS_NULL == error) {
        log_error(
            "Can not create mqtt client instance, initialize plugin failed: %s",
            neu_plugin_module.module_name);
        return -1;
    }

    log_info("Initialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    mqttc_client_close(plugin->mqtt_client);
    mqtt_option_uninit(&plugin->option);
    context_list_destroy(&plugin->context_list);

    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    if (NULL == configs || NULL == configs->buf) {
        return -1;
    }

    int rc = mqtt_option_init_by_config(configs, &plugin->option);
    if (0 != rc) {
        log_error("MQTT option init fail:%d, initialize plugin failed: %s", rc,
                  neu_plugin_module.module_name);

        mqtt_option_uninit(&plugin->option);
        return -1;
    }

    log_info("Config plugin: %s", neu_plugin_module.module_name);
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
        neu_reqresp_read_resp_t *read_resp = NULL;
        read_resp = (neu_reqresp_read_resp_t *) req->buf;

        struct context *ctx = NULL;
        ctx = context_list_find(&plugin->context_list, req->req_id);
        if (NULL != ctx) {
            char *json_str = command_read_once_response(
                plugin, &ctx->parse_header, read_resp->data_val);
            MQTT_SEND(plugin->mqtt_client, "neuronlite/response", 0, json_str);
            context_list_remove(&plugin->context_list, ctx);
        }
        break;
    }
    case NEU_REQRESP_WRITE_RESP: {
        neu_reqresp_write_resp_t *write_resp = NULL;
        write_resp = (neu_reqresp_write_resp_t *) req->buf;

        struct context *ctx = NULL;
        ctx = context_list_find(&plugin->context_list, req->req_id);
        if (NULL != ctx) {
            char *json_str = command_write_response(plugin, &ctx->parse_header,
                                                    write_resp->data_val);
            MQTT_SEND(plugin->mqtt_client, "neuronlite/response", 0, json_str);
            context_list_remove(&plugin->context_list, ctx);
        }
        break;
    }
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_data_t *neu_data = NULL;
        neu_data                     = (neu_reqresp_data_t *) req->buf;
        char *json_str =
            command_read_cycle_response(plugin, neu_data->data_val);
        MQTT_SEND(plugin->mqtt_client, "neuronlite/response", 0, json_str);
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
