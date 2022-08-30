/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "command/command.h"
#include "mqtt_plugin.h"
#include "mqtt_util.h"

#define INTERVAL 100000U
#define TIMEOUT 3000U
#define MAX_TOPIC_LEN 256

#define TOPIC_READ_REQ "/neuron/%s/read/req"
#define TOPIC_WRITE_REQ "/neuron/%s/write/req"

#define TOPIC_READ_RES "/neuron/%s/read/resp"
#define TOPIC_WRITE_RES "/neuron/%s/write/resp"
#define TOPIC_UPLOAD_RES "/neuron/%s/upload"
#define TOPIC_HEARTBEAT_RES "/neuron/%s/heartbeat"

#define QOS0 0
#define QOS1 1
#define QOS2 2

const neu_plugin_module_t neu_plugin_module;

struct topic_pair {
    char *topic_request;
    char *topic_response;
    int   qos_request;
    int   qos_response;
    int   type;
};

struct mqtt_routine {
    neu_plugin_t *     plugin;
    neu_mqtt_option_t  option;
    neu_mqtt_client_t  client;
    UT_array *         topics;
    neu_mem_cache_t *  cache;
    nng_mtx *          mutex;
    neu_events_t *     events;
    neu_event_timer_t *send;
};

static char *topics_format(char *format, char *name)
{
    char *topic = calloc(1, MAX_TOPIC_LEN);
    if (NULL == topic) {
        return NULL;
    }

    int rc = snprintf(topic, MAX_TOPIC_LEN, format, name);
    if (-1 != rc) {
        return topic;
    }

    free(topic);
    return NULL;
}

static void topics_add(UT_array *topics, neu_plugin_t *plugin, char *request,
                       int qos_request, char *response, int qos_response,
                       int type)
{
    struct topic_pair *pair = calloc(1, sizeof(struct topic_pair));
    pair->topic_request     = request;
    pair->topic_response    = response;
    pair->qos_request       = qos_request;
    pair->qos_response      = qos_response;
    pair->type              = type;
    utarray_push_back(topics, &pair);

    plog_info(plugin, "add topic-req:%s, topic-res:%s, count:%d",
              pair->topic_request, pair->topic_response, utarray_len(topics));
}

static void topics_cleanup(UT_array *topics, neu_plugin_t *plugin)
{
    for (struct topic_pair **pair =
             (struct topic_pair **) utarray_front(topics);
         NULL != pair;
         pair = (struct topic_pair **) utarray_next(topics, pair)) {

        plog_info(plugin, "remove topic-req:%s, topic-res:%s",
                  (*pair)->topic_request, (*pair)->topic_response);

        if (NULL != (*pair)->topic_request) {
            free((*pair)->topic_request);
        }

        if (NULL != (*pair)->topic_response) {
            free((*pair)->topic_response);
        }

        free(*pair);
    }
}

static void topics_generate(UT_array *topics, neu_plugin_t *plugin,
                            neu_mqtt_option_t *option)
{
    char *name = plugin->common.name;

    // READ TOPIC
    char *read_req = topics_format(TOPIC_READ_REQ, name);
    char *read_res = topics_format(TOPIC_READ_RES, name);
    topics_add(topics, plugin, read_req, QOS0, read_res, QOS0, TOPIC_TYPE_READ);

    // WRITE TOPIC
    char *write_req = topics_format(TOPIC_WRITE_REQ, name);
    char *write_res = topics_format(TOPIC_WRITE_RES, name);
    topics_add(topics, plugin, write_req, QOS0, write_res, QOS0,
               TOPIC_TYPE_WRITE);

    /// UPLOAD TOPIC SETTING
    char *upload_res = strdup(option->upload_topic);
    topics_add(topics, plugin, NULL, QOS0, upload_res, QOS0, TOPIC_TYPE_UPLOAD);

    // HEARTBEAT TOPIC SETTING
    char *heartbeat_res = strdup(option->heartbeat_topic);
    topics_add(topics, plugin, NULL, QOS0, heartbeat_res, QOS0,
               TOPIC_TYPE_HEARTBEAT);
}

static struct topic_pair *topics_find_topic(UT_array *topics, const char *topic)
{
    for (struct topic_pair **pair =
             (struct topic_pair **) utarray_front(topics);
         NULL != pair;
         pair = (struct topic_pair **) utarray_next(topics, pair)) {
        if (0 == strcmp((*pair)->topic_request, topic)) {
            return *pair;
        }
    }

    return NULL;
}

static struct topic_pair *topics_find_type(UT_array *topics, int type)
{
    for (struct topic_pair **pair =
             (struct topic_pair **) utarray_front(topics);
         NULL != pair;
         pair = (struct topic_pair **) utarray_next(topics, pair)) {
        if (type == (*pair)->type) {
            return *pair;
        }
    }

    return NULL;
}

static int mqtt_routine_cache_send(void *data)
{
    mqtt_routine_t *routine = (mqtt_routine_t *) data;

    // size_t used_bytes = 0;
    // size_t used_items = 0;
    // neu_mem_cache_used(routine->cache, &used_bytes, &used_items);
    // plog_info(routine->plugin, "used bytes:%lu, used items:%lu", used_bytes,
    //           used_items);

    neu_err_code_e rc = neu_mqtt_client_is_connected(routine->client);
    if (0 != rc) {
        return -1;
    }

    nng_mtx_lock(routine->mutex);
    cache_item_t item = neu_mem_cache_earliest(routine->cache);
    if (NULL == item.data) {
        nng_mtx_unlock(routine->mutex);
        return -1;
    }

    int                type     = TOPIC_TYPE_UPLOAD;
    struct topic_pair *pair     = topics_find_type(routine->topics, type);
    char *             json_str = item.data;
    const char *       topic    = pair->topic_response;
    const int          qos      = pair->qos_response;
    neu_err_code_e     error =
        neu_mqtt_client_publish(routine->client, topic, qos,
                                (unsigned char *) json_str, strlen(json_str));
    if (NEU_ERR_SUCCESS != error) {
        plog_error(routine->plugin, "cache publish error code :%d, topoic:%s",
                   error, topic);
    }

    free(json_str);
    nng_mtx_unlock(routine->mutex);
    return 0;
}

static void mqtt_routine_response(const char *topic_name, size_t topic_len,
                                  void *payload, const size_t len,
                                  void *context)
{
    neu_plugin_t *     plugin  = (neu_plugin_t *) context;
    mqtt_routine_t *   routine = plugin->routine;
    struct topic_pair *pair    = topics_find_topic(routine->topics, topic_name);
    mqtt_response_t    response = { .topic_name = topic_name,
                                 .topic_len  = topic_len,
                                 .payload    = payload,
                                 .len        = len,
                                 .plugin     = context,
                                 .topic_pair = pair,
                                 .type       = pair->type };
    command_response_handle(&response);
}

static void topics_subscribe(UT_array *topics, neu_mqtt_client_t client)
{
    for (struct topic_pair **pair =
             (struct topic_pair **) utarray_front(topics);
         NULL != pair;
         pair = (struct topic_pair **) utarray_next(topics, pair)) {
        if (NULL != (*pair)->topic_request) {
            neu_mqtt_client_subscribe(client, (*pair)->topic_request,
                                      (*pair)->qos_request,
                                      mqtt_routine_response);
        }
    }
}

static void mqtt_routine_state(void *context, int state)
{
    if (NULL == context) {
        return;
    }

    neu_plugin_t *plugin = (neu_plugin_t *) context;
    if (0 == state) {
        plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    } else {
        plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    }
}

static mqtt_routine_t *mqtt_routine_start(neu_plugin_t *plugin,
                                          const char *  config)
{
    assert(NULL != plugin);
    assert(NULL != config);

    mqtt_routine_t *routine = calloc(1, sizeof(mqtt_routine_t));
    if (NULL == routine) {
        return NULL;
    }

    // MQTT client start
    int rc = mqtt_option_init(plugin, (char *) config, &routine->option);
    if (0 != rc) {
        plog_error(plugin, "%s", mqtt_option_error(rc));
        mqtt_option_uninit(plugin, &routine->option);
        free(routine);
        return NULL;
    }

    routine->option.state_update_func = mqtt_routine_state;
    routine->option.log               = plugin->common.log;
    neu_mqtt_client_t *client_p       = (neu_mqtt_client_t *) &routine->client;
    neu_err_code_e     error          = NEU_ERR_SUCCESS;
    error = neu_mqtt_client_open(client_p, &routine->option, plugin);
    if (NEU_ERR_SUCCESS != error) {
        mqtt_option_uninit(plugin, &routine->option);
        free(routine);
        return NULL;
    }

    neu_mqtt_client_continue(routine->client);

    plog_info(plugin, "open mqtt client: %s:%s, code:%d", routine->option.host,
              routine->option.port, error);

    // topics generate
    routine->plugin      = plugin;
    UT_icd topic_ptr_icd = { sizeof(struct topic_pair *), NULL, NULL, NULL };
    utarray_new(routine->topics, &topic_ptr_icd);
    topics_generate(routine->topics, plugin, &routine->option);
    topics_subscribe(routine->topics, routine->client);

    // cache send
    const size_t max_bytes = routine->option.cache * 1024 * 1024;
    const size_t max_items = 0;
    routine->cache         = neu_mem_cache_create(max_bytes, max_items);

    nng_mtx_alloc(&routine->mutex);
    routine->events               = neu_event_new();
    neu_event_timer_param_t param = {
        .second      = 0,
        .millisecond = 100,
        .usr_data    = routine,
        .cb          = mqtt_routine_cache_send,
    };

    routine->send = neu_event_add_timer(routine->events, param);
    return routine;
}

static void mqtt_routine_stop(mqtt_routine_t *routine)
{
    assert(NULL != routine);

    // stop cache send timer
    neu_event_del_timer(routine->events, routine->send);
    neu_event_close(routine->events);
    nng_mtx_free(routine->mutex);
    neu_mem_cache_destroy(routine->cache);

    // stop recevied
    neu_mqtt_client_suspend(routine->client);

    // close mqtt client and clean
    neu_mqtt_client_close(routine->client);
    plog_info(routine->plugin, "close mqtt client:%s:%s", routine->option.host,
              routine->option.port);

    topics_cleanup(routine->topics, routine->plugin);
    utarray_free(routine->topics);
    mqtt_option_uninit(routine->plugin, &routine->option);
}

static neu_plugin_t *mqtt_plugin_open(void)
{
    neu_plugin_t *plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    neu_plugin_common_init(&plugin->common);
    return plugin;
}

static int mqtt_plugin_close(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    const char *name = neu_plugin_module.module_name;
    plog_info(plugin, "success to free plugin:%s", name);

    free(plugin);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_init(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    plugin->routine = NULL;
    plugin->running = false;
    plugin->config  = NULL;

    const char *name = neu_plugin_module.module_name;
    plog_info(plugin, "initialize plugin: %s", name);
    return NEU_ERR_SUCCESS;
}

static int plguin_start_running(neu_plugin_t *plugin)
{
    if (!plugin->running && NULL != plugin->config) {
        mqtt_routine_t *routine = mqtt_routine_start(plugin, plugin->config);
        if (NULL == routine) {
            plugin->routine           = NULL;
            plugin->running           = false;
            plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
            return -1;
        }

        plugin->routine           = routine;
        plugin->running           = true;
        plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    }

    return 0;
}

static void plugin_stop_running(neu_plugin_t *plugin)
{
    if (plugin->running) {
        mqtt_routine_stop(plugin->routine);
        if (NULL != plugin->routine) {
            free(plugin->routine);
            plugin->routine = NULL;
        }

        plugin->running           = false;
        plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    }
}

static void plugin_config_free(neu_plugin_t *plugin)
{
    if (NULL != plugin->config) {
        free(plugin->config);
        plugin->config = NULL;
    }
}

static void plugin_config_save(neu_plugin_t *plugin, const char *config)
{
    plugin_config_free(plugin);
    plugin->config = strdup(config);
}

static int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    plugin_stop_running(plugin);
    plugin_config_free(plugin);

    const char *name = neu_plugin_module.module_name;
    plog_info(plugin, "uninitialize plugin: %s", name);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_config(neu_plugin_t *plugin, const char *config)
{
    assert(NULL != plugin);
    assert(NULL != config);

    if (NEU_ERR_SUCCESS != mqtt_option_validate(plugin, config)) {
        return NEU_ERR_NODE_SETTING_INVALID;
    }

    plugin_config_save(plugin, config);

    // Routine not running
    if (!plugin->running) {
        return NEU_ERR_SUCCESS;
    }

    plugin_stop_running(plugin);
    int rc = plguin_start_running(plugin);
    if (0 != rc) {
        return NEU_ERR_MQTT_INIT_FAILURE;
    }

    plog_info(plugin, "config plugin: %s", neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_start(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    int rc = plguin_start_running(plugin);
    if (0 != rc) {
        return NEU_ERR_NODE_SETTING_INVALID;
    }

    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_stop(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    plugin_stop_running(plugin);
    return NEU_ERR_SUCCESS;
}

static neu_err_code_e write_response(neu_plugin_t *      plugin,
                                     neu_reqresp_head_t *head,
                                     neu_resp_error_t *  data)
{
    neu_resp_error_t *write_data = data;

    if (!plugin->running) {
        return NEU_ERR_MQTT_FAILURE;
    }

    mqtt_routine_t *   routine = plugin->routine;
    int                type    = TOPIC_TYPE_WRITE;
    struct topic_pair *pair    = topics_find_type(routine->topics, type);
    char *      json_str = command_write_response(plugin, head, write_data);
    const char *topic    = pair->topic_response;
    const int   qos      = pair->qos_response;

    neu_err_code_e error =
        neu_mqtt_client_publish(routine->client, topic, qos,
                                (unsigned char *) json_str, strlen(json_str));
    if (NEU_ERR_SUCCESS != error) {
        plog_error(plugin, "write response publish error code :%d, topoic:%s",
                   error, topic);
        free(json_str);
        return error;
    }

    plog_info(plugin, "topic:%s, qos:%d, payload:%s", topic, qos, json_str);
    free(json_str);
    return NEU_ERR_SUCCESS;
}

static neu_err_code_e read_response(neu_plugin_t *      plugin,
                                    neu_reqresp_head_t *head, void *data)
{
    neu_resp_read_group_t *read_data = data;

    if (!plugin->running) {
        return NEU_ERR_MQTT_FAILURE;
    }

    mqtt_routine_t *   routine = plugin->routine;
    int                type    = TOPIC_TYPE_READ;
    struct topic_pair *pair    = topics_find_type(routine->topics, type);
    char *      json_str = command_read_once_response(plugin, head, read_data,
                                                routine->option.format);
    const char *topic    = pair->topic_response;
    const int   qos      = pair->qos_response;
    neu_err_code_e error =
        neu_mqtt_client_publish(routine->client, topic, qos,
                                (unsigned char *) json_str, strlen(json_str));
    if (NEU_ERR_SUCCESS != error) {
        plog_error(plugin, "read response publish error code :%d, topoic:%s",
                   error, topic);
        free(json_str);
        return error;
    }

    plog_info(plugin, "topic:%s, qos:%d, payload:%s", topic, qos, json_str);
    free(json_str);
    return NEU_ERR_SUCCESS;
}

static void cache_string_release(void *data)
{
    free(data);
}

static neu_err_code_e trans_data(neu_plugin_t *plugin, void *data)
{
    neu_reqresp_trans_data_t *trans_data = data;

    if (!plugin->running) {
        return NEU_ERR_MQTT_FAILURE;
    }

    mqtt_routine_t *   routine = plugin->routine;
    int                type    = TOPIC_TYPE_UPLOAD;
    struct topic_pair *pair    = topics_find_type(routine->topics, type);
    char *json_str = command_read_periodic_response(plugin, trans_data,
                                                    routine->option.format);
    if (NULL == json_str) {
        return NEU_ERR_MQTT_FAILURE;
    }

    neu_err_code_e rc = neu_mqtt_client_is_connected(routine->client);
    if (0 != rc) {
        cache_item_t item = {
            .size    = strlen(json_str) + 1,
            .data    = json_str,
            .release = cache_string_release,
        };

        nng_mtx_lock(routine->mutex);
        rc = neu_mem_cache_add(routine->cache, &item);
        nng_mtx_unlock(routine->mutex);

        if (0 != rc) {
            item.release(item.data);
        }

        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }

    const char *   topic = pair->topic_response;
    const int      qos   = pair->qos_response;
    neu_err_code_e error =
        neu_mqtt_client_publish(routine->client, topic, qos,
                                (unsigned char *) json_str, strlen(json_str));
    if (NEU_ERR_SUCCESS != error) {
        plog_error(plugin, "trans data publish error code :%d, topoic:%s",
                   error, topic);
        free(json_str);
        return error;
    }

    free(json_str);
    return NEU_ERR_SUCCESS;
}

static neu_err_code_e node_state_send(neu_plugin_t *      plugin,
                                      neu_reqresp_head_t *head, void *data)
{
    UNUSED(head);

    neu_reqresp_nodes_state_t *states  = (neu_reqresp_nodes_state_t *) data;
    mqtt_routine_t *           routine = plugin->routine;
    if (!plugin->running) {
        utarray_free(states->states);
        return NEU_ERR_MQTT_FAILURE;
    }

    int                type = TOPIC_TYPE_HEARTBEAT;
    struct topic_pair *pair = topics_find_type(routine->topics, type);
    char *             json_str =
        command_heartbeat_response(routine->plugin, states->states);
    if (NULL == json_str) {
        utarray_free(states->states);
        return -1;
    }

    const char *   topic = pair->topic_response;
    const int      qos   = pair->qos_response;
    neu_err_code_e error =
        neu_mqtt_client_publish(routine->client, topic, qos,
                                (unsigned char *) json_str, strlen(json_str));
    if (NEU_ERR_SUCCESS != error) {
        plog_error(routine->plugin,
                   "heartbeat publish error code :%d, topoic:%s", error, topic);
    }

    free(json_str);
    utarray_free(states->states);

    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                               void *data)
{
    assert(NULL != plugin);
    assert(NULL != head);
    assert(NULL != data);

    neu_err_code_e error = NEU_ERR_SUCCESS;

    switch (head->type) {
    case NEU_RESP_ERROR:
        error = write_response(plugin, head, data);
        break;
    case NEU_RESP_READ_GROUP:
        error = read_response(plugin, head, data);
        break;
    case NEU_REQRESP_TRANS_DATA:
        error = trans_data(plugin, data);
        break;
    case NEU_REQRESP_NODES_STATE: {
        error = node_state_send(plugin, head, data);
        break;
    }
    case NEU_REQRESP_NODE_DELETED:
        break;
    default:
        error = NEU_ERR_MQTT_FAILURE;
        break;
    }

    return error;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = mqtt_plugin_open,
    .close   = mqtt_plugin_close,
    .init    = mqtt_plugin_init,
    .uninit  = mqtt_plugin_uninit,
    .start   = mqtt_plugin_start,
    .stop    = mqtt_plugin_stop,
    .setting = mqtt_plugin_config,
    .request = mqtt_plugin_request,
};

const neu_plugin_module_t neu_plugin_module = {
    .version     = NEURON_PLUGIN_VER_1_0,
    .module_name = "mqtt",
    .module_descr =
        "Neuron northbound MQTT communication application. The data collected "
        "by Neuron from the device can be transmitted to the MQTT Broker "
        "through the MQTT application, and users can also send commands to "
        "Neuron through the MQTT application.",
    .intf_funs  = &plugin_intf_funs,
    .kind       = NEU_PLUGIN_KIND_SYSTEM,
    .type       = NEU_NA_TYPE_APP,
    .sub_msg[0] = NEU_SUBSCRIBE_NODES_STATE,
    .single     = false,
};
