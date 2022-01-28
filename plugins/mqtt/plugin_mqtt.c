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

struct context {
    neu_list_node   node;
    double          timestamp; // Armv7l support
    int             req_id;
    neu_json_mqtt_t parse_header;
    char *          result;
    bool            ready;
};

struct topic_pair {
    neu_list_node node;
    char *        topic_request;
    char *        topic_respons;
    int           qos_request;
    int           qos_response;
    int           type;
};

struct neu_plugin {
    neu_plugin_common_t common;
    neu_mqtt_option_t   option;
    pthread_t           daemon;
    pthread_mutex_t     running_mutex; // lock publish thread running state
    pthread_mutex_t     list_mutex;    // lock context state
    bool                running;
    neu_list            context_list; // publish context list
    neu_list            topic_list;   // topic list
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
                             bool ready)
{
    struct context *ctx = context_create();
    if (NULL == ctx) {
        return;
    }

    ctx->timestamp = current_time_get();
    ctx->req_id    = req_id;

    if (NULL != parse_header) {
        ctx->parse_header.function = parse_header->function;
        ctx->parse_header.uuid     = strdup(parse_header->uuid);
    }

    ctx->result = result;
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
        return strdup(temp);
    }

    return NULL;
}

static struct topic_pair *topic_pair_create()
{
    struct topic_pair *pair = NULL;
    pair                    = malloc(sizeof(struct topic_pair));
    if (NULL != pair) {
        memset(pair, 0, sizeof(struct topic_pair));
    }

    return pair;
}

static void topic_pair_destory(struct topic_pair *pair)
{
    if (NULL != pair) {
        if (NULL != pair->topic_request) {
            free(pair->topic_request);
        }

        if (NULL != pair->topic_respons) {
            free(pair->topic_respons);
        }

        free(pair);
    }
}

static void topic_list_add(neu_list *list, char *request, int qos_request,
                           char *response, int qos_response, int type)
{
    if (NULL == request) {
        return;
    }

    struct topic_pair *pair = topic_pair_create();
    if (NULL == pair) {
        return;
    }

    pair->topic_request = request;
    pair->topic_respons = response;
    pair->qos_request   = qos_request;
    pair->qos_response  = qos_response;
    pair->type          = type;

    NEU_LIST_NODE_INIT(&pair->node);
    neu_list_append(list, pair);
}

static struct topic_pair *topic_list_find(neu_list *list, const char *topic)
{
    struct topic_pair *item = NULL;
    NEU_LIST_FOREACH(list, item)
    {
        if (0 == strcmp(item->topic_request, topic)) {
            return item;
        }
    }

    return NULL;
}

static void topic_list_cleanup(neu_list *list)
{
    while (!neu_list_empty(list)) {
        struct topic_pair *pair = NULL;
        pair                    = neu_list_first(list);
        if (NULL != pair) {
            neu_list_remove(list, pair);
            topic_pair_destory(pair);
        }
    }
}

static void topic_list_generate(neu_list *list, char *name)
{
    topic_list_add(list, real_topic_generate(TOPIC_PING_REQ, name), QOS0,
                   real_topic_generate(TOPIC_STATUS_RES, name), QOS0,
                   TOPIC_TYPE_PING);

    topic_list_add(list, real_topic_generate(TOPIC_NODE_REQ, name), QOS0,
                   real_topic_generate(TOPIC_NODE_RES, name), QOS0,
                   TOPIC_TYPE_NODE);

    topic_list_add(list, real_topic_generate(TOPIC_GCONFIG_REQ, name), QOS0,
                   real_topic_generate(TOPIC_GCONFIG_RES, name), QOS0,
                   TOPIC_TYPE_GCONFIG);

    topic_list_add(list, real_topic_generate(TOPIC_TAGS_REQ, name), QOS0,
                   real_topic_generate(TOPIC_TAGS_RES, name), QOS0,
                   TOPIC_TYPE_TAGS);

    topic_list_add(list, real_topic_generate(TOPIC_PLUGIN_REQ, name), QOS0,
                   real_topic_generate(TOPIC_PLUGIN_RES, name), QOS0,
                   TOPIC_TYPE_PLUGIN);

    topic_list_add(list, real_topic_generate(TOPIC_SUBSCRIBE_REQ, name), QOS0,
                   real_topic_generate(TOPIC_SUBSCRIBE_RES, name), QOS0,
                   TOPIC_TYPE_SUBSCRIBE);

    topic_list_add(list, real_topic_generate(TOPIC_READ_REQ, name), QOS0,
                   real_topic_generate(TOPIC_READ_RES, name), QOS0,
                   TOPIC_TYPE_READ);

    topic_list_add(list, real_topic_generate(TOPIC_WRITE_REQ, name), QOS0,
                   real_topic_generate(TOPIC_WRITE_RES, name), QOS0,
                   TOPIC_TYPE_WRITE);

    topic_list_add(list, real_topic_generate(TOPIC_TTYS_REQ, name), QOS0,
                   real_topic_generate(TOPIC_TTYS_RES, name), QOS0,
                   TOPIC_TYPE_TTYS);

    topic_list_add(list, real_topic_generate(TOPIC_SCHEMA_REQ, name), QOS0,
                   real_topic_generate(TOPIC_SCHEMA_RES, name), QOS0,
                   TOPIC_TYPE_SCHEMA);

    topic_list_add(list, real_topic_generate(TOPIC_SETTING_REQ, name), QOS0,
                   real_topic_generate(TOPIC_SETTING_RES, name), QOS0,
                   TOPIC_TYPE_SETTING);

    topic_list_add(list, real_topic_generate(TOPIC_CTR_REQ, name), QOS0,
                   real_topic_generate(TOPIC_CTR_RES, name), QOS0,
                   TOPIC_TYPE_CTR);

    topic_list_add(list, real_topic_generate(TOPIC_STATE_REQ, name), QOS0,
                   real_topic_generate(TOPIC_STATE_RES, name), QOS0,
                   TOPIC_TYPE_STATE);
}

static void mqtt_context_add(neu_plugin_t *plugin, uint32_t req_id,
                             neu_json_mqtt_t *parse_header, char *result,
                             bool ready)
{
    pthread_mutex_lock(&plugin->list_mutex);
    context_list_add(plugin, req_id, parse_header, result, ready);
    pthread_mutex_unlock(&plugin->list_mutex);
}

static void mqtt_response_handle1(const char *topic_name, size_t topic_len,
                                  void *payload, const size_t len,
                                  void *context)
{
    neu_plugin_t *     plugin = (neu_plugin_t *) context;
    struct topic_pair *pair = topic_list_find(&plugin->topic_list, topic_name);

    mqtt_response_t response = { .topic_name  = topic_name,
                                 .topic_len   = topic_len,
                                 .payload     = payload,
                                 .len         = len,
                                 .plugin      = context,
                                 .topic_pair  = pair,
                                 .context_add = mqtt_context_add };
    command_response_handle(&response);
    return;
}

static void topic_list_subscribe(neu_list *list, neu_mqtt_client_t *client)
{
    struct topic_pair *item = NULL;
    NEU_LIST_FOREACH(list, item)
    {
        if (NULL != item) {
            neu_mqtt_client_subscribe(client, item->topic_request,
                                      item->qos_request, mqtt_response_handle1);
        }
    }
}

static void mqtt_context_publish(neu_plugin_t *plugin)
{
    struct context *ctx    = NULL;
    char *          result = NULL;
    pthread_mutex_lock(&plugin->list_mutex);

    ctx = neu_list_first(&plugin->context_list);
    if (NULL != ctx) {
        if (ctx->ready) {
            neu_list_remove(&plugin->context_list, ctx);
            if (NULL != ctx->result) {
                result = strdup(ctx->result);
            }

            context_destroy(ctx);
        }
    }

    pthread_mutex_unlock(&plugin->list_mutex);

    if (NULL != result) {
        neu_err_code_e error = neu_mqtt_client_publish(
            plugin->client, plugin->option.respons_topic, 0,
            (unsigned char *) result, strlen(result));
        log_debug("Publish error code:%d, json:%s", error, result);
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

static void mqtt_response_handle(const char *topic_name, size_t topic_len,
                                 void *payload, const size_t len, void *context)
{
    mqtt_response_t response = { .topic_name  = topic_name,
                                 .topic_len   = topic_len,
                                 .payload     = payload,
                                 .len         = len,
                                 .plugin      = context,
                                 .topic_pair  = NULL,
                                 .context_add = mqtt_context_add };
    command_response_handle(&response);
    return;
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
    // Context list init
    NEU_LIST_INIT(&plugin->context_list, struct context, node);
    NEU_LIST_INIT(&plugin->topic_list, struct topic_pair, node);

    // Publish thread init
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

static int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    // Close MQTT client
    neu_mqtt_client_close(plugin->client);
    plugin->client = NULL;
    topic_list_cleanup(&plugin->topic_list);
    mqtt_option_uninit(&plugin->option);

    // Quit publish thread
    pthread_mutex_lock(&plugin->running_mutex);
    plugin->running = false;
    pthread_mutex_unlock(&plugin->running_mutex);
    pthread_join(plugin->daemon, NULL);
    pthread_mutex_destroy(&plugin->list_mutex);
    pthread_mutex_destroy(&plugin->running_mutex);

    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    if (NULL == configs || NULL == configs->buf) {
        return -1;
    }

    // Try close MQTT client
    neu_mqtt_client_close(plugin->client);
    plugin->client = NULL;

    mqtt_option_uninit(&plugin->option);

    // Use new config set MQTT option instance
    int rc = mqtt_option_init(configs, &plugin->option);
    if (0 != rc) {
        log_error("MQTT option init fail:%d, initialize plugin failed: %s", rc,
                  neu_plugin_module.module_name);
        mqtt_option_uninit(&plugin->option);
        return -1;
    }

    log_info("Config plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_start(neu_plugin_t *plugin)
{
    // MQTT client setup
    neu_err_code_e error = neu_mqtt_client_open(
        (neu_mqtt_client_t *) &plugin->client, &plugin->option, plugin);
    error = neu_mqtt_client_subscribe(plugin->client, plugin->option.topic, 0,
                                      mqtt_response_handle);

    if (NEU_ERR_MQTT_IS_NULL == error) {
        log_error("Can not create mqtt client instance, initialize plugin "
                  "failed: %s",
                  neu_plugin_module.module_name);
        return -1;
    }

    // Subscribe all topics
    neu_list *topic_list = &plugin->topic_list;
    topic_list_cleanup(topic_list); // Try to delete a previous subscription
    topic_list_generate(topic_list, plugin->option.clientid);
    topic_list_subscribe(topic_list, plugin->client);
    return 0;
}

static int mqtt_plugin_stop(neu_plugin_t *plugin)
{
    // Close MQTT client
    neu_mqtt_client_close(plugin->client);
    plugin->client = NULL;
    topic_list_cleanup(&plugin->topic_list);
    return 0;
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
        neu_reqresp_data_t *neu_data = NULL;
        neu_data                     = (neu_reqresp_data_t *) req->buf;

        char *json_str =
            command_read_cycle_response(plugin, neu_data->data_val);
        mqtt_context_add(plugin, 0, NULL, json_str, true);
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
    .module_name  = "neuron-mqtt-plugin",
    .module_descr = "Neuron northbound MQTT communication plugin",
    .intf_funs    = &plugin_intf_funs,
    .kind         = PLUGIN_KIND_SYSTEM,
    .type         = NEU_NODE_TYPE_MQTT
};
