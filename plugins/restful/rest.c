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
    neu_plugin_common_t    common;
    nng_http_server *      api_server;
    nng_http_server *      web_server;
    neu_rest_handle_ctx_t *handle_ctx;
};

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
    log_info("rest add handler, method: %s, url: %s, ret: %d", method,
             rest_handler->url, ret);

    return ret;
}

static neu_plugin_t *dashb_plugin_open(neu_adapter_t *            adapter,
                                       const adapter_callbacks_t *callbacks)
{
    neu_plugin_t *                 plugin;
    nng_http_server *              api_server;
    nng_http_server *              web_server;
    nng_url *                      api_url;
    nng_url *                      web_url;
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

    neu_plugin_common_init(&plugin->common);
    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;

    plugin->handle_ctx = neu_rest_init_ctx(plugin);

    int ret = nng_url_parse(&web_url, "http://0.0.0.0:7000");
    if (ret != 0) {
        return NULL;
    }

    ret = nng_url_parse(&api_url, "http://0.0.0.0:7001");
    if (ret != 0) {
        return NULL;
    }

    ret = nng_http_server_hold(&web_server, web_url);
    if (ret != 0) {
        return NULL;
    }

    ret = nng_http_server_hold(&api_server, api_url);
    if (ret != 0) {
        return NULL;
    }

    neu_rest_api_handler(&rest_handlers, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        rest_add_handler(api_server, &rest_handlers[i]);
    }

    neu_rest_web_handler(&rest_handlers, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        rest_add_handler(web_server, &rest_handlers[i]);
    }

    ret = nng_http_server_start(web_server);
    if (ret != 0) {
        return NULL;
    }

    ret = nng_http_server_start(api_server);
    if (ret != 0) {
        return NULL;
    }

    plugin->web_server = web_server;
    plugin->api_server = api_server;
    nng_url_free(web_url);
    nng_url_free(api_url);

    log_info("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;
}

static int dashb_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    nng_http_server_stop(plugin->api_server);
    nng_http_server_release(plugin->api_server);
    nng_http_server_stop(plugin->web_server);
    nng_http_server_release(plugin->web_server);

    neu_rest_free_ctx(plugin->handle_ctx);

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
    (void) adapter_callbacks;

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
