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

#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>

#include "connection/neu_tcp.h"
#include "neu_datatag_table.h"
#include "neu_tag_group_config.h"
#include "neu_vector.h"
#include "neuron.h"

#include "modbus.h"
#include "modbus_point.h"

const neu_plugin_module_t neu_plugin_module;

enum process_status {
    STOP    = 0,
    RUNNING = 1,
};

enum device_connect_status {
    DISCONNECTED = 0,
    CONNECTED    = 1,
};

struct neu_plugin {
    neu_plugin_common_t        common;
    pthread_mutex_t            mtx;
    pthread_t                  loop;
    neu_tcp_client_t *         client;
    uint32_t                   interval;
    enum process_status        status;
    enum device_connect_status connect_status;
    neu_datatag_table_t *      tag_table;
    modbus_point_context_t *   point_ctx;
};

static void *  loop(void *arg);
static ssize_t send_recv_callback(void *arg, char *send_buf, ssize_t send_len,
                                  char *recv_buf, ssize_t recv_len);

static ssize_t send_recv_callback(void *arg, char *send_buf, ssize_t send_len,
                                  char *recv_buf, ssize_t recv_len)
{
    neu_plugin_t *plugin = (neu_plugin_t *) arg;
    return neu_tcp_client_send_recv(plugin->client, send_buf, send_len,
                                    recv_buf, recv_len);
}

static void *loop(void *arg)
{
    neu_plugin_t *plugin = (neu_plugin_t *) arg;

    while (1) {
        uint32_t interval = 0;

        pthread_mutex_lock(&plugin->mtx);
        if (plugin->status != RUNNING) {
            pthread_mutex_unlock(&plugin->mtx);
            break;
        }
        interval = plugin->interval;
        pthread_mutex_unlock(&plugin->mtx);

        usleep(interval * 1000);

        modbus_point_all_search(plugin->point_ctx, true, send_recv_callback);
    }

    return NULL;
}

static neu_plugin_t *modbus_tcp_open(neu_adapter_t *            adapter,
                                     const adapter_callbacks_t *callbacks)
{
    neu_plugin_t *plugin;

    if (adapter == NULL || callbacks == NULL) {
        log_error("Open plugin with NULL adapter or callbacks");
        return NULL;
    }

    plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    if (plugin == NULL) {
        log_error("Failed to allocate plugin %s",
                  neu_plugin_module.module_name);
        return NULL;
    }

    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;

    return plugin;
}

static int modbus_tcp_close(neu_plugin_t *plugin)
{
    free(plugin);
    return 0;
}

static int modbus_tcp_init(neu_plugin_t *plugin)
{

    pthread_mutex_init(&plugin->mtx, NULL);
    plugin->status    = RUNNING;
    plugin->point_ctx = modbus_point_init(plugin);

    modbus_point_add(plugin->point_ctx, "1!400001", MODBUS_B16);

    modbus_point_new_cmd(plugin->point_ctx);
    plugin->client   = neu_tcp_client_create("192.168.50.17", 502);
    plugin->interval = 10000;
    pthread_create(&plugin->loop, NULL, loop, plugin);

    log_info("modbus tcp init.....");
    return 0;
}

static int modbus_tcp_uninit(neu_plugin_t *plugin)
{

    pthread_mutex_lock(&plugin->mtx);
    plugin->status = STOP;
    pthread_mutex_unlock(&plugin->mtx);

    modbus_point_clean(plugin->point_ctx);

    neu_tcp_client_close(plugin->client);

    pthread_mutex_destroy(&plugin->mtx);

    return 0;
}

static int get_datatags_table(neu_plugin_t *plugin)
{
    int                     ret = 0;
    neu_request_t           cmd;
    neu_cmd_get_datatags_t  get_datatags_cmd;
    neu_response_t *        datatags_result = NULL;
    neu_reqresp_datatags_t *resp_datatags;

    cmd.req_type = NEU_REQRESP_GET_DATATAGS;
    cmd.req_id   = 2;
    cmd.buf      = (void *) &get_datatags_cmd;
    cmd.buf_len  = sizeof(neu_cmd_get_datatags_t);

    ret = plugin->common.adapter_callbacks->command(plugin->common.adapter,
                                                    &cmd, &datatags_result);

    if (ret < 0) {
        return -1;
    }
    resp_datatags     = (neu_reqresp_datatags_t *) datatags_result->buf;
    plugin->tag_table = resp_datatags->datatag_tbl;

    free(resp_datatags);
    free(datatags_result);
    return 0;
}

static int modbus_tcp_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    (void) configs;
    int ret = 0;

    plugin->client = neu_tcp_client_create("192.168.50.17", 502);
    log_info("modbus config.............");

    ret = get_datatags_table(plugin);

    return ret;
}

static int modbus_tcp_request(neu_plugin_t *plugin, neu_request_t *req)
{
    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        neu_reqresp_read_t *data = (neu_reqresp_read_t *) req->buf;
        uint32_t  interval = neu_taggrp_cfg_get_interval(data->grp_config);
        vector_t *tagv     = neu_taggrp_cfg_get_datatag_ids(data->grp_config);

        VECTOR_FOR_EACH(tagv, iter)
        {
            datatag_id_t * id  = (datatag_id_t *) iterator_get(&iter);
            neu_datatag_t *tag = neu_datatag_tbl_get(plugin->tag_table, *id);
            printf("%p\n", tag);
        }

        break;
    }
    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *data = (neu_reqresp_write_t *) req->buf;
        vector_t *tagv = neu_taggrp_cfg_get_datatag_ids(data->grp_config);

        VECTOR_FOR_EACH(tagv, iter)
        {
            datatag_id_t * id  = (datatag_id_t *) iterator_get(&iter);
            neu_datatag_t *tag = neu_datatag_tbl_get(plugin->tag_table, *id);

            (void) tag;
        }

        break;
    }
    default:
        break;
    }

    return 0;
}

static int modbus_tcp_event_reply(neu_plugin_t *     plugin,
                                  neu_event_reply_t *reqply)
{
    (void) plugin;
    (void) reqply;
    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open        = modbus_tcp_open,
    .close       = modbus_tcp_close,
    .init        = modbus_tcp_init,
    .uninit      = modbus_tcp_uninit,
    .config      = modbus_tcp_config,
    .request     = modbus_tcp_request,
    .event_reply = modbus_tcp_event_reply,
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-modbus-tcp-plugin",
    .module_descr = "modbus tcp driver plugin",
    .intf_funs    = &plugin_intf_funs,
};
