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
#include <stdint.h>
#include <stdlib.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "vector.h"

#include "datatag_table.h"
#include "log.h"
#include "plugin.h"

#include "adapter_handle.h"
#include "datatag_handle.h"
#include "group_config_handle.h"
#include "http.h"
#include "license_handle.h"
#include "log_handle.h"
#include "normal_handle.h"
#include "plugin_handle.h"
#include "rw_handle.h"

#include "handle.h"

struct neu_rest_handle_ctx {
    void *plugin;
};

struct neu_rest_handle_ctx *rest_ctx = NULL;

struct neu_rest_handler web_handlers[] = {
    {
        .method     = NEU_REST_METHOD_GET,
        .type       = NEU_REST_HANDLER_DIRECTORY,
        .url        = "",
        .value.path = "./dist",
    },
};

static void cors(nng_aio *aio);

struct neu_rest_handler cors_handler[] = {
    {
        .url = "/api/v2/ping",
    },
    {
        .url = "/api/v2/login",
    },
    {
        .url = "/api/v2/tags",
    },
    {
        .url = "/api/v2/gconfig",
    },
    {
        .url = "/api/v2/node",
    },
    {
        .url = "/api/v2/plugin",
    },
    {
        .url = "/api/v2/tty",
    },
    {
        .url = "/api/v2/read",
    },
    {
        .url = "/api/v2/write",
    },
    {
        .url = "/api/v2/subscribe",
    },
    {
        .url = "/api/v2/schema",
    },
    {
        .url = "/api/v2/node/setting",
    },
    {
        .url = "/api/v2/node/ctl",
    },
    {
        .url = "/api/v2/node/state",
    },
    {
        .url = "/api/v2/log",
    },
    {
        .url = "/api/v2/license",
    },
};

struct neu_rest_handler api_handlers[] = {
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/ping",
        .value.handler = handle_ping,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/login",
        .value.handler = handle_login,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = handle_add_tags,

    },
    {
        .method        = NEU_REST_METHOD_PUT,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = handle_update_tags,

    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = handle_get_tags,
    },
    {
        .method        = NEU_REST_METHOD_DELETE,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = handle_del_tags,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/gconfig",
        .value.handler = handle_add_group_config,
    },
    {
        .method        = NEU_REST_METHOD_PUT,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/gconfig",
        .value.handler = handle_update_group_config,
    },
    {
        .method        = NEU_REST_METHOD_DELETE,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/gconfig",
        .value.handler = handle_del_group_config,
    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/gconfig",
        .value.handler = handle_get_group_config,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/node",
        .value.handler = handle_add_adapter,
    },
    {
        .method        = NEU_REST_METHOD_DELETE,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/node",
        .value.handler = handle_del_adapter,
    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/node",
        .value.handler = handle_get_adapter,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/plugin",
        .value.handler = handle_add_plugin,
    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/plugin",
        .value.handler = handle_get_plugin,
    },
    {
        .method        = NEU_REST_METHOD_DELETE,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/plugin",
        .value.handler = handle_del_plugin,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/read",
        .value.handler = handle_read,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/write",
        .value.handler = handle_write,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/subscribe",
        .value.handler = handle_grp_subscribe,
    },
    {
        .method        = NEU_REST_METHOD_DELETE,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/subscribe",
        .value.handler = handle_grp_unsubscribe,
    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/subscribe",
        .value.handler = handle_grp_get_subscribe,
    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/schema",
        .value.handler = handle_get_plugin_schema,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/node/setting",
        .value.handler = handle_set_node_setting,
    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/node/setting",
        .value.handler = handle_get_node_setting,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/node/ctl",
        .value.handler = handle_node_ctl,
    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/node/state",
        .value.handler = handle_get_node_state,
    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/log",
        .value.handler = handle_get_log,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/license",
        .value.handler = handle_set_license,
    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/license",
        .value.handler = handle_get_license,
    },
};

void neu_rest_web_handler(const struct neu_rest_handler **handlers,
                          uint32_t *                      size)
{
    *handlers = web_handlers;
    *size     = sizeof(web_handlers) / sizeof(struct neu_rest_handler);
}

void neu_rest_api_handler(const struct neu_rest_handler **handlers,
                          uint32_t *                      size)
{
    *handlers = api_handlers;
    *size     = sizeof(api_handlers) / sizeof(struct neu_rest_handler);
}

void neu_rest_api_cors_handler(const struct neu_rest_handler **handlers,
                               uint32_t *                      size)
{
    *handlers = cors_handler;
    *size     = sizeof(cors_handler) / sizeof(struct neu_rest_handler);

    for (uint32_t i = 0; i < *size; i++) {
        cors_handler[i].method        = NEU_REST_METHOD_OPTIONS;
        cors_handler[i].type          = NEU_REST_HANDLER_FUNCTION;
        cors_handler[i].value.handler = cors;
    }
}

neu_rest_handle_ctx_t *neu_rest_init_ctx(void *plugin)
{
    rest_ctx         = calloc(1, sizeof(neu_rest_handle_ctx_t));
    rest_ctx->plugin = plugin;

    return rest_ctx;
}

void neu_rest_free_ctx(neu_rest_handle_ctx_t *ctx)
{
    free(ctx);
}

void *neu_rest_get_plugin()
{
    return rest_ctx->plugin;
}

static void cors(nng_aio *aio)
{
    nng_http_res *res = NULL;

    nng_http_res_alloc(&res);

    nng_http_res_set_header(res, "Access-Control-Allow-Origin", "*");
    nng_http_res_set_header(res, "Access-Control-Allow-Methods",
                            "POST,GET,PUT,DELETE,OPTIONS");
    nng_http_res_set_header(res, "Access-Control-Allow-Headers", "*");

    nng_http_res_copy_data(res, " ", strlen(" "));
    nng_http_res_set_status(res, NNG_HTTP_STATUS_OK);

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
}
