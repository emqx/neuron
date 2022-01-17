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

#include "config.h"
#include "handle.h"
#include "http.h"
#include "neu_log.h"
#include "rest.h"
#include "rw_handle.h"
#include "utils/neu_jwt.h"

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

static nng_http_server *server_init(char *type)
{
    nng_url *        url;
    char *           host           = NULL;
    char *           port           = NULL;
    char             host_port[128] = { 0 };
    nng_http_server *server;

    host = neu_config_get_value("./neuron.yaml", 2, type, "host");
    port = neu_config_get_value("./neuron.yaml", 2, type, "port");

    snprintf(host_port, sizeof(host_port), "http://%s:%s", host, port);
    log_info("%s bind url: %s", type, host_port);

    free(host);
    free(port);

    int ret = nng_url_parse(&url, host_port);
    if (ret != 0) {
        nng_url_free(url);
        return NULL;
    }

    ret = nng_http_server_hold(&server, url);
    if (ret != 0) {
        return NULL;
    }
    nng_url_free(url);

    return server;
}

static neu_plugin_t *dashb_plugin_open(neu_adapter_t *            adapter,
                                       const adapter_callbacks_t *callbacks)
{
    int                            rv;
    neu_plugin_t *                 plugin;
    uint32_t                       n_handler     = 0;
    const struct neu_rest_handler *rest_handlers = NULL;
    const struct neu_rest_handler *cors          = NULL;

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

    plugin->web_server = server_init("web");
    plugin->api_server = server_init("api");

    if (plugin->web_server == NULL || plugin->api_server == NULL) {
        log_error("Failed to create web server and api server");
        goto server_init_fail;
    }

    neu_rest_api_handler(&rest_handlers, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        rest_add_handler(plugin->api_server, &rest_handlers[i]);
    }
    neu_rest_api_cors_handler(&cors, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        rest_add_handler(plugin->api_server, &cors[i]);
    }
    if ((rv = nng_http_server_start(plugin->api_server)) != 0) {
        log_error("Failed to start api server, error=%d", rv);
        goto api_server_start_fail;
    }

    neu_rest_web_handler(&rest_handlers, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        rest_add_handler(plugin->web_server, &rest_handlers[i]);
    }

    if ((rv = nng_http_server_start(plugin->web_server)) != 0) {
        log_error("Failed to start web server, error=%d", rv);
        goto web_server_start_fail;
    }

    handle_rw_init();
    log_info("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;

web_server_start_fail:
    nng_http_server_stop(plugin->api_server);
api_server_start_fail:
server_init_fail:
    if (plugin->web_server != NULL) {
        nng_http_server_release(plugin->web_server);
    }
    if (plugin->api_server != NULL) {
        nng_http_server_release(plugin->api_server);
    }
    neu_rest_free_ctx(plugin->handle_ctx);
    free(plugin);
    return NULL;
}

static int dashb_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    handle_rw_uninit();
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

    char *private_key_path = neu_config_get_value(
        (char *) "./neuron.yaml", 2, (char *) "api", (char *) "private_key");
    char *public_key_path = neu_config_get_value(
        (char *) "./neuron.yaml", 2, (char *) "api", (char *) "public_key");
    rv = neu_jwt_init(public_key_path, private_key_path);

    free(private_key_path);
    free(public_key_path);

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
    case NEU_REQRESP_READ_RESP:
        handle_read_resp(req);
        break;
    case NEU_REQRESP_WRITE_RESP:
        handle_write_resp(req);
        break;
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

static int dashb_plugin_start(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

static int dashb_plugin_stop(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

static int dashb_plugin_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    (void) plugin;
    (void) tag;

    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open         = dashb_plugin_open,
    .close        = dashb_plugin_close,
    .init         = dashb_plugin_init,
    .uninit       = dashb_plugin_uninit,
    .start        = dashb_plugin_start,
    .stop         = dashb_plugin_stop,
    .config       = dashb_plugin_config,
    .request      = dashb_plugin_request,
    .validate_tag = dashb_plugin_validate_tag,
    .event_reply  = dashb_plugin_event_reply
};

#define DEFAULT_DASHBOARD_PLUGIN_DESCR \
    "A restful plugin for dashboard webserver"

const neu_plugin_module_t default_dashboard_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-default-dashboard",
    .module_descr = DEFAULT_DASHBOARD_PLUGIN_DESCR,
    .intf_funs    = &plugin_intf_funs
};
