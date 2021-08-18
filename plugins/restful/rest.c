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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "handle.h"
#include "neu_log.h"
#include "rest.h"

#define neu_plugin_module default_dashboard_plugin_module

struct neu_plugin {
    neu_plugin_common_t common;
    nng_http_server *   server;
    uint32_t            new_event_id;
};

/*
static uint32_t plugin_get_event_id(neu_plugin_t *plugin)
{
    uint32_t req_id;

    plugin->new_event_id++;
    if (plugin->new_event_id == 0) {
        plugin->new_event_id = 1;
    }

    req_id = plugin->new_event_id;
    return req_id;
}
*/

static int rest_add_handler(nng_http_server *              server,
                            const struct neu_rest_handler *rest_handler)
{
    nng_http_handler *handler;
    int               ret        = -1;
    char              method[16] = { 0 };

    switch (rest_handler->type) {
    case NEU_REST_HANDLER_FUNCTION:
        ret = nng_http_handler_alloc(&handler, rest_handler->url,
                                     rest_handler->value.handler);
        break;
    case NEU_REST_HANDLER_DIRECTORY:
        ret = nng_http_handler_alloc_directory(&handler, rest_handler->url,
                                               rest_handler->value.path);
        break;
    }

    if (ret != 0) {
        return -1;
    }

    switch (rest_handler->method) {
    case NEU_REST_METHOD_GET:
        ret = nng_http_handler_set_method(handler, "GET");
        strncpy(method, "GET", sizeof(method));
        break;
    case NEU_REST_METHOD_POST:
        ret = nng_http_handler_set_method(handler, "POST");
        strncpy(method, "POST", sizeof(method));
        break;
    case NEU_REST_METHOD_PUT:
        ret = nng_http_handler_set_method(handler, "PUT");
        strncpy(method, "PUT", sizeof(method));
        break;
    case NEU_REST_METHOD_DELETE:
        ret = nng_http_handler_set_method(handler, "DELETE");
        strncpy(method, "DELETE", sizeof(method));
        break;
    case NEU_REST_METHOD_OPTIONS:
        ret = nng_http_handler_set_method(handler, "OPTIONS");
        strncpy(method, "OPTIONS", sizeof(method));
        break;
    }

    if (ret != 0) {
        return -1;
    }

    ret = nng_http_server_add_handler(server, handler);
    log_info("rest add handler, method: %s, url: %s", method,
             rest_handler->url);

    return ret;
}

static neu_plugin_t *dashb_plugin_open(neu_adapter_t *            adapter,
                                       const adapter_callbacks_t *callbacks)
{
    neu_plugin_t *                 plugin;
    nng_http_server *              server;
    nng_url *                      url;
    uint32_t                       n_handler     = 0;
    const struct neu_rest_handler *rest_handlers = NULL;

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

    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    plugin->new_event_id             = 1;

    int ret = nng_url_parse(&url, "http://0.0.0.0:7000");
    if (ret != 0) {
        return NULL;
    }

    ret = nng_http_server_hold(&server, url);
    if (ret != 0) {
        return NULL;
    }

    neu_rest_init_all_handler(&rest_handlers, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        rest_add_handler(server, &rest_handlers[i]);
    }

    ret = nng_http_server_start(server);
    if (ret != 0) {
        return NULL;
    }

    plugin->server = server;
    nng_url_free(url);
    log_info("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;
}

static int dashb_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    nng_http_server_stop(plugin->server);
    nng_http_server_release(plugin->server);
    free(plugin);
    log_info("Success to free plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dashb_plugin_init(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    log_info("Initialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dashb_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dashb_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    log_info("config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dashb_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    int rv = 0;

    if (plugin == NULL || req == NULL) {
        log_warn("The plugin pointer or request is NULL");
        return (-1);
    }

    log_info("send request to plugin: %s", neu_plugin_module.module_name);
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    switch (req->req_type) {
    default:
        break;
    }
    return rv;
}

static int dashb_plugin_event_reply(neu_plugin_t *     plugin,
                                    neu_event_reply_t *reply)
{
    int rv = 0;

    (void) plugin;
    (void) reply;

    log_info("reply event to plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open        = dashb_plugin_open,
    .close       = dashb_plugin_close,
    .init        = dashb_plugin_init,
    .uninit      = dashb_plugin_uninit,
    .config      = dashb_plugin_config,
    .request     = dashb_plugin_request,
    .event_reply = dashb_plugin_event_reply
};

#define DEFAULT_DASHBOARD_PLUGIN_DESCR \
    "A restful plugin for dashboard webserver"

const neu_plugin_module_t default_dashboard_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-default-dashboard",
    .module_descr = DEFAULT_DASHBOARD_PLUGIN_DESCR,
    .intf_funs    = &plugin_intf_funs
};
