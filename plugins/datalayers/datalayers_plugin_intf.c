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
    pthread_mutex_lock(&plugin->plugin_mutex);
    plugin->consumer_thread_stop_flag = false;
    pthread_mutex_unlock(&plugin->plugin_mutex);

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
    return plugin;
}

void stop_consumer_thread(pthread_t *consumer_thread, neu_plugin_t *plugin)
{
    pthread_mutex_lock(&plugin->plugin_mutex);
    plugin->consumer_thread_stop_flag = true;
    pthread_mutex_unlock(&plugin->plugin_mutex);

    int join_status = pthread_join(*consumer_thread, NULL);
    if (join_status != 0) {
        plog_notice(plugin, "Failed to join consumer thread: %d\n",
                    join_status);
    }
}

int datalayers_plugin_close(neu_plugin_t *plugin)
{
    const char *name = neu_plugin_module.module_name;

    pthread_mutex_lock(&plugin->plugin_mutex);

    plog_notice(plugin, "success to free plugin:%s", name);

    stop_consumer_thread(&plugin->consumer_thread, plugin);

    tasks_free(&plugin->task_queue);

    client_destroy(plugin->client);
    free(plugin);

    pthread_mutex_unlock(&plugin->plugin_mutex);

    return NEU_ERR_SUCCESS;
}

int datalayers_plugin_init(neu_plugin_t *plugin, bool load)
{
    (void) load;

    pthread_mutex_init(&plugin->plugin_mutex, NULL);

    task_queue_init(&plugin->task_queue);

    pthread_create(&plugin->consumer_thread, NULL,
                   (void *) db_write_task_consumer, plugin);
    pthread_detach(plugin->consumer_thread);

    NEU_PLUGIN_REGISTER_CACHED_QUEUE_SIZE_METRIC(plugin);
    NEU_PLUGIN_REGISTER_MAX_CACHED_QUEUE_SIZE_METRIC(plugin);
    NEU_PLUGIN_REGISTER_DISCARDED_MSGS_METRIC(plugin);

    plog_notice(plugin, "initialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

int datalayers_plugin_uninit(neu_plugin_t *plugin)
{
    pthread_mutex_lock(&plugin->plugin_mutex);

    stop_consumer_thread(&plugin->consumer_thread, plugin);

    tasks_free(&plugin->task_queue);
    datalayers_config_fini(&plugin->config);

    route_tbl_free(plugin->route_tbl);
    plugin->route_tbl = NULL;

    pthread_mutex_unlock(&plugin->plugin_mutex);
    pthread_mutex_destroy(&plugin->plugin_mutex);

    plog_notice(plugin, "uninitialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int config_datalayers_client(neu_plugin_t *             plugin,
                                    const datalayers_config_t *config)
{
    if (!config->host || !config->port || !config->username ||
        !config->password) {
        plog_error(
            plugin,
            "Invalid datalayers config: NULL host/port/username/password");
        return NEU_ERR_DATALAYERS_INIT_FAILURE;
    }

    plugin->client = client_create(config->host, config->port, config->username,
                                   config->password);

    if (NULL == plugin->client) {
        plog_error(plugin, "datalayers client_create failed");
        plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
        return NEU_ERR_DATALAYERS_CONNECT_FAILURE;
    }

    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    return NEU_ERR_SUCCESS;
}

int datalayers_plugin_config(neu_plugin_t *plugin, const char *setting)
{
    int                 rv          = 0;
    const char *        plugin_name = neu_plugin_module.module_name;
    datalayers_config_t config      = { 0 };
    if (plugin->client != NULL) {
        stop_consumer_thread(&plugin->consumer_thread, plugin);
        tasks_free(&plugin->task_queue);
        client_destroy(plugin->client);
        plugin->client = NULL;
    }

    rv = plugin->parse_config(plugin, setting, &config);
    if (0 != rv) {
        plog_error(plugin, "neu_datalayers_config_parse fail");
        return NEU_ERR_NODE_SETTING_INVALID;
    }

    rv = config_datalayers_client(plugin, &config);

    datalayers_config_fini(&plugin->config);

    memmove(&plugin->config, &config, sizeof(config));

    if (rv == 0) {
        plog_notice(plugin, "config plugin `%s` success", plugin_name);
        create_consumer_thread(plugin);
    } else {
        plog_error(plugin, "datalayers client configuration failed");
    }

    return rv;
}

int datalayers_plugin_start(neu_plugin_t *plugin)
{
    const char *plugin_name = neu_plugin_module.module_name;

    if (NULL == plugin->client) {
        plog_error(plugin, "datalayers client_create failed");
        return NEU_ERR_DATALAYERS_CONNECT_FAILURE;
    }

    plog_notice(plugin, "start plugin `%s` success", plugin_name);
    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
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
            plog_notice(plugin, "datalayers client is NULL, reconnect");
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL,
                                     1, NULL);

            config_datalayers_client(plugin, &plugin->config);
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