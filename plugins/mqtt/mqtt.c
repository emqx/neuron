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

#include "command/command.h"
#include "mqtt.h"
#include "util.h"

#define INTERVAL 100000U
#define TIMEOUT 3000U

#define TOPIC_READ_REQ "neuron/%s/read/req"
#define TOPIC_WRITE_REQ "neuron/%s/write/req"

#define TOPIC_READ_RES "neuron/%s/read/resp"
#define TOPIC_WRITE_RES "neuron/%s/write/resp"
#define TOPIC_UPLOAD_RES "neuron/%s/upload"

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
    neu_plugin_t *    plugin;
    neu_mqtt_option_t option;
    neu_mqtt_client_t client;
    UT_array *        topics;
};

static neu_plugin_t *plugin_log = NULL;

static char *topics_format(char *format, char *name)
{
    char temp[256] = { '\0' };
    int  rc        = snprintf(temp, 256, format, name);
    if (-1 != rc) {
        char *topic = strdup(temp);
        return topic;
    }

    return NULL;
}

static void topics_add(UT_array *topics, char *request, int qos_request,
                       char *response, int qos_response, int type)
{
    struct topic_pair *pair = calloc(1, sizeof(struct topic_pair));
    pair->topic_request     = request;
    pair->topic_response    = response;
    pair->qos_request       = qos_request;
    pair->qos_response      = qos_response;
    pair->type              = type;
    utarray_push_back(topics, &pair);
    plog_info(plugin_log, "add topic-req:%s, topic-res:%s, count:%d",
              pair->topic_request, pair->topic_response, utarray_len(topics));
}

static void topics_cleanup(UT_array *topics)
{
    for (struct topic_pair **pair =
             (struct topic_pair **) utarray_front(topics);
         NULL != pair;
         pair = (struct topic_pair **) utarray_next(topics, pair)) {

        plog_info(plugin_log, "remove topic-req:%s, topic-res:%s",
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

static void topics_generate(UT_array *topics, char *name, char *upload_topic)
{
    char *read_req = topics_format(TOPIC_READ_REQ, name);
    char *read_res = topics_format(TOPIC_READ_RES, name);
    topics_add(topics, read_req, QOS0, read_res, QOS0, TOPIC_TYPE_READ);

    char *write_req = topics_format(TOPIC_WRITE_REQ, name);
    char *write_res = topics_format(TOPIC_WRITE_RES, name);
    topics_add(topics, write_req, QOS0, write_res, QOS0, TOPIC_TYPE_WRITE);

    /// UPLOAD TOPIC SETTING
    char *upload_req = NULL;
    char *upload_res = NULL;
    if (NULL != upload_topic && 0 < strlen(upload_topic)) {
        upload_res = strdup(upload_topic);
    } else {
        upload_res = topics_format(TOPIC_UPLOAD_RES, name);
    }

    topics_add(topics, upload_req, QOS0, upload_res, QOS0, TOPIC_TYPE_UPLOAD);
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
    int rc = mqtt_option_init((char *) config, &routine->option);
    if (0 != rc) {
        mqtt_option_uninit(&routine->option);
        free(routine);
        return NULL;
    }

    routine->option.state_update_func = mqtt_routine_state;
    routine->option.log               = plugin->common.log;
    neu_mqtt_client_t *client         = (neu_mqtt_client_t *) &routine->client;
    neu_err_code_e     error          = NEU_ERR_SUCCESS;
    error = neu_mqtt_client_open(client, &routine->option, plugin);
    if (NEU_ERR_SUCCESS != error) {
        mqtt_option_uninit(&routine->option);
        free(routine);
        return NULL;
    }

    neu_mqtt_client_continue(routine->client);

    plog_info(plugin, "open mqtt client: %s:%s, code:%d", routine->option.host,
              routine->option.port, error);

    routine->plugin = plugin;
    UT_icd ptr_icd  = { sizeof(struct topic_pair *), NULL, NULL, NULL };
    utarray_new(routine->topics, &ptr_icd);
    topics_generate(routine->topics, routine->option.clientid,
                    routine->option.upload_topic);
    topics_subscribe(routine->topics, routine->client);
    return routine;
}

static void mqtt_routine_stop(mqtt_routine_t *routine)
{
    assert(NULL != routine);

    // stop recevied
    neu_mqtt_client_suspend(routine->client);

    // close mqtt client and clean
    neu_mqtt_client_close(routine->client);
    plog_info(routine->plugin, "close mqtt client:%s:%s", routine->option.host,
              routine->option.port);

    topics_cleanup(routine->topics);
    utarray_free(routine->topics);
    mqtt_option_uninit(&routine->option);
}

static neu_plugin_t *mqtt_plugin_open(void)
{
    neu_plugin_t *plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    neu_plugin_common_init(&plugin->common);
    plugin_log = plugin; // for plog
    return plugin;
}

static int mqtt_plugin_close(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    const char *name = neu_plugin_module.module_name;
    plog_info(plugin, "success to free plugin:%s", name);

    plugin_log = NULL;
    free(plugin);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_init(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    plugin->routine  = NULL;
    plugin->running  = false;
    plugin->config   = NULL;
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
        plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTING;
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

    plugin_config_save(plugin, config);

    if (!plugin->running) {
        return NEU_ERR_SUCCESS;
    }

    plugin_stop_running(plugin);
    int rc = plguin_start_running(plugin);
    if (0 != rc) {
        return NEU_ERR_MQTT_FAILURE;
    }

    plog_info(plugin, "config plugin: %s", neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int mqtt_plugin_start(neu_plugin_t *plugin)
{
    assert(NULL != plugin);

    int rc = plguin_start_running(plugin);
    if (0 != rc) {
        return NEU_ERR_MQTT_FAILURE;
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

// static neu_err_code_e reconnect(neu_plugin_t *plugin, neu_reqresp_head_t
// *head)
// {
//     if (NEU_RESP_ERROR == head->type || NEU_RESP_READ_GROUP == head->type
//     ||
//         NEU_REQRESP_TRANS_DATA == head->type) {
//         if (!plugin->running && NULL != plugin->config) {
//             mqtt_routine_t *routine =
//                 mqtt_routine_start(plugin, plugin->config);
//             if (NULL == routine) {
//                 plugin->routine = NULL;
//                 plugin->running = false;
//                 return NEU_ERR_FAILURE;
//             }

//             plugin->routine           = routine;
//             plugin->running           = true;
//             plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTING;
//         }
//     }

//     return NEU_ERR_SUCCESS;
// }

static int mqtt_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                               void *data)
{
    assert(NULL != plugin);
    assert(NULL != head);
    assert(NULL != data);

    neu_err_code_e error = NEU_ERR_SUCCESS;
    // error                = reconnect(plugin, head);
    // if (NEU_ERR_SUCCESS != error) {
    //     return error;
    // }

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
    case NEU_RESP_APP_SUBSCRIBE_GROUP: {
        neu_resp_app_subscribe_group_t *sub =
            (neu_resp_app_subscribe_group_t *) data;

        utarray_free(sub->tags);
        break;
    }
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
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "mqtt",
    .module_descr = "Neuron northbound MQTT communication plugin",
    .intf_funs    = &plugin_intf_funs,
    .kind         = NEU_PLUGIN_KIND_SYSTEM,
    .type         = NEU_NA_TYPE_APP,
};
