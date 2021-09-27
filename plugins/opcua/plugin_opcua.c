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
    const char *default_cert_file =
        neu_config_get_value("./neuron.yaml", 2, "opcua", "default_cert_file");
    const char *default_key_file =
        neu_config_get_value("./neuron.yaml", 2, "opcua", "default_key_file");
    const char *host =
        neu_config_get_value("./neuron.yaml", 2, "opcua", "host");
    const char *port = neu_config_get_value("neuron.yaml", 2, "opcua", "port");
    const char *username =
        neu_config_get_value("./neuron.yaml", 2, "opcua", "username");
    const char *password =
        neu_config_get_value("./neuron.yaml", 2, "opcua", "password");
    const char *cert_file =
        neu_config_get_value("./neuron.yaml", 2, "opcua", "cert_file");
    const char *key_file =
        neu_config_get_value("./neuron.yaml", 2, "opcua", "key_file");

    // OPC-UA option
    plugin->option.default_cert_file = strdup(default_cert_file);
    plugin->option.default_key_file  = strdup(default_key_file);
    plugin->option.host              = strdup(host);
    plugin->option.port              = atoi(port);
    plugin->option.username          = strdup(username);
    plugin->option.password          = strdup(password);
    plugin->option.cert_file         = strdup(cert_file);
    plugin->option.key_file          = strdup(key_file);

    plugin->handle_context = malloc(sizeof(opc_handle_context_t));
    memset(plugin->handle_context, 0, sizeof(opc_handle_context_t));
    plugin->handle_context->plugin       = plugin;
    plugin->handle_context->table        = NULL;
    plugin->handle_context->self_node_id = neu_plugin_self_node_id(plugin);

    NEU_LIST_INIT(&plugin->handle_context->subscribe_list,
                  opc_subscribe_tuple_t, node);

    int rc = open62541_client_open(&plugin->option, plugin, &plugin->client);
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

    log_info("Initialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int opcua_plugin_uninit(neu_plugin_t *plugin)
{
    open62541_client_close(plugin->client);
    free(plugin->handle_context);
    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int opcua_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    log_info("config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static void periodic_response(neu_plugin_t *plugin, neu_taggrp_config_t *config,
                              neu_data_val_t *resp_val)
{
    UNUSED(plugin);
    UNUSED(config);
    UNUSED(resp_val);
    // neu_variable_t *head = neu_variable_create();
    // neu_plugin_response_trans_data(plugin, config, head, 0);
    // neu_variable_destroy(head);
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

    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        neu_reqresp_read_t *read_req = (neu_reqresp_read_t *) req->buf;
        neu_data_val_t *    resp_val;
        resp_val = neu_dvalue_unit_new();
        if (resp_val == NULL) {
            log_error("Failed to allocate data value for response tags");
            rv = -1;
            break;
        }

        plugin_handle_read_once(plugin->handle_context, read_req->grp_config,
                                resp_val);
        neu_plugin_response_trans_data(plugin, read_req->grp_config, resp_val,
                                       req->req_id);
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

        plugin_handle_write_value(plugin->handle_context, write_val, resp_val);
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
