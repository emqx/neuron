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
#include <sys/queue.h>
#include <sys/time.h>

#include "command/command.h"
#include "connection/mqtt_client_intf.h"
#include "mqtt.h"
#include "util.h"

const neu_plugin_module_t neu_plugin_module;

struct topic_pair {
    char *topic_request;
    char *topic_response;
    int   qos_request;
    int   qos_response;
    int   type;
};

struct context {
    double             timestamp;
    int                req_id;
    neu_json_mqtt_t    json_mqtt;
    char *             result;
    struct topic_pair *pair;
    bool               ready;
    TAILQ_ENTRY(context) entry;
};

typedef struct mqtt_routine {
    neu_plugin_t *    plugin;
    bool              running;
    pthread_mutex_t   contexts_mtx;
    pthread_mutex_t   running_mtx;
    neu_mqtt_option_t option;
    neu_mqtt_client_t client;
    pthread_t         daemon;
    neu_vector_t      topics;
    TAILQ_HEAD(, context) head;
} mqtt_routine_t;

struct neu_plugin {
    neu_plugin_common_t common;
    bool                routine_running;
    mqtt_routine_t *    routine;
    pthread_mutex_t     mutex;
};

static double current_time_get()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double ms = tv.tv_sec; // Armv7l support
    return ms * 1000 + tv.tv_usec / 1000;
}

static int timeout(struct context *ctx)
{
    double time = current_time_get();
    if (TIMEOUT < (time - ctx->timestamp)) {
        return 0;
    }

    return -1;
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

static void topics_generate(vector_t *topics, char *name, char *upload_topic)
{
    char *ping_req = real_topic_generate(TOPIC_PING_REQ, name);
    char *ping_res = real_topic_generate(TOPIC_STATUS_RES, name);
    topics_add(topics, ping_req, QOS0, ping_res, QOS0, TOPIC_TYPE_PING);

    char *read_req = real_topic_generate(TOPIC_READ_REQ, name);
    char *read_res = real_topic_generate(TOPIC_READ_RES, name);
    topics_add(topics, read_req, QOS0, read_res, QOS0, TOPIC_TYPE_READ);

    char *write_req = real_topic_generate(TOPIC_WRITE_REQ, name);
    char *write_res = real_topic_generate(TOPIC_WRITE_RES, name);
    topics_add(topics, write_req, QOS0, write_res, QOS0, TOPIC_TYPE_WRITE);

    /// UPLOAD TOPIC SETTING
    char *upload_req = NULL;
    char *upload_res = NULL;
    if (NULL != upload_topic && 0 < strlen(upload_topic)) {
        upload_res = strdup(upload_topic);
    } else {
        upload_res = real_topic_generate(TOPIC_UPLOAD_RES, name);
    }

    topics_add(topics, upload_req, QOS0, upload_res, QOS0, TOPIC_TYPE_UPLOAD);
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

static int mqtt_routine_send(mqtt_routine_t *routine, struct context *context);

static struct context *mqtt_routine_context_create(int              req_id,
                                                   neu_json_mqtt_t *json_mqtt,
                                                   char *result, void *pair,
                                                   bool ready);

static void mqtt_routine_callback(neu_plugin_t *plugin, uint32_t req_id,
                                  neu_json_mqtt_t *json_mqtt, char *result,
                                  void *pair, bool ready)
{
    pthread_mutex_lock(&plugin->mutex);

    if (!plugin->routine_running) {
        pthread_mutex_unlock(&plugin->mutex);
        return;
    }

    struct context *entry =
        mqtt_routine_context_create(req_id, json_mqtt, result, pair, ready);
    mqtt_routine_send(plugin->routine, entry);

    pthread_mutex_unlock(&plugin->mutex);
}

static void mqtt_routine_response(const char *topic_name, size_t topic_len,
                                  void *payload, const size_t len,
                                  void *context)
{
    neu_plugin_t *     plugin  = (neu_plugin_t *) context;
    mqtt_routine_t *   routine = plugin->routine;
    struct topic_pair *pair =
        vector_find_item(&routine->topics, (void *) topic_name, topic_match);
    mqtt_response_t response = { .topic_name  = topic_name,
                                 .topic_len   = topic_len,
                                 .payload     = payload,
                                 .len         = len,
                                 .plugin      = context,
                                 .topic_pair  = pair,
                                 .type        = pair->type,
                                 .context_add = mqtt_routine_callback };
    command_response_handle(&response);
}

static void topics_subscribe(vector_t *topics, neu_mqtt_client_t *client)
{
    VECTOR_FOR_EACH(topics, item)
    {
        struct topic_pair *pair = NULL;
        pair                    = iterator_get(&item);
        if (NULL != pair && NULL != pair->topic_request) {
            neu_mqtt_client_subscribe(client, pair->topic_request,
                                      pair->qos_request, mqtt_routine_response);
        }
    }
}

static struct context *mqtt_routine_context_create(int              req_id,
                                                   neu_json_mqtt_t *json_mqtt,
                                                   char *result, void *pair,
                                                   bool ready)
{
    struct context *entry = calloc(1, sizeof(struct context));
    if (NULL == entry) {
        return NULL;
    }

    entry->timestamp = current_time_get();
    entry->req_id    = req_id;

    if (NULL != json_mqtt) {
        entry->json_mqtt.uuid    = strdup(json_mqtt->uuid);
        entry->json_mqtt.command = strdup(json_mqtt->command);
    }

    entry->result = result;
    entry->pair   = pair;
    entry->ready  = ready;

    return entry;
}

static struct context *mqtt_routine_context_find(mqtt_routine_t *routine,
                                                 const int       id)
{
    assert(NULL != routine);

    struct context *item = NULL;
    TAILQ_FOREACH(item, &routine->head, entry)
    {
        if (id == item->req_id) {
            return item;
        }
    }

    return NULL;
}

static void mqtt_routine_context_destroy(struct context *entry)
{
    if (NULL != entry) {
        if (NULL != entry->json_mqtt.uuid) {
            free(entry->json_mqtt.uuid);
        }

        if (NULL != entry->json_mqtt.command) {
            free(entry->json_mqtt.command);
        }

        if (NULL != entry->result) {
            free(entry->result);
        }

        free(entry);
    }
}

static void mqtt_routine_publish(mqtt_routine_t *routine)
{
    char *topic  = NULL;
    char *result = NULL;

    pthread_mutex_lock(&routine->contexts_mtx);
    struct context *item = NULL;
    TAILQ_FOREACH(item, &routine->head, entry)
    {
        if (item->ready) {
            break;
        }
    }

    if (NULL != item) {
        if (NULL != item->result) {
            result = strdup(item->result);
        }

        if (NULL != item->pair) {
            topic = item->pair->topic_response;
        }

        TAILQ_REMOVE(&routine->head, item, entry);
        mqtt_routine_context_destroy(item);
    }

    pthread_mutex_unlock(&routine->contexts_mtx);

    if (NULL != result && NULL != topic) {
        if (NULL == routine->client) {
            free(result);
            return;
        }

        neu_err_code_e error =
            neu_mqtt_client_publish(routine->client, topic, 0,
                                    (unsigned char *) result, strlen(result));
        if (NEU_ERR_SUCCESS != error) {
            log_error_node(routine->plugin, "publish error code :%d, topoic:%s",
                           error, topic);
        }

        free(result);
    }
}

static void mqtt_routine_timeout(mqtt_routine_t *routine)
{
    pthread_mutex_lock(&routine->contexts_mtx);
    struct context *item     = NULL;
    struct context *tmp_item = NULL;
    for (item = TAILQ_FIRST(&routine->head); NULL != item; item = tmp_item) {
        tmp_item = TAILQ_NEXT(item, entry);
        if (!item->ready && 0 == timeout(item)) {
            TAILQ_REMOVE(&routine->head, item, entry);
            mqtt_routine_context_destroy(item);
        }
    }
    pthread_mutex_unlock(&routine->contexts_mtx);
}

static void mqtt_routine_state(mqtt_routine_t *routine)
{
    neu_plugin_t *plugin = routine->plugin;
    if (NULL != routine->client) {
        if (NEU_ERR_SUCCESS != neu_mqtt_client_is_connected(routine->client)) {
            plugin->common.link_state = NEU_PLUGIN_LINK_STATE_DISCONNECTED;
        } else {
            plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTED;
        }
    }
}

static void mqtt_routine_delay(mqtt_routine_t *routine)
{
    pthread_mutex_lock(&routine->contexts_mtx);
    if (TAILQ_EMPTY(&routine->head)) {
        pthread_mutex_unlock(&routine->contexts_mtx);
        usleep(INTERVAL);
        return;
    }
    pthread_mutex_unlock(&routine->contexts_mtx);
}

static void mqtt_routine_cleanup(mqtt_routine_t *routine)
{
    while (!TAILQ_EMPTY(&routine->head)) {
        struct context *item = NULL;
        item                 = TAILQ_FIRST(&routine->head);
        if (NULL != item) {
            TAILQ_REMOVE(&routine->head, item, entry);
            mqtt_routine_context_destroy(item);
        }
    }
}

static void *mqtt_routine_loop(void *argument)
{
    assert(NULL != argument);

    mqtt_routine_t *routine = (mqtt_routine_t *) argument;

    pthread_mutex_lock(&routine->running_mtx);
    routine->running = true;
    pthread_mutex_unlock(&routine->running_mtx);

    while (true) {
        pthread_mutex_lock(&routine->running_mtx);
        if (!routine->running) {
            pthread_mutex_unlock(&routine->running_mtx);
            break;
        }
        pthread_mutex_unlock(&routine->running_mtx);

        mqtt_routine_publish(routine);
        mqtt_routine_timeout(routine);
        mqtt_routine_state(routine);
        mqtt_routine_delay(routine);
    }

    mqtt_routine_cleanup(routine);
    return NULL;
}

static mqtt_routine_t *mqtt_routine_start(neu_plugin_t *plugin,
                                          neu_config_t *config)
{
    assert(NULL != plugin);
    assert(NULL != config);

    mqtt_routine_t *routine = calloc(1, sizeof(mqtt_routine_t));
    if (NULL == routine) {
        return NULL;
    }

    // MQTT client start
    int rc = mqtt_option_init(config, &routine->option);
    if (0 != rc) {
        mqtt_option_uninit(&routine->option);
        free(routine);
        return NULL;
    }

    neu_mqtt_client_t *client = (neu_mqtt_client_t *) &routine->client;
    neu_err_code_e     error  = NEU_ERR_SUCCESS;
    error = neu_mqtt_client_open(client, &routine->option, plugin);
    if (NEU_ERR_SUCCESS != error) {
        mqtt_option_uninit(&routine->option);
        free(routine);
        return NULL;
    }

    neu_mqtt_client_continue(routine->client);

    log_info_node(plugin, "try open mqtt client: %s:%s, code:%d",
                  routine->option.host, routine->option.port, error);

    // routine start
    routine->plugin = plugin;
    TAILQ_INIT(&routine->head);
    pthread_mutex_init(&routine->contexts_mtx, NULL);
    pthread_mutex_init(&routine->running_mtx, NULL);

    if (0 !=
        pthread_create(&routine->daemon, NULL, mqtt_routine_loop, routine)) {
        log_error_node(plugin, "routine create error");
        neu_mqtt_client_close(client);
        free(routine);
        return NULL;
    }

    vector_init(&routine->topics, 1, sizeof(struct topic_pair));
    topics_generate(&routine->topics, routine->option.clientid,
                    routine->option.upload_topic);
    topics_subscribe(&routine->topics, routine->client);
    return routine;
}

static int mqtt_routine_send(mqtt_routine_t *routine, struct context *context)
{
    assert(NULL != routine);

    pthread_mutex_lock(&routine->contexts_mtx);
    TAILQ_INSERT_TAIL(&routine->head, context, entry);
    pthread_mutex_unlock(&routine->contexts_mtx);
    return NEU_ERR_SUCCESS;
}

static void mqtt_routine_stop(mqtt_routine_t *routine)
{
    assert(NULL != routine);

    // stop recevied
    neu_mqtt_client_suspend(routine->client);

    // stop routine and clean contexts
    pthread_mutex_lock(&routine->running_mtx);
    routine->running = false;
    pthread_mutex_unlock(&routine->running_mtx);
    pthread_join(routine->daemon, NULL);
    pthread_mutex_destroy(&routine->contexts_mtx);
    pthread_mutex_destroy(&routine->running_mtx);

    // close mqtt client and clean
    neu_mqtt_client_close(routine->client);
    topics_cleanup(&routine->topics);
    vector_uninit(&routine->topics);
    mqtt_option_uninit(&routine->option);
}

static void mqtt_routine_continue(mqtt_routine_t *routine)
{
    assert(NULL != routine && NULL != routine->client);

    neu_mqtt_client_continue(routine->client);
}

static void mqtt_routine_suspend(mqtt_routine_t *routine)
{
    assert(NULL != routine && NULL != routine->client);

    neu_mqtt_client_suspend(routine->client);
}

static neu_plugin_t *mqtt_plugin_open(neu_adapter_t *            adapter,
                                      const adapter_callbacks_t *callbacks)
{
    assert(NULL != adapter);
    assert(NULL != callbacks);

    neu_plugin_t *plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    if (NULL == plugin) {
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);
    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;

    const char *name = neu_plugin_module.module_name;
    log_info_node(plugin, "success to create plugin: %s", name);
    return plugin;
}

static int mqtt_plugin_close(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    const char *name = neu_plugin_module.module_name;
    log_info_node(plugin, "success to free plugin:%s", name);

    free(plugin);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_init(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    plugin->routine         = NULL;
    plugin->routine_running = false;
    pthread_mutex_init(&plugin->mutex, NULL);

    const char *name = neu_plugin_module.module_name;
    log_info_node(plugin, "initialize plugin: %s", name);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    pthread_mutex_lock(&plugin->mutex);
    if (plugin->routine_running) {
        mqtt_routine_stop(plugin->routine);
        plugin->routine_running = false;
        if (NULL != plugin->routine) {
            free(plugin->routine);
            plugin->routine = NULL;
        }
    }
    pthread_mutex_unlock(&plugin->mutex);

    pthread_mutex_destroy(&plugin->mutex);
    plugin->common.link_state = NEU_PLUGIN_LINK_STATE_DISCONNECTED;
    const char *name          = neu_plugin_module.module_name;
    log_info_node(plugin, "uninitialize plugin: %s", name);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_config(neu_plugin_t *plugin, neu_config_t *config)
{
    assert(NULL != plugin);
    assert(NULL != config);

    pthread_mutex_lock(&plugin->mutex);
    if (plugin->routine_running) {
        mqtt_routine_stop(plugin->routine);
        plugin->routine_running = false;
        if (NULL != plugin->routine) {
            free(plugin->routine);
            plugin->routine = NULL;
        }
    }
    pthread_mutex_unlock(&plugin->mutex);

    mqtt_routine_t *routine = mqtt_routine_start(plugin, config);
    if (NULL == routine) {
        pthread_mutex_lock(&plugin->mutex);
        plugin->routine         = NULL;
        plugin->routine_running = false;
        pthread_mutex_unlock(&plugin->mutex);
        return NEU_ERR_FAILURE;
    }

    pthread_mutex_lock(&plugin->mutex);
    plugin->routine         = routine;
    plugin->routine_running = true;
    pthread_mutex_unlock(&plugin->mutex);

    plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTING;

    log_info_node(plugin, "config plugin: %s", neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_start(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    pthread_mutex_lock(&plugin->mutex);
    if (plugin->routine_running) {
        mqtt_routine_continue(plugin->routine);
    }
    pthread_mutex_unlock(&plugin->mutex);

    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_stop(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    pthread_mutex_lock(&plugin->mutex);
    if (plugin->routine_running) {
        mqtt_routine_suspend(plugin->routine);
    }
    pthread_mutex_unlock(&plugin->mutex);

    return NEU_ERR_SUCCESS;
}

static neu_err_code_e read_response(neu_plugin_t *plugin, neu_request_t *req)
{
    pthread_mutex_lock(&plugin->mutex);

    if (!plugin->routine_running) {
        pthread_mutex_unlock(&plugin->mutex);
        return NEU_ERR_FAILURE;
    }

    neu_reqresp_read_resp_t *read_resp = (neu_reqresp_read_resp_t *) req->buf;

    pthread_mutex_lock(&plugin->routine->contexts_mtx);
    struct context *entry = NULL;
    entry = mqtt_routine_context_find(plugin->routine, req->req_id);
    if (NULL != entry) {
        char *json = command_read_once_response(
            plugin, req->sender_id, &entry->json_mqtt, read_resp->data_val);
        entry->result = json;
        entry->ready  = true;
    }
    pthread_mutex_unlock(&plugin->routine->contexts_mtx);

    pthread_mutex_unlock(&plugin->mutex);
    return NEU_ERR_SUCCESS;
}

static neu_err_code_e write_response(neu_plugin_t *plugin, neu_request_t *req)
{
    pthread_mutex_lock(&plugin->mutex);

    if (!plugin->routine_running) {
        pthread_mutex_unlock(&plugin->mutex);
        return NEU_ERR_FAILURE;
    }

    neu_reqresp_write_resp_t *write_resp = NULL;
    write_resp = (neu_reqresp_write_resp_t *) req->buf;

    pthread_mutex_lock(&plugin->routine->contexts_mtx);
    struct context *entry = NULL;
    entry = mqtt_routine_context_find(plugin->routine, req->req_id);
    if (NULL != entry) {
        char *json =
            command_write_response(&entry->json_mqtt, write_resp->data_val);
        entry->result = json;
        entry->ready  = true;
    }
    pthread_mutex_unlock(&plugin->routine->contexts_mtx);

    pthread_mutex_unlock(&plugin->mutex);
    return NEU_ERR_SUCCESS;
}

static neu_err_code_e trans_data(neu_plugin_t *plugin, neu_request_t *req)
{
    pthread_mutex_lock(&plugin->mutex);

    if (!plugin->routine_running) {
        pthread_mutex_unlock(&plugin->mutex);
        return NEU_ERR_FAILURE;
    }

    mqtt_routine_t *     routine   = plugin->routine;
    neu_reqresp_data_t * neu_data  = (neu_reqresp_data_t *) req->buf;
    struct topic_pair *  pair      = NULL;
    int                  type      = TOPIC_TYPE_UPLOAD;
    uint64_t             sender    = req->sender_id;
    const char *         node_name = req->node_name;
    neu_taggrp_config_t *config    = neu_data->grp_config;
    pair = vector_find_item(&routine->topics, (void *) &type, topic_type_match);
    char *json = command_read_periodic_response(plugin, sender, node_name,
                                                config, neu_data->data_val,
                                                plugin->routine->option.format);
    struct context *c = mqtt_routine_context_create(0, NULL, json, pair, true);
    if (NULL != c) {
        mqtt_routine_send(routine, c);
    }

    pthread_mutex_unlock(&plugin->mutex);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    assert(NULL != plugin);
    assert(NULL != req);

    neu_err_code_e error = NEU_ERR_SUCCESS;

    switch (req->req_type) {
    case NEU_REQRESP_READ_RESP:
        error = read_response(plugin, req);
        break;
    case NEU_REQRESP_WRITE_RESP:
        error = write_response(plugin, req);
        break;
    case NEU_REQRESP_TRANS_DATA:
        error = trans_data(plugin, req);
        break;
    default:
        error = NEU_ERR_FAILURE;
        break;
    }

    return error;
}

static int mqtt_plugin_event_reply(neu_plugin_t *     plugin,
                                   neu_event_reply_t *reply)
{
    UNUSED(plugin);
    UNUSED(reply);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    (void) plugin;
    (void) tag;
    return NEU_ERR_SUCCESS;
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
    .module_name  = "mqtt",
    .module_descr = "Neuron northbound MQTT communication plugin",
    .intf_funs    = &plugin_intf_funs,
    .kind         = PLUGIN_KIND_SYSTEM,
    .type         = NEU_NODE_TYPE_MQTT
};
