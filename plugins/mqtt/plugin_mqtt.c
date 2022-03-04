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
#include <sys/time.h>

#include "command/command.h"
#include "connection/mqtt_client_intf.h"
#include "mqtt_util.h"
#include <neuron.h>

#define UNUSED(x) (void) (x)
#define INTERVAL 100000U
#define TIMEOUT 5000000U

#define TOPIC_PING_REQ "neuron/%s/ping"
#define TOPIC_NODE_REQ "neuron/%s/node/req"
#define TOPIC_GCONFIG_REQ "neuron/%s/gconfig/req"
#define TOPIC_TAGS_REQ "neuron/%s/tags/req"
#define TOPIC_PLUGIN_REQ "neuron/%s/plugin/req"
#define TOPIC_SUBSCRIBE_REQ "neuron/%s/subscribe/req"
#define TOPIC_READ_REQ "neuron/%s/read/req"
#define TOPIC_WRITE_REQ "neuron/%s/write/req"
#define TOPIC_TTYS_REQ "neuron/%s/ttys/req"
#define TOPIC_SCHEMA_REQ "neuron/%s/schema/plugin/req"
#define TOPIC_SETTING_REQ "neuron/%s/node/setting/req"
#define TOPIC_CTR_REQ "neuron/%s/node/ctl/req"
#define TOPIC_STATE_REQ "neuron/%s/node/state/req"

#define TOPIC_STATUS_RES "neuron/%s/status"
#define TOPIC_NODE_RES "neuron/%s/node/resp"
#define TOPIC_GCONFIG_RES "neuron/%s/gconfig/resp"
#define TOPIC_TAGS_RES "neuron/%s/tags/resp"
#define TOPIC_PLUGIN_RES "neuron/%s/plugin/resp"
#define TOPIC_SUBSCRIBE_RES "neuron/%s/subscribe/resp"
#define TOPIC_READ_RES "neuron/%s/read/resp"
#define TOPIC_WRITE_RES "neuron/%s/write/resp"
#define TOPIC_TTYS_RES "neuron/%s/ttys/resp"
#define TOPIC_SCHEMA_RES "neuron/%s/schema/plugin/resp"
#define TOPIC_SETTING_RES "neuron/%s/node/setting/resp"
#define TOPIC_CTR_RES "neuron/%s/node/ctl/resp"
#define TOPIC_STATE_RES "neuron/%s/node/state/resp"

#define QOS0 0
#define QOS1 1
#define QOS2 2

const neu_plugin_module_t neu_plugin_module;

struct topic_pair {
    neu_list_node node;
    char *        topic_request;
    char *        topic_response;
    int           qos_request;
    int           qos_response;
    int           type;
};

struct context {
    neu_list_node      node;
    double             timestamp; // Armv7l support
    int                req_id;
    neu_json_mqtt_t    parse_header;
    char *             result;
    struct topic_pair *pair;
    bool               ready;
};

struct neu_plugin {
    neu_plugin_common_t common;
    neu_mqtt_option_t   option;
    pthread_t           daemon;
    pthread_mutex_t     running_mutex; // lock publish thread running state
    pthread_mutex_t     list_mutex;    // lock context state
    bool                running;
    neu_list            context_list; // publish context list
    neu_vector_t        topics;       // topic list
    neu_mqtt_client_t   client;
};

static struct context *context_create()
{
    struct context *ctx = NULL;
    ctx                 = malloc(sizeof(struct context));
    if (NULL != ctx) {
        memset(ctx, 0, sizeof(struct context));
    }

    return ctx;
}

static void context_destroy(struct context *ctx)
{
    if (NULL != ctx) {
        if (NULL != ctx->parse_header.uuid) {
            free(ctx->parse_header.uuid);
        }

        if (NULL != ctx->parse_header.command) {
            free(ctx->parse_header.command);
        }

        if (NULL != ctx->result) {
            free(ctx->result);
        }

        free(ctx);
    }
}

static double current_time_get()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double ms = tv.tv_sec; // Armv7l support
    return ms * 1000 + tv.tv_usec / 1000;
}

static int context_timeout(struct context *ctx)
{
    double time = current_time_get();
    if (TIMEOUT < (time - ctx->timestamp)) {
        return 0;
    }

    return -1;
}

static void context_list_add(neu_plugin_t *plugin, int req_id,
                             neu_json_mqtt_t *parse_header, char *result,
                             void *pair, bool ready)
{
    struct context *ctx = context_create();
    if (NULL == ctx) {
        return;
    }

    ctx->timestamp = current_time_get();
    ctx->req_id    = req_id;

    if (NULL != parse_header) {
        ctx->parse_header.uuid    = strdup(parse_header->uuid);
        ctx->parse_header.command = strdup(parse_header->command);
    }

    ctx->result = result;
    ctx->pair   = pair;
    ctx->ready  = ready;

    NEU_LIST_NODE_INIT(&ctx->node);
    neu_list_append(&plugin->context_list, ctx);
}

static struct context *context_list_find(neu_plugin_t *plugin, const int id)
{
    struct context *item = NULL;
    NEU_LIST_FOREACH(&plugin->context_list, item)
    {
        if (id == item->req_id) {
            return item;
        }
    }

    return NULL;
}

static char *real_topic_generate(char *format, char *name)
{
    char temp[256] = { '\0' };
    int  rc        = snprintf(temp, 256, format, name);
    if (-1 != rc) {
        char *topic = strdup(temp);
        return topic;
    }

    return NULL;
}

static void topics_add(vector_t *topics, char *request, int qos_request,
                       char *response, int qos_response, int type)
{
    if (NULL == request) {
        return;
    }

    struct topic_pair pair = { 0 };
    pair.topic_request     = request;
    pair.topic_response    = response;
    pair.qos_request       = qos_request;
    pair.qos_response      = qos_response;
    pair.type              = type;
    vector_push_back(topics, &pair);
}

static void topics_cleanup(vector_t *topics)
{
    VECTOR_FOR_EACH(topics, item)
    {
        struct topic_pair *pair = NULL;
        pair                    = (struct topic_pair *) iterator_get(&item);
        if (NULL != pair) {
            if (NULL != pair->topic_request) {
                free(pair->topic_request);
            }

            if (NULL != pair->topic_response) {
                free(pair->topic_response);
            }
        }
    }

    vector_clear(topics);
}

static void topics_generate(vector_t *topics, char *name)
{
    topics_add(topics, real_topic_generate(TOPIC_PING_REQ, name), QOS0,
               real_topic_generate(TOPIC_STATUS_RES, name), QOS0,
               TOPIC_TYPE_PING);

    topics_add(topics, real_topic_generate(TOPIC_NODE_REQ, name), QOS0,
               real_topic_generate(TOPIC_NODE_RES, name), QOS0,
               TOPIC_TYPE_NODE);

    topics_add(topics, real_topic_generate(TOPIC_GCONFIG_REQ, name), QOS0,
               real_topic_generate(TOPIC_GCONFIG_RES, name), QOS0,
               TOPIC_TYPE_GCONFIG);

    topics_add(topics, real_topic_generate(TOPIC_TAGS_REQ, name), QOS0,
               real_topic_generate(TOPIC_TAGS_RES, name), QOS0,
               TOPIC_TYPE_TAGS);

    topics_add(topics, real_topic_generate(TOPIC_PLUGIN_REQ, name), QOS0,
               real_topic_generate(TOPIC_PLUGIN_RES, name), QOS0,
               TOPIC_TYPE_PLUGIN);

    topics_add(topics, real_topic_generate(TOPIC_SUBSCRIBE_REQ, name), QOS0,
               real_topic_generate(TOPIC_SUBSCRIBE_RES, name), QOS0,
               TOPIC_TYPE_SUBSCRIBE);

    topics_add(topics, real_topic_generate(TOPIC_READ_REQ, name), QOS0,
               real_topic_generate(TOPIC_READ_RES, name), QOS0,
               TOPIC_TYPE_READ);

    topics_add(topics, real_topic_generate(TOPIC_WRITE_REQ, name), QOS0,
               real_topic_generate(TOPIC_WRITE_RES, name), QOS0,
               TOPIC_TYPE_WRITE);

    topics_add(topics, real_topic_generate(TOPIC_TTYS_REQ, name), QOS0,
               real_topic_generate(TOPIC_TTYS_RES, name), QOS0,
               TOPIC_TYPE_TTYS);

    topics_add(topics, real_topic_generate(TOPIC_SCHEMA_REQ, name), QOS0,
               real_topic_generate(TOPIC_SCHEMA_RES, name), QOS0,
               TOPIC_TYPE_SCHEMA);

    topics_add(topics, real_topic_generate(TOPIC_SETTING_REQ, name), QOS0,
               real_topic_generate(TOPIC_SETTING_RES, name), QOS0,
               TOPIC_TYPE_SETTING);

    topics_add(topics, real_topic_generate(TOPIC_CTR_REQ, name), QOS0,
               real_topic_generate(TOPIC_CTR_RES, name), QOS0, TOPIC_TYPE_CTR);

    topics_add(topics, real_topic_generate(TOPIC_STATE_REQ, name), QOS0,
               real_topic_generate(TOPIC_STATE_RES, name), QOS0,
               TOPIC_TYPE_STATE);
}

static void mqtt_context_add(neu_plugin_t *plugin, uint32_t req_id,
                             neu_json_mqtt_t *parse_header, char *result,
                             void *pair, bool ready)
{
    pthread_mutex_lock(&plugin->list_mutex);
    context_list_add(plugin, req_id, parse_header, result, pair, ready);
    pthread_mutex_unlock(&plugin->list_mutex);
}

static bool topic_match(const void *key, const void *item)
{
    char *             topic_name = (char *) key;
    struct topic_pair *pair       = (struct topic_pair *) item;
    if (0 == strcmp(pair->topic_request, topic_name)) {
        return true;
    }

    return false;
}

static bool topic_type_match(const void *key, const void *item)
{
    int                type = *(int *) key;
    struct topic_pair *pair = (struct topic_pair *) item;
    if (type == pair->type) {
        return true;
    }

    return false;
}

static void mqtt_response_handle(const char *topic_name, size_t topic_len,
                                 void *payload, const size_t len, void *context)
{
    neu_plugin_t *     plugin = (neu_plugin_t *) context;
    struct topic_pair *pair =
        vector_find_item(&plugin->topics, (void *) topic_name, topic_match);
    mqtt_response_t response = { .topic_name  = topic_name,
                                 .topic_len   = topic_len,
                                 .payload     = payload,
                                 .len         = len,
                                 .plugin      = context,
                                 .topic_pair  = pair,
                                 .type        = pair->type,
                                 .context_add = mqtt_context_add };
    command_response_handle(&response);
}

static void topics_subscribe(vector_t *topics, neu_mqtt_client_t *client)
{
    VECTOR_FOR_EACH(topics, item)
    {
        struct topic_pair *pair = NULL;
        pair                    = iterator_get(&item);
        if (NULL != pair) {
            neu_mqtt_client_subscribe(client, pair->topic_request,
                                      pair->qos_request, mqtt_response_handle);
        }
    }
}

static void mqtt_context_publish(neu_plugin_t *plugin)
{
    struct context *ctx    = NULL;
    char *          topic  = NULL;
    char *          result = NULL;
    pthread_mutex_lock(&plugin->list_mutex);

    ctx = neu_list_first(&plugin->context_list);
    if (NULL != ctx) {
        if (ctx->ready) {
            neu_list_remove(&plugin->context_list, ctx);

            if (NULL != ctx->result) {
                result = strdup(ctx->result);
            }

            if (NULL != ctx->pair) {
                topic = ctx->pair->topic_response;
            }

            context_destroy(ctx);
        }
    }

    pthread_mutex_unlock(&plugin->list_mutex);

    if (NULL != result && NULL != topic) {
        neu_err_code_e error = neu_mqtt_client_publish(
            plugin->client, topic, 0, (unsigned char *) result, strlen(result));
        log_debug("Publish error code:%d, topic('%s'): %s", error, topic,
                  result);
        free(result);
    }
}

static void mqtt_context_timeout_remove(neu_plugin_t *plugin)
{
    struct context *ctx = NULL;
    pthread_mutex_lock(&plugin->list_mutex);

    ctx = neu_list_first(&plugin->context_list);
    if (NULL != ctx) {
        if (!ctx->ready && (0 == context_timeout(ctx))) {
            neu_list_remove(&plugin->context_list, ctx);
            context_destroy(ctx);
        }
    }

    pthread_mutex_unlock(&plugin->list_mutex);
}

static void mqtt_context_cleanup(neu_plugin_t *plugin)
{
    pthread_mutex_lock(&plugin->list_mutex);

    while (!neu_list_empty(&plugin->context_list)) {
        struct context *ctx = neu_list_first(&plugin->context_list);
        neu_list_remove(&plugin->context_list, ctx);
        context_destroy(ctx);
    }

    pthread_mutex_unlock(&plugin->list_mutex);
}

static void *mqtt_send_loop(void *argument)
{
    neu_plugin_t *plugin = (neu_plugin_t *) argument;

    bool run_flag = true;
    while (1) {
        // Get run state
        pthread_mutex_lock(&plugin->running_mutex);
        run_flag = plugin->running;
        pthread_mutex_unlock(&plugin->running_mutex);
        if (!run_flag) {
            break;
        }

        // Get context and publish
        mqtt_context_publish(plugin);

        // Remove context of timeout
        mqtt_context_timeout_remove(plugin);

        // If list empty -> delay(INTERVAL)
        int rc = 0;
        pthread_mutex_lock(&plugin->list_mutex);
        rc = neu_list_empty(&plugin->context_list);
        pthread_mutex_unlock(&plugin->list_mutex);
        if (rc) {
            usleep(INTERVAL);
        }

        // Update link state
        if (NEU_ERR_SUCCESS != neu_mqtt_client_is_connected(plugin->client)) {
            plugin->common.link_state = NEU_PLUGIN_LINK_STATE_DISCONNECTED;
        } else {
            plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTED;
        }
    }

    // Cleanup on quit
    mqtt_context_cleanup(plugin);
    return NULL;
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
    NEU_LIST_INIT(&plugin->context_list, struct context, node);
    vector_init(&plugin->topics, 1, sizeof(struct topic_pair));

    plugin->running = true;
    pthread_mutex_init(&plugin->running_mutex, NULL);
    pthread_mutex_init(&plugin->list_mutex, NULL);

    if (0 != pthread_create(&plugin->daemon, NULL, mqtt_send_loop, plugin)) {
        log_error("Failed to start thread daemon.");
        return -1;
    }

    log_info("Initialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_stop(neu_plugin_t *plugin);

static int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    mqtt_plugin_stop(plugin);
    mqtt_option_uninit(&plugin->option);

    pthread_mutex_lock(&plugin->running_mutex);
    plugin->running = false;
    pthread_mutex_unlock(&plugin->running_mutex);
    pthread_join(plugin->daemon, NULL);
    pthread_mutex_destroy(&plugin->list_mutex);
    pthread_mutex_destroy(&plugin->running_mutex);

    topics_cleanup(&plugin->topics);
    vector_uninit(&plugin->topics);

    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    mqtt_plugin_stop(plugin);

    mqtt_option_uninit(&plugin->option);
    int rc = mqtt_option_init(configs, &plugin->option);
    if (0 != rc) {
        const char *name = neu_plugin_module.module_name;
        log_error("MQTT option init fail:%d, %s", rc, name);
        mqtt_option_uninit(&plugin->option);
        return NEU_ERR_FAILURE;
    }

    topics_cleanup(&plugin->topics);
    topics_generate(&plugin->topics, plugin->option.clientid);

    log_info("Config plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_start(neu_plugin_t *plugin)
{
    neu_err_code_e error = neu_mqtt_client_open(
        (neu_mqtt_client_t *) &plugin->client, &plugin->option, plugin);
    if (NEU_ERR_MQTT_IS_NULL == error) {
        const char *name = neu_plugin_module.module_name;
        log_error("Start plugin failed: %s", name);
        return NEU_ERR_PLUGIN_DISCONNECTED;
    }

    topics_subscribe(&plugin->topics, plugin->client);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_stop(neu_plugin_t *plugin)
{
    neu_mqtt_client_close(plugin->client);
    plugin->client = NULL;
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    int rv = 0;

    if (plugin == NULL || req == NULL) {
        log_warn("The plugin pointer or request is NULL");
        return (-1);
    }

    log_info_node(plugin, "send request to plugin: %s, type:%d",
                  neu_plugin_module.module_name, req->req_type);

    switch (req->req_type) {
    case NEU_REQRESP_READ_RESP: {
        neu_reqresp_read_resp_t *read_resp = NULL;
        read_resp = (neu_reqresp_read_resp_t *) req->buf;

        pthread_mutex_lock(&plugin->list_mutex);
        struct context *ctx = NULL;
        ctx                 = context_list_find(plugin, req->req_id);
        if (NULL != ctx) {
            char *json_str = command_read_once_response(
                plugin, &ctx->parse_header, read_resp->data_val);
            ctx->result = json_str;
            ctx->ready  = true;
        }
        pthread_mutex_unlock(&plugin->list_mutex);
        break;
    }
    case NEU_REQRESP_WRITE_RESP: {
        neu_reqresp_write_resp_t *write_resp = NULL;
        write_resp = (neu_reqresp_write_resp_t *) req->buf;

        pthread_mutex_lock(&plugin->list_mutex);
        struct context *ctx = NULL;
        ctx                 = context_list_find(plugin, req->req_id);
        if (NULL != ctx) {
            char *json_str = command_write_response(plugin, &ctx->parse_header,
                                                    write_resp->data_val);
            ctx->result    = json_str;
            ctx->ready     = true;
        }
        pthread_mutex_unlock(&plugin->list_mutex);
        break;
    }
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_data_t *neu_data   = NULL;
        neu_data                       = (neu_reqresp_data_t *) req->buf;
        struct topic_pair *  pair      = NULL;
        char *               json_str  = NULL;
        int                  type      = TOPIC_TYPE_READ;
        uint64_t             sender    = req->sender_id;
        const char *         node_name = req->node_name;
        neu_taggrp_config_t *config    = neu_data->grp_config;
        pair =
            vector_find_item(&plugin->topics, (void *) &type, topic_type_match);
        json_str = command_read_periodic_response(plugin, sender, node_name,
                                                  config, neu_data->data_val);
        mqtt_context_add(plugin, 0, NULL, json_str, pair, true);
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

static int mqtt_plugin_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    (void) plugin;
    (void) tag;

    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open         = mqtt_plugin_open,
    .close        = mqtt_plugin_close,
    .init         = mqtt_plugin_init,
    .uninit       = mqtt_plugin_uninit,
    .start        = mqtt_plugin_start,
    .stop         = mqtt_plugin_stop,
    .config       = mqtt_plugin_config,
    .request      = mqtt_plugin_request,
    .validate_tag = mqtt_plugin_validate_tag,
    .event_reply  = mqtt_plugin_event_reply
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "mqtt-plugin",
    .module_descr = "Neuron northbound MQTT communication plugin",
    .intf_funs    = &plugin_intf_funs,
    .kind         = PLUGIN_KIND_SYSTEM,
    .type         = NEU_NODE_TYPE_MQTT
};
