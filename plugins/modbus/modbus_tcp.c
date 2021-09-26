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

struct subscribe_instance {
    neu_taggrp_config_t *   grp_configs;
    char *                  name;
    modbus_point_context_t *point_ctx;
    nng_aio *               aio;
    neu_plugin_t *          plugin;

    TAILQ_ENTRY(subscribe_instance) node;
};

struct neu_plugin {
    neu_plugin_common_t  common;
    pthread_mutex_t      mtx;
    neu_tcp_client_t *   client;
    neu_node_id_t        node_id;
    neu_datatag_table_t *tag_table;
    TAILQ_HEAD(, subscribe_instance) sub_instances;
};

static void    start_periodic_read(neu_plugin_t *       plugin,
                                   neu_taggrp_config_t *grp_config);
static void    stop_periodic_read(neu_plugin_t *       plugin,
                                  neu_taggrp_config_t *grp_config);
static void    tags_read(neu_plugin_t *plugin, neu_taggrp_config_t *grp_config);
static void    tags_periodic_read(nng_aio *aio, void *arg, int code);
static ssize_t send_recv_callback(void *arg, char *send_buf, ssize_t send_len,
                                  char *recv_buf, ssize_t recv_len);

static ssize_t send_recv_callback(void *arg, char *send_buf, ssize_t send_len,
                                  char *recv_buf, ssize_t recv_len)
{
    neu_plugin_t *plugin = (neu_plugin_t *) arg;
    return neu_tcp_client_send_recv(plugin->client, send_buf, send_len,
                                    recv_buf, recv_len);
}

static void start_periodic_read(neu_plugin_t *       plugin,
                                neu_taggrp_config_t *grp_config)
{

    struct subscribe_instance *sub_inst =
        calloc(1, sizeof(struct subscribe_instance));
    vector_t *ids      = neu_taggrp_cfg_get_datatag_ids(grp_config);
    nng_aio * aio      = NULL;
    uint32_t  interval = neu_taggrp_cfg_get_interval(grp_config);

    nng_aio_alloc(&aio, NULL, NULL);
    nng_aio_set_timeout(aio, interval != 0 ? interval : 10000);

    sub_inst->grp_configs = grp_config;
    sub_inst->point_ctx   = modbus_point_init(plugin);
    sub_inst->aio         = aio;
    sub_inst->plugin      = plugin;
    sub_inst->name        = strdup(neu_taggrp_cfg_get_name(grp_config));

    VECTOR_FOR_EACH(ids, iter)
    {
        neu_datatag_id_t *id  = (neu_datatag_id_t *) iterator_get(&iter);
        neu_datatag_t *   tag = neu_datatag_tbl_get(plugin->tag_table, *id);

        if ((tag->attribute & NEU_ATTRIBUTETYPE_READ) ==
            NEU_ATTRIBUTETYPE_READ) {
            switch (tag->type) {
            case NEU_DATATYPE_WORD:
                modbus_point_add(sub_inst->point_ctx, tag->addr_str,
                                 MODBUS_B16);
                break;
            case NEU_DATATYPE_DWORD:
                modbus_point_add(sub_inst->point_ctx, tag->addr_str,
                                 MODBUS_B32);
                break;
            case NEU_DATATYPE_BOOLEAN:
                modbus_point_add(sub_inst->point_ctx, tag->addr_str, MODBUS_B8);
                break;
            default:
                break;
            }
        }
    }

    modbus_point_new_cmd(sub_inst->point_ctx);

    log_info("pre start periodic read, grp: %s(%p), interval: %d",
             sub_inst->name, grp_config, interval);
    pthread_mutex_lock(&plugin->mtx);
    TAILQ_INSERT_TAIL(&plugin->sub_instances, sub_inst, node);
    pthread_mutex_unlock(&plugin->mtx);

    log_info("start periodic read, grp: %s(%p), interval: %d", sub_inst->name,
             grp_config, interval);
    nng_aio_defer(aio, tags_periodic_read, sub_inst);
}

static void stop_periodic_read(neu_plugin_t *       plugin,
                               neu_taggrp_config_t *grp_config)
{
    struct subscribe_instance *sub_inst = NULL;

    pthread_mutex_lock(&plugin->mtx);
    TAILQ_FOREACH(sub_inst, &plugin->sub_instances, node)
    {
        if (sub_inst->grp_configs == grp_config) {
            TAILQ_REMOVE(&plugin->sub_instances, sub_inst, node);
            nng_aio_cancel(sub_inst->aio);
            modbus_point_destory(sub_inst->point_ctx);
            free(sub_inst->name);
            free(sub_inst);
            log_info("stop periodic read, grp: %s(%p)", sub_inst->name,
                     grp_config);
            break;
        }
    }
    pthread_mutex_unlock(&plugin->mtx);
}

static void tags_read(neu_plugin_t *plugin, neu_taggrp_config_t *grp_config)
{
    struct subscribe_instance *sub_inst = NULL;

    pthread_mutex_lock(&plugin->mtx);

    TAILQ_FOREACH(sub_inst, &plugin->sub_instances, node)
    {
        if (sub_inst->grp_configs == grp_config) {
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

static void tags_periodic_read(nng_aio *aio, void *arg, int code)
{
    switch (code) {
    case NNG_ETIMEDOUT: {
        struct subscribe_instance *sub_inst = (struct subscribe_instance *) arg;

        if (neu_taggrp_cfg_is_anchored(sub_inst->grp_configs)) {
            neu_response_t     resp      = { 0 };
            neu_reqresp_data_t data_resp = { 0 };

            modbus_point_all_read(sub_inst->point_ctx, true,
                                  send_recv_callback);
            // todo modbus point find;
            data_resp.grp_config = sub_inst->grp_configs;
            // data_resp.data_var
            resp.req_id    = 1;
            resp.resp_type = NEU_REQRESP_TRANS_DATA;
            resp.buf       = &data_resp;
            resp.buf_len   = sizeof(neu_reqresp_data_t);

            sub_inst->plugin->common.adapter_callbacks->response(
                sub_inst->plugin->common.adapter, &resp);

            nng_aio_defer(aio, tags_periodic_read, arg);
        } else {
            neu_taggrp_config_t *new_config = neu_system_find_group_config(
                sub_inst->plugin, sub_inst->plugin->node_id, sub_inst->name);

            stop_periodic_read(sub_inst->plugin, sub_inst->grp_configs);

            if (new_config != NULL) {
                start_periodic_read(sub_inst->plugin, new_config);
            }
        }

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
    struct subscribe_instance *sub_inst = NULL;

    pthread_mutex_lock(&plugin->mtx);
    sub_inst = TAILQ_FIRST(&plugin->sub_instances);

    while (sub_inst != NULL) {
        TAILQ_REMOVE(&plugin->sub_instances, sub_inst, node);

        nng_aio_cancel(sub_inst->aio);
        modbus_point_destory(sub_inst->point_ctx);
        free(sub_inst->name);
        free(sub_inst);

        sub_inst = TAILQ_FIRST(&plugin->sub_instances);
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
        plugin->node_id = neu_plugin_self_node_id(plugin);
        plugin->tag_table =
            neu_system_get_datatags_table(plugin, plugin->node_id);
    }

    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        neu_reqresp_read_t *data = (neu_reqresp_read_t *) req->buf;

        tags_read(plugin, data->grp_config);
        break;
    }
    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *data = (neu_reqresp_write_t *) req->buf;

        (void) data;
        break;
    }
    case NEU_REQRESP_SUBSCRIBE_NODE: {
        neu_reqresp_read_t *data = (neu_reqresp_read_t *) req->buf;

        start_periodic_read(plugin, data->grp_config);
        break;
    }
    case NEU_REQRESP_UNSUBSCRIBE_NODE: {
        neu_reqresp_read_t *data = (neu_reqresp_read_t *) req->buf;

        stop_periodic_read(plugin, data->grp_config);
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
