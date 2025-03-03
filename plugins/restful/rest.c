/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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

#include "adapter_handle.h"
#include "argparse.h"
#include "datatag_handle.h"
#include "define.h"
#include "global_config_handle.h"
#include "group_config_handle.h"
#include "handle.h"
#include "normal_handle.h"
#include "otel/otel_manager.h"
#include "plugin_handle.h"
#include "rest.h"
#include "rw_handle.h"
#include "scan_handle.h"
#include "utils/http.h"
#include "utils/log.h"
#include "utils/neu_jwt.h"
#include "utils/time.h"
#include "json/neu_json_fn.h"

#define neu_plugin_module default_dashboard_plugin_module

struct neu_plugin {
    neu_plugin_common_t    common;
    nng_http_server *      server;
    neu_rest_handle_ctx_t *handle_ctx;
};

static nng_http_server *server_init()
{
    nng_url *        url;
    nng_http_server *server;

    nlog_notice("bind url: %s", host_port);

    int ret = nng_url_parse(&url, host_port);
    if (ret != 0) {
        nng_url_free(url);
        return NULL;
    }

    ret = nng_http_server_hold(&server, url);
    if (ret != 0) {
        nlog_error("rest api server bind error: %d", ret);
        return NULL;
    }
    nng_url_free(url);

    return server;
}

static neu_plugin_t *dashb_plugin_open(void)
{
    int                            rv;
    neu_plugin_t *                 plugin    = calloc(1, sizeof(neu_plugin_t));
    uint32_t                       n_handler = 0;
    const struct neu_http_handler *rest_handlers = NULL;
    const struct neu_http_handler *cors          = NULL;

    neu_plugin_common_init(&plugin->common);

    plugin->handle_ctx = neu_rest_init_ctx(plugin);

    plugin->server = server_init();

    if (plugin->server == NULL) {
        nlog_error("Failed to create RESTful server");
        goto server_init_fail;
    }

    neu_rest_handler(&rest_handlers, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        neu_http_add_handler(plugin->server, &rest_handlers[i]);
    }
    neu_rest_api_cors_handler(&cors, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        neu_http_add_handler(plugin->server, &cors[i]);
    }
    if ((rv = nng_http_server_start(plugin->server)) != 0) {
        nlog_error("Failed to start api server, error=%d", rv);
        goto server_init_fail;
    }

    nlog_notice("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;

server_init_fail:
    if (plugin->server != NULL) {
        nng_http_server_release(plugin->server);
    }
    neu_rest_free_ctx(plugin->handle_ctx);
    free(plugin);
    return NULL;
}

static int dashb_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    nng_http_server_stop(plugin->server);
    nng_http_server_release(plugin->server);

    free(plugin);
    nlog_notice("Success to free plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dashb_plugin_init(neu_plugin_t *plugin, bool load)
{
    (void) load;
    int rv = 0;

    (void) plugin;

    rv = neu_jwt_init(g_config_dir);

    nlog_notice("Initialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dashb_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    nlog_notice("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dashb_plugin_config(neu_plugin_t *plugin, const char *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    nlog_notice("config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dashb_plugin_request(neu_plugin_t *      plugin,
                                neu_reqresp_head_t *header, void *data)
{
    (void) plugin;

    if (header->ctx && nng_aio_get_input(header->ctx, 3)) {
        // catch all response messages for global config request
        handle_global_config_resp(header->ctx, header->type, data);
        return 0;
    }

    neu_otel_trace_ctx trace = NULL;
    neu_otel_scope_ctx scope = NULL;
    if (neu_otel_control_is_started() && header->ctx) {
        trace = neu_otel_find_trace(header->ctx);
        if (trace) {
            scope = neu_otel_add_span(trace);
            neu_otel_scope_set_span_name(scope, "rest response");
            char new_span_id[36] = { 0 };
            neu_otel_new_span_id(new_span_id);
            neu_otel_scope_set_span_id(scope, new_span_id);
            uint8_t *p_sp_id = neu_otel_scope_get_pre_span_id(scope);
            if (p_sp_id) {
                neu_otel_scope_set_parent_span_id2(scope, p_sp_id, 8);
            }
            neu_otel_scope_add_span_attr_int(scope, "thread id",
                                             (int64_t)(pthread_self()));
            neu_otel_scope_set_span_start_time(scope, neu_time_ns());
        }
    }

    switch (header->type) {
    case NEU_RESP_ERROR: {
        neu_resp_error_t *error = (neu_resp_error_t *) data;
        NEU_JSON_RESPONSE_ERROR(error->error, {
            neu_http_response(header->ctx, error->error, result_error);
        });
        if (neu_otel_control_is_started() && trace) {
            if (error->error != NEU_ERR_SUCCESS) {
                neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_ERROR,
                                                error->error);
            } else {
                neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK,
                                                error->error);
            }
        }
        break;
    }
    case NEU_RESP_WRITE_TAGS: {
        handle_write_tags_resp(header->ctx, (neu_resp_write_tags_t *) data);
        break;
    }
    case NEU_RESP_CHECK_SCHEMA: {
        handle_get_plugin_schema_resp(header->ctx,
                                      (neu_resp_check_schema_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    }
    case NEU_RESP_GET_PLUGIN:
        handle_get_plugin_resp(header->ctx, (neu_resp_get_plugin_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_GET_NODE:
        handle_get_adapter_resp(header->ctx, (neu_resp_get_node_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_GET_GROUP:
        handle_get_group_resp(header->ctx, (neu_resp_get_group_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_GET_DRIVER_GROUP:
        handle_get_driver_group_resp(header->ctx,
                                     (neu_resp_get_driver_group_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_ADD_TAG:
        handle_add_tags_resp(header->ctx, (neu_resp_add_tag_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_add_span_attr_int(
                scope, "error", ((neu_resp_add_tag_t *) data)->error);
        }
        break;
    case NEU_RESP_ADD_GTAG:
        handle_add_gtags_resp(header->ctx, (neu_resp_add_tag_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_add_span_attr_int(
                scope, "error", ((neu_resp_add_tag_t *) data)->error);
        }
        break;
    case NEU_RESP_UPDATE_TAG:
        handle_update_tags_resp(header->ctx, (neu_resp_update_tag_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_add_span_attr_int(
                scope, "error", ((neu_resp_update_tag_t *) data)->error);
        }
        break;
    case NEU_RESP_GET_TAG:
        handle_get_tags_resp(header->ctx, (neu_resp_get_tag_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_GET_SUBSCRIBE_GROUP:
        handle_grp_get_subscribe_resp(header->ctx,
                                      (neu_resp_get_subscribe_group_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_GET_NODE_SETTING:
        handle_get_node_setting_resp(header->ctx,
                                     (neu_resp_get_node_setting_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_GET_NODES_STATE:
        handle_get_nodes_state_resp(header->ctx,
                                    (neu_resp_get_nodes_state_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_GET_NODE_STATE:
        handle_get_node_state_resp(header->ctx,
                                   (neu_resp_get_node_state_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_READ_GROUP:
        handle_read_resp(header->ctx, (neu_resp_read_group_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_READ_GROUP_PAGINATE:
        handle_read_paginate_resp(header->ctx,
                                  (neu_resp_read_group_paginate_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_TEST_READ_TAG:
        handle_test_read_tag_resp(header->ctx,
                                  (neu_resp_test_read_tag_t *) data);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, 0);
        }
        break;
    case NEU_RESP_SCAN_TAGS:
        handle_scan_tags_resp(header->ctx, (neu_resp_scan_tags_t *) data);
        if (neu_otel_control_is_started() && trace) {
            if (((neu_resp_scan_tags_t *) data)->error != NEU_ERR_SUCCESS) {
                neu_otel_scope_set_status_code2(
                    scope, NEU_OTEL_STATUS_ERROR,
                    ((neu_resp_scan_tags_t *) data)->error);
            } else {
                neu_otel_scope_set_status_code2(
                    scope, NEU_OTEL_STATUS_OK,
                    ((neu_resp_scan_tags_t *) data)->error);
            }
        }
        break;
    default:
        nlog_fatal("recv unhandle msg: %s",
                   neu_reqresp_type_string(header->type));
        assert(false);
        break;
    }

    if (neu_otel_control_is_started() && trace) {
        neu_otel_scope_set_span_end_time(scope, neu_time_ns());
        neu_otel_trace_set_final(trace);
    }

    return 0;
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

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = dashb_plugin_open,
    .close   = dashb_plugin_close,
    .init    = dashb_plugin_init,
    .uninit  = dashb_plugin_uninit,
    .start   = dashb_plugin_start,
    .stop    = dashb_plugin_stop,
    .setting = dashb_plugin_config,
    .request = dashb_plugin_request,
};

#define DEFAULT_DASHBOARD_PLUGIN_DESCR \
    "A restful plugin for dashboard webserver"

const neu_plugin_module_t default_dashboard_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-default-dashboard",
    .module_descr = DEFAULT_DASHBOARD_PLUGIN_DESCR,
    .intf_funs    = &plugin_intf_funs,
    .kind         = NEU_PLUGIN_KIND_SYSTEM,
    .type         = NEU_NA_TYPE_APP,
};
