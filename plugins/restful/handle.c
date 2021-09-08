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

#include "neu_datatag_table.h"
#include "neu_log.h"
#include "neu_plugin.h"

#include "parser/neu_json_login.h"
#include "parser/neu_json_parser.h"

#include "adapter_handle.h"
#include "datatag_handle.h"
#include "group_config_handle.h"
#include "http.h"
#include "plugin_handle.h"

#include "handle.h"

static void ping(nng_aio *aio);
static void login(nng_aio *aio);
static void logout(nng_aio *aio);

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

struct neu_rest_handler api_handlers[] = {
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/ping",
        .value.handler = ping,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/login",
        .value.handler = login,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/logout",
        .value.handler = logout,
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
        .method        = NEU_REST_METHOD_PUT,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/node",
        .value.handler = handle_update_adapter,
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
        .method        = NEU_REST_METHOD_PUT,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/plugin",
        .value.handler = handle_update_plugin,
    },
    {
        .method        = NEU_REST_METHOD_DELETE,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/plugin",
        .value.handler = handle_del_plugin,
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

static void ping(nng_aio *aio)
{
    http_ok(aio, "{\"status\": \"OK\"}");
}

static void login(nng_aio *aio)
{
    REST_PROCESS_HTTP_REQUEST(aio, struct neu_parse_login_req,
                              { http_ok(aio, "{\"status\": \"ok\"}"); })
}

static void logout(nng_aio *aio)
{
    (void) aio;
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

neu_taggrp_config_t *neu_rest_find_config(neu_plugin_t *plugin,
                                          neu_node_id_t node_id,
                                          const char *  name)
{
    vector_t grp_configs = neu_system_get_group_configs(plugin, node_id);
    neu_taggrp_config_t *find_config = NULL;

    VECTOR_FOR_EACH(&grp_configs, iter)
    {
        neu_taggrp_config_t *config =
            *(neu_taggrp_config_t **) iterator_get(&iter);
        if (strncmp(neu_taggrp_cfg_get_name(config), name, strlen(name)) == 0) {
            find_config = config;
            break;
        }
    }

    vector_uninit(&grp_configs);

    return find_config;
}