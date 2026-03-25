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

#include <pthread.h>

#include "define.h"
#include "errcodes.h"
#include "flight_sql_client.h"

#include "plugin.h"

#include "datalayers_config.h"
#include "datalayers_handle.h"
#include "datalayers_plugin.h"
#include "datalayers_plugin_intf.h"
#include <netinet/in.h>

extern const neu_plugin_module_t neu_plugin_module;

void create_consumer_thread(neu_plugin_t *plugin)
{
    pthread_mutex_lock(&plugin->queue_mutex);
    plugin->consumer_thread_stop_flag = false;
    pthread_mutex_unlock(&plugin->queue_mutex);

    int status = pthread_create(&plugin->consumer_thread, NULL,
                                (void *) db_write_task_consumer, plugin);
    if (status != 0) {
        plog_notice(plugin, "Failed to create consumer thread: %d\n", status);
    }
}

neu_plugin_t *datalayers_plugin_open(void)
{
    neu_plugin_t *plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    neu_plugin_common_init(&plugin->common);
    plugin->parse_config = datalayers_config_parse;
    plugin->config_seq   = 0;
    return plugin;
}

void stop_consumer_thread(pthread_t *consumer_thread, neu_plugin_t *plugin)
{
    pthread_mutex_lock(&plugin->queue_mutex);
    plugin->consumer_thread_stop_flag = true;
    pthread_cond_broadcast(&plugin->queue_not_empty);
    pthread_mutex_unlock(&plugin->queue_mutex);

    int join_status = pthread_join(*consumer_thread, NULL);
    if (join_status != 0) {
        plog_notice(plugin, "Failed to join consumer thread: %d\n",
                    join_status);
    }
}

int datalayers_plugin_close(neu_plugin_t *plugin)
{
    const char *name = neu_plugin_module.module_name;
    plog_notice(plugin, "success to free plugin:%s", name);

    free(plugin);

    return NEU_ERR_SUCCESS;
}

int datalayers_plugin_init(neu_plugin_t *plugin, bool load)
{
    (void) load;

    pthread_rwlock_init(&plugin->plugin_mutex, NULL);

    pthread_mutex_init(&plugin->queue_mutex, NULL);
    pthread_cond_init(&plugin->queue_not_empty, NULL);

    pthread_mutex_lock(&plugin->queue_mutex);
    plugin->consumer_thread_stop_flag = false;
    pthread_mutex_unlock(&plugin->queue_mutex);

    task_queue_init(&plugin->task_queue);

    pthread_create(&plugin->consumer_thread, NULL,
                   (void *) db_write_task_consumer, plugin);

    NEU_PLUGIN_REGISTER_CACHED_QUEUE_SIZE_METRIC(plugin);
    NEU_PLUGIN_REGISTER_MAX_CACHED_QUEUE_SIZE_METRIC(plugin);
    NEU_PLUGIN_REGISTER_DISCARDED_MSGS_METRIC(plugin);

    plog_notice(plugin, "initialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

int datalayers_plugin_uninit(neu_plugin_t *plugin)
{
    pthread_rwlock_wrlock(&plugin->plugin_mutex);
    plugin->config_seq++;

    stop_consumer_thread(&plugin->consumer_thread, plugin);
    pthread_mutex_lock(&plugin->queue_mutex);
    tasks_free(&plugin->task_queue);
    pthread_mutex_unlock(&plugin->queue_mutex);
    pthread_mutex_destroy(&plugin->queue_mutex);
    pthread_cond_destroy(&plugin->queue_not_empty);
    datalayers_config_fini(&plugin->config);

    route_tbl_free(plugin->route_tbl);
    plugin->route_tbl = NULL;

    client_destroy(plugin->client);
    plugin->client = NULL;

    pthread_rwlock_unlock(&plugin->plugin_mutex);
    pthread_rwlock_destroy(&plugin->plugin_mutex);

    plog_notice(plugin, "uninitialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static void *datalayers_connect_routine(void *arg)
{
    neu_plugin_t *plugin = (neu_plugin_t *) arg;
    char *        host   = NULL;
    char *        user   = NULL;
    char *        pass   = NULL;
    int           port   = 0;
    uint32_t      seq    = 0;

    pthread_rwlock_wrlock(&plugin->plugin_mutex);
    seq = plugin->config_seq;
    if (!plugin->config.host || !plugin->config.username ||
        !plugin->config.password) {
        pthread_rwlock_unlock(&plugin->plugin_mutex);
        return NULL;
    }
    host = strdup(plugin->config.host);
    user = strdup(plugin->config.username);
    pass = strdup(plugin->config.password);
    port = (int) plugin->config.port;
    pthread_rwlock_unlock(&plugin->plugin_mutex);

    if (!host || !user || !pass) {
        free(host);
        free(user);
        free(pass);
        return NULL;
    }

    neu_datalayers_client *c = client_create(host, port, user, pass);
    free(host);
    free(user);
    free(pass);

    pthread_rwlock_wrlock(&plugin->plugin_mutex);
    if (seq != plugin->config_seq) {
        if (c) {
            client_destroy(c);
        }
        pthread_rwlock_unlock(&plugin->plugin_mutex);
        return NULL;
    }
    if (c) {
        if (plugin->client) {
            client_destroy(plugin->client);
        }
        plugin->client            = c;
        plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    } else {
        plog_error(plugin, "datalayers async connect failed");
        plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    }
    pthread_rwlock_unlock(&plugin->plugin_mutex);
    return NULL;
}

static void datalayers_spawn_async_connect(neu_plugin_t *plugin)
{
    pthread_t tid;
    int st = pthread_create(&tid, NULL, datalayers_connect_routine, plugin);
    if (st != 0) {
        plog_error(plugin, "datalayers pthread_create connect failed: %d", st);
        return;
    }
    pthread_detach(tid);
}

int datalayers_plugin_config(neu_plugin_t *plugin, const char *setting)
{
    int                 rv          = 0;
    const char *        plugin_name = neu_plugin_module.module_name;
    datalayers_config_t config      = { 0 };

    rv = plugin->parse_config(plugin, setting, &config);
    if (0 != rv) {
        plog_error(plugin, "neu_datalayers_config_parse fail");
        return NEU_ERR_NODE_SETTING_INVALID;
    }

    plugin->config_seq++;

    if (plugin->client != NULL) {
        stop_consumer_thread(&plugin->consumer_thread, plugin);
        pthread_mutex_lock(&plugin->queue_mutex);
        tasks_free(&plugin->task_queue);
        pthread_mutex_unlock(&plugin->queue_mutex);
        client_destroy(plugin->client);
        plugin->client = NULL;
        create_consumer_thread(plugin);
    }

    datalayers_config_fini(&plugin->config);

    memmove(&plugin->config, &config, sizeof(config));

    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;

    plog_notice(plugin, "config plugin `%s` success (connect in background)",
                plugin_name);

    datalayers_spawn_async_connect(plugin);

    return NEU_ERR_SUCCESS;
}

int datalayers_plugin_start(neu_plugin_t *plugin)
{
    const char *plugin_name = neu_plugin_module.module_name;

    if (NULL == plugin->client) {
        plog_notice(plugin, "datalayers start: connect in background");
        datalayers_spawn_async_connect(plugin);
    }

    plog_notice(plugin, "start plugin `%s` success", plugin_name);
    if (plugin->client) {
        plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    } else {
        plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    }
    return NEU_ERR_SUCCESS;
}

int datalayers_plugin_stop(neu_plugin_t *plugin)
{

    plog_notice(plugin, "stop plugin `%s` success",
                neu_plugin_module.module_name);
    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    return NEU_ERR_SUCCESS;
}

int datalayers_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                              void *data)
{
    neu_err_code_e error = NEU_ERR_SUCCESS;

    switch (head->type) {
    case NEU_REQRESP_TRANS_DATA: {
        if (plugin->client == NULL) {
            plog_notice(plugin, "datalayers client is NULL, reconnect async");
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL,
                                     1, NULL);

            datalayers_spawn_async_connect(plugin);
        }

        error = handle_trans_data(plugin, data);
        break;
    }
    case NEU_REQ_SUBSCRIBE_GROUP:
        error = handle_subscribe_group(plugin, data);
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
    case NEU_REQRESP_NODE_DELETED: {
        neu_reqresp_node_deleted_t *req = data;
        if (0 != strcmp(plugin->common.name, req->node)) {
            error = handle_del_driver(plugin, data);
        }
        break;
    }
    default:
        error = NEU_ERR_EINTERNAL;
        break;
    }

    return error;
}

const neu_plugin_intf_funs_t datalayers_plugin_intf_funs = {
    .open    = datalayers_plugin_open,
    .close   = datalayers_plugin_close,
    .init    = datalayers_plugin_init,
    .uninit  = datalayers_plugin_uninit,
    .start   = datalayers_plugin_start,
    .stop    = datalayers_plugin_stop,
    .setting = datalayers_plugin_config,
    .request = datalayers_plugin_request,
};