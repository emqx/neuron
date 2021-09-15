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
#include <sys/queue.h>

#include <nng/nng.h>

#include "connection/neu_tcp.h"
#include "neu_datatag_table.h"
#include "neu_tag_group_config.h"
#include "neu_vector.h"
#include "neuron.h"

#include "modbus.h"
#include "modbus_point.h"

const neu_plugin_module_t neu_plugin_module;

struct reg_group_config {
    neu_taggrp_config_t *   configs;
    modbus_point_context_t *point_ctx;
    nng_aio *               aio;
    neu_plugin_t *          plugin;

    TAILQ_ENTRY(reg_group_config) node;
};

struct neu_plugin {
    neu_plugin_common_t  common;
    pthread_mutex_t      mtx;
    neu_tcp_client_t *   client;
    neu_datatag_table_t *tag_table;
    TAILQ_HEAD(, reg_group_config) grp_configs;
};

static void    start_cycle_read(neu_plugin_t *       plugin,
                                neu_taggrp_config_t *grp_config);
static void    stop_cycle_read(neu_plugin_t *       plugin,
                               neu_taggrp_config_t *grp_config);
static void    tags_read(neu_plugin_t *plugin, neu_taggrp_config_t *grp_config);
static void    tags_cycle_read(nng_aio *aio, void *arg, int code);
static ssize_t send_recv_callback(void *arg, char *send_buf, ssize_t send_len,
                                  char *recv_buf, ssize_t recv_len);

static ssize_t send_recv_callback(void *arg, char *send_buf, ssize_t send_len,
                                  char *recv_buf, ssize_t recv_len)
{
    neu_plugin_t *plugin = (neu_plugin_t *) arg;
    return neu_tcp_client_send_recv(plugin->client, send_buf, send_len,
                                    recv_buf, recv_len);
}

static void start_cycle_read(neu_plugin_t *       plugin,
                             neu_taggrp_config_t *grp_config)
{

    struct reg_group_config *gc  = calloc(1, sizeof(struct reg_group_config));
    vector_t *               ids = neu_taggrp_cfg_get_datatag_ids(grp_config);
    nng_aio *                aio = NULL;
    uint32_t                 interval = neu_taggrp_cfg_get_interval(grp_config);

    nng_aio_alloc(&aio, NULL, NULL);
    nng_aio_set_timeout(aio, interval != 0 ? interval : 10000);

    gc->configs   = grp_config;
    gc->point_ctx = modbus_point_init(plugin);
    gc->aio       = aio;
    gc->plugin    = plugin;

    VECTOR_FOR_EACH(ids, iter)
    {
        neu_datatag_id_t *id  = (neu_datatag_id_t *) iterator_get(&iter);
        neu_datatag_t *   tag = neu_datatag_tbl_get(plugin->tag_table, *id);

        if ((tag->attribute & NEU_ATTRIBUTETYPE_READ) ==
            NEU_ATTRIBUTETYPE_READ) {
            switch (tag->type) {
            case NEU_DATATYPE_WORD:
                modbus_point_add(gc->point_ctx, tag->addr_str, MODBUS_B16);
                break;
            case NEU_DATATYPE_DWORD:
                modbus_point_add(gc->point_ctx, tag->addr_str, MODBUS_B32);
                break;
            case NEU_DATATYPE_BOOLEAN:
                modbus_point_add(gc->point_ctx, tag->addr_str, MODBUS_B8);
                break;
            default:
                break;
            }
        }
    }

    modbus_point_new_cmd(gc->point_ctx);

    pthread_mutex_lock(&plugin->mtx);
    TAILQ_INSERT_TAIL(&plugin->grp_configs, gc, node);
    pthread_mutex_unlock(&plugin->mtx);

    log_info("start cycle read, grp: %p, interval: %d", grp_config, interval);
    nng_aio_defer(aio, tags_cycle_read, gc);
}

static void stop_cycle_read(neu_plugin_t *       plugin,
                            neu_taggrp_config_t *grp_config)
{
    struct reg_group_config *gc = NULL;

    pthread_mutex_lock(&plugin->mtx);
    TAILQ_FOREACH(gc, &plugin->grp_configs, node)
    {
        if (gc->configs == grp_config) {
            TAILQ_REMOVE(&plugin->grp_configs, gc, node);
            nng_aio_cancel(gc->aio);
            modbus_point_destory(gc->point_ctx);
            free(gc);
            log_info("stop cycle read, grp: %p", grp_config);
            break;
        }
    }
    pthread_mutex_unlock(&plugin->mtx);
}

static void tags_read(neu_plugin_t *plugin, neu_taggrp_config_t *grp_config)
{
    struct reg_group_config *gc = NULL;

    pthread_mutex_lock(&plugin->mtx);

    TAILQ_FOREACH(gc, &plugin->grp_configs, node)
    {
        if (gc->configs == grp_config) {
            // todo modbus_point_find
            neu_response_t     resp      = { 0 };
            neu_reqresp_data_t data_resp = { 0 };

            data_resp.grp_config = grp_config;
            // data_resp.data_var
            resp.req_id    = 1;
            resp.resp_type = NEU_REQRESP_TRANS_DATA;
            resp.buf       = &data_resp;
            resp.buf_len   = sizeof(neu_reqresp_data_t);

            plugin->common.adapter_callbacks->response(plugin->common.adapter,
                                                       &resp);
            log_info("find read grp: %p", grp_config);
            break;
        }
    }

    pthread_mutex_unlock(&plugin->mtx);

    return;
}

static void tags_cycle_read(nng_aio *aio, void *arg, int code)
{
    switch (code) {
    case NNG_ETIMEDOUT: {
        struct reg_group_config *gc        = (struct reg_group_config *) arg;
        neu_response_t           resp      = { 0 };
        neu_reqresp_data_t       data_resp = { 0 };

        // todo check group config update
        modbus_point_all_read(gc->point_ctx, true, send_recv_callback);
        // todo modbus point find;
        data_resp.grp_config = gc->configs;
        // data_resp.data_var
        resp.req_id    = 1;
        resp.resp_type = NEU_REQRESP_TRANS_DATA;
        resp.buf       = &data_resp;
        resp.buf_len   = sizeof(neu_reqresp_data_t);

        gc->plugin->common.adapter_callbacks->response(
            gc->plugin->common.adapter, &resp);

        nng_aio_defer(aio, tags_cycle_read, arg);
        break;
    }
    case NNG_ECANCELED:
        log_warn("aio: %p cancel", aio);
        break;
    default:
        log_warn("aio: %p, skip error: %d", aio, code);
        break;
    }
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

    neu_plugin_common_init(&plugin->common);

    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    plugin->common.state             = NEURON_PLUGIN_STATE_NULL;

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
    plugin->client = neu_tcp_client_create("192.168.8.27", 502);

    plugin->common.state = NEURON_PLUGIN_STATE_READY;
    log_info("modbus tcp init.....");
    return 0;
}

static int modbus_tcp_uninit(neu_plugin_t *plugin)
{
    struct reg_group_config *gc = NULL;

    pthread_mutex_lock(&plugin->mtx);
    gc = TAILQ_FIRST(&plugin->grp_configs);

    while (gc != NULL) {
        TAILQ_REMOVE(&plugin->grp_configs, gc, node);

        nng_aio_cancel(gc->aio);
        modbus_point_destory(gc->point_ctx);
        free(gc);

        gc = TAILQ_FIRST(&plugin->grp_configs);
    }
    pthread_mutex_unlock(&plugin->mtx);

    neu_tcp_client_close(plugin->client);

    pthread_mutex_destroy(&plugin->mtx);

    return 0;
}

static int modbus_tcp_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    (void) configs;
    (void) plugin;
    int ret = 0;

    log_info("modbus config.............");

    return ret;
}

static int modbus_tcp_request(neu_plugin_t *plugin, neu_request_t *req)
{
    if (plugin->tag_table == NULL) {
        plugin->tag_table = neu_system_get_datatags_table(
            plugin, neu_plugin_self_node_id(plugin));
    }

    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        neu_reqresp_read_t *data = (neu_reqresp_read_t *) req->buf;

        tags_read(plugin, data->grp_config);
        break;
    }
    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *data = (neu_reqresp_write_t *) req->buf;

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
