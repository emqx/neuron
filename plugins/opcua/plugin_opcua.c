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
#include <stdio.h>
#include <stdlib.h>

#include <config.h>
#include <neuron.h>

#include "option.h"

#include "open62541_client.h"
#include "plugin_handle.h"

#define UNUSED(x) (void) (x)

const neu_plugin_module_t neu_plugin_module;

struct neu_plugin {
    neu_plugin_common_t   common;
    option_t              option;
    open62541_client_t *  client;
    opc_handle_context_t *handle_context;
};

static neu_plugin_t *opcua_plugin_open(neu_adapter_t *            adapter,
                                       const adapter_callbacks_t *callbacks)
{
    neu_plugin_t *plugin;

    if (adapter == NULL || callbacks == NULL) {
        log_error("Open plugin with NULL adapter or callbacks");
        return NULL;
    }

    plugin = (neu_plugin_t *) malloc(sizeof(neu_plugin_t));
    if (plugin == NULL) {
        log_error("Failed to allocate plugin %s",
                  neu_plugin_module.module_name);
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);
    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    plugin->common.state.running     = NEU_PLUGIN_RUNNING_STATE_IDLE;
    plugin->common.state.link        = NEU_PLUGIN_LINK_STATE_CONNECTING;

    log_info("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;
}

static int opcua_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    free(plugin);
    log_info("Success to free plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int opcua_plugin_init(neu_plugin_t *plugin)
{
    int rc = opcua_option_init(&plugin->option);
    if (0 != rc) {
        log_error("OPCUA option init fail:%d initialize plugin failed: %s", rc,
                  neu_plugin_module.module_name);
        return -1;
    }

    plugin->handle_context = malloc(sizeof(opc_handle_context_t));
    memset(plugin->handle_context, 0, sizeof(opc_handle_context_t));
    plugin->handle_context->plugin       = plugin;
    plugin->handle_context->table        = NULL;
    plugin->handle_context->self_node_id = neu_plugin_self_node_id(plugin);

    NEU_LIST_INIT(&plugin->handle_context->subscribe_list,
                  opc_subscribe_tuple_t, node);

    rc = open62541_client_open(&plugin->option, plugin, &plugin->client);
    if (0 != rc) {
        log_error("Can not connect to opc.tcp://%s:%d", plugin->option.host,
                  plugin->option.port);

        plugin->handle_context->client = NULL;
        return -1;
    } else {
        log_info("Connected to opc.tcp://%s:%d", plugin->option.host,
                 plugin->option.port);
        plugin->handle_context->client = plugin->client;
    }

    plugin->common.state.running = NEU_PLUGIN_RUNNING_STATE_RUNNING;
    log_info("Initialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int opcua_plugin_uninit(neu_plugin_t *plugin)
{
    plugin_handle_stop(plugin->handle_context);
    open62541_client_close(plugin->client);
    opcua_option_uninit(&plugin->option);
    free(plugin->handle_context);

    plugin->common.state.running = NEU_PLUGIN_RUNNING_STATE_STOPPED;
    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int opcua_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{

    if (NULL == configs || NULL == configs->buf) {
        return -1;
    }

    int rc = opcua_option_init_by_config(configs, &plugin->option);
    if (0 != rc) {
        log_error("OPCUA option init fail:%d, initialize plugin failed: %s", rc,
                  neu_plugin_module.module_name);

        opcua_option_uninit(&plugin->option);
        return -1;
    }

    plugin->common.state.running = NEU_PLUGIN_RUNNING_STATE_READY;
    log_info("Config plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static void periodic_response(neu_plugin_t *plugin, neu_taggrp_config_t *config,
                              neu_data_val_t *resp_val)
{
    neu_plugin_response_trans_data(plugin, config, resp_val, 0);
}

static void subscribe_response(neu_plugin_t *       plugin,
                               neu_taggrp_config_t *config,
                               neu_data_val_t *     resp_val)
{
    UNUSED(plugin);
    UNUSED(config);
    UNUSED(resp_val);
    // TODO: return subscription data
}

static int opcua_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    int rv = 0;
    if (plugin == NULL || req == NULL) {
        log_warn("The plugin pointer or request is NULL");
        return -1;
    }

    if (NULL == plugin->handle_context->client) {
        return -2;
    }

    if (NULL == plugin->handle_context->table) {
        plugin->handle_context->table = neu_system_get_datatags_table(
            plugin, plugin->handle_context->self_node_id);
    }

    int rc = 0;
    UNUSED(rc);
    log_info("send request to plugin: %s", neu_plugin_module.module_name);
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        neu_reqresp_read_t *read_req = (neu_reqresp_read_t *) req->buf;
        neu_data_val_t *    resp_val;
        neu_response_t      resp;
        neu_reqresp_data_t  data_resp;

        resp_val = neu_dvalue_unit_new();
        if (NULL == resp_val) {
            log_error("Failed to allocate data value for response tags");
            rv = -1;
            break;
        }

        plugin_handle_read_once(plugin->handle_context, read_req->grp_config,
                                resp_val);

        data_resp.grp_config = read_req->grp_config;
        data_resp.data_val   = resp_val;
        if (NULL == data_resp.data_val) {
            rv = -1;
            break;
        }

        memset(&resp, 0, sizeof(resp));
        resp.req_id    = req->req_id;
        resp.resp_type = NEU_REQRESP_READ_RESP;
        resp.recver_id = req->sender_id;
        resp.buf_len   = sizeof(neu_reqresp_data_t);
        resp.buf       = &data_resp;
        rv = adapter_callbacks->response(plugin->common.adapter, &resp);
        break;
    }
    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *write_req = (neu_reqresp_write_t *) req->buf;
        neu_data_val_t *     write_val = write_req->data_val;

        neu_data_val_t *resp_val;
        resp_val = neu_dvalue_unit_new();
        if (resp_val == NULL) {
            log_error("Failed to allocate data value for response write data");
            break;
        }

        neu_response_t     resp;
        neu_reqresp_data_t data_resp;
        plugin_handle_write_value(plugin->handle_context, write_req->grp_config,
                                  write_val, resp_val);

        data_resp.grp_config = write_req->grp_config;
        data_resp.data_val   = resp_val;

        memset(&resp, 0, sizeof(resp));
        resp.req_id    = req->req_id;
        resp.resp_type = NEU_REQRESP_WRITE_RESP;
        resp.recver_id = req->sender_id;
        resp.buf_len   = sizeof(neu_reqresp_data_t);
        resp.buf       = &data_resp;
        rv = adapter_callbacks->response(plugin->common.adapter, &resp);

        break;
    }
    case NEU_REQRESP_SUBSCRIBE_NODE: {
        neu_reqresp_read_t *subscribe_req = (neu_reqresp_read_t *) req->buf;
        plugin_handle_subscribe(plugin->handle_context,
                                subscribe_req->grp_config, periodic_response,
                                subscribe_response);
        break;
    }
    case NEU_REQRESP_UNSUBSCRIBE_NODE: {
        neu_reqresp_read_t *unsubscribe_req = (neu_reqresp_read_t *) req->buf;
        plugin_handle_unsubscribe(plugin->handle_context,
                                  unsubscribe_req->grp_config);
        break;
    }

    default:
        break;
    }
    return rv;
}

static int opcua_plugin_event_reply(neu_plugin_t *     plugin,
                                    neu_event_reply_t *reply)
{
    int rv = 0;

    (void) plugin;
    (void) reply;

    log_info("reply event to plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open        = opcua_plugin_open,
    .close       = opcua_plugin_close,
    .init        = opcua_plugin_init,
    .uninit      = opcua_plugin_uninit,
    .config      = opcua_plugin_config,
    .request     = opcua_plugin_request,
    .event_reply = opcua_plugin_event_reply
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-opcua-plugin",
    .module_descr = "Neuron OPC-UA interactive communication plugin",
    .intf_funs    = &plugin_intf_funs
};
