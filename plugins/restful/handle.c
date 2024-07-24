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
#include <stdint.h>
#include <stdlib.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "plugin.h"

#include "adapter_handle.h"
#include "datatag_handle.h"
#include "global_config_handle.h"
#include "group_config_handle.h"
#include "log_handle.h"
#include "metric_handle.h"
#include "ndriver_handle.h"
#include "normal_handle.h"
#include "plugin_handle.h"
#include "rw_handle.h"
#include "template_handle.h"
#include "utils/http.h"
#include "version_handle.h"

#include "handle.h"

struct neu_rest_handle_ctx {
    void *plugin;
};

static struct neu_rest_handle_ctx *rest_ctx = NULL;

static struct neu_http_handler cors_handler[] = {
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
        .url = "/api/v2/gtags",
    },
    {
        .url = "/api/v2/group",
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
        .url = "/api/v2/read/paginate",
    },
    {
        .url = "/api/v2/write",
    },
    {
        .url = "/api/v2/write/tags",
    },
    {
        .url = "/api/v2/write/gtags",
    },
    {
        .url = "/api/v2/subscribe",
    },
    {
        .url = "/api/v2/subscribes",
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
        .url = "/api/v2/version",
    },
    {
        .url = "/api/v2/password",
    },
    {
        .url = "/api/v2/log/level",
    },
    {
        .url = "/api/v2/global/config",
    },
    {
        .url = "/api/v2/template",
    },
    {
        .url = "/api/v2/template/group",
    },
    {
        .url = "/api/v2/template/tag",
    },
    {
        .url = "/api/v2/template/inst",
    },
    {
        .url = "/api/v2/template/instances",
    },
    {
        .url = "/api/v2/ndriver/map",
    },
    {
        .url = "/api/v2/ndriver/tag/param",
    },
    {
        .url = "/api/v2/ndriver/tag/info",
    },
    {
        .url = "/api/v2/ndriver/tag",
    },
    {
        .url = "/api/v2/metrics",
    },
};

static struct neu_http_handler rest_handlers[] = {
    {
        .method     = NEU_HTTP_METHOD_GET,
        .type       = NEU_HTTP_HANDLER_DIRECTORY,
        .url        = "/web",
        .value.path = "./dist",
    },
    {
        .method     = NEU_HTTP_METHOD_UNDEFINE,
        .type       = NEU_HTTP_HANDLER_REDIRECT,
        .url        = "/",
        .value.path = "/web",
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/ping",
        .value.handler = handle_ping,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/login",
        .value.handler = handle_login,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/password",
        .value.handler = handle_password,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = handle_add_tags,

    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/gtags",
        .value.handler = handle_add_gtags,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = handle_update_tags,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = handle_get_tags,
    },
    {
        .method        = NEU_HTTP_METHOD_DELETE,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = handle_del_tags,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/group",
        .value.handler = handle_add_group_config,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/group",
        .value.handler = handle_update_group,
    },
    {
        .method        = NEU_HTTP_METHOD_DELETE,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/group",
        .value.handler = handle_del_group_config,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/group",
        .value.handler = handle_get_group_config,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/node",
        .value.handler = handle_add_adapter,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/node",
        .value.handler = handle_update_adapter,
    },
    {
        .method        = NEU_HTTP_METHOD_DELETE,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/node",
        .value.handler = handle_del_adapter,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/node",
        .value.handler = handle_get_adapter,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/plugin",
        .value.handler = handle_add_plugin,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/plugin",
        .value.handler = handle_get_plugin,
    },
    {
        .method        = NEU_HTTP_METHOD_DELETE,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/plugin",
        .value.handler = handle_del_plugin,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/plugin",
        .value.handler = handle_update_plugin,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/read",
        .value.handler = handle_read,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/read/paginate",
        .value.handler = handle_read_paginate,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/write",
        .value.handler = handle_write,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/write/tags",
        .value.handler = handle_write_tags,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/write/gtags",
        .value.handler = handle_write_gtags,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/subscribe",
        .value.handler = handle_grp_subscribe,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/subscribe",
        .value.handler = handle_grp_update_subscribe,
    },
    {
        .method        = NEU_HTTP_METHOD_DELETE,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/subscribe",
        .value.handler = handle_grp_unsubscribe,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/subscribe",
        .value.handler = handle_grp_get_subscribe,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/subscribes",
        .value.handler = handle_grp_subscribes,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/schema",
        .value.handler = handle_get_plugin_schema,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/node/setting",
        .value.handler = handle_set_node_setting,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/node/setting",
        .value.handler = handle_get_node_setting,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/node/ctl",
        .value.handler = handle_node_ctl,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/node/state",
        .value.handler = handle_get_node_state,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/log/level",
        .value.handler = handle_log_level,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/version",
        .value.handler = handle_get_version,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/global/config",
        .value.handler = handle_get_global_config,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/global/config",
        .value.handler = handle_put_global_config,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template",
        .value.handler = handle_add_template,
    },
    {
        .method        = NEU_HTTP_METHOD_DELETE,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template",
        .value.handler = handle_del_template,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template",
        .value.handler = handle_get_template,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template/tag",
        .value.handler = handle_add_template_tags,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template/tag",
        .value.handler = handle_update_template_tags,
    },
    {
        .method        = NEU_HTTP_METHOD_DELETE,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template/tag",
        .value.handler = handle_del_template_tags,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template/tag",
        .value.handler = handle_get_template_tags,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template/inst",
        .value.handler = handle_instantiate_template,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template/instances",
        .value.handler = handle_instantiate_templates,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template/group",
        .value.handler = handle_add_template_group,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template/group",
        .value.handler = handle_update_template_group,
    },
    {
        .method        = NEU_HTTP_METHOD_DELETE,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template/group",
        .value.handler = handle_del_template_group,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/template/group",
        .value.handler = handle_get_template_group,
    },
    {
        .method        = NEU_HTTP_METHOD_POST,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/ndriver/map",
        .value.handler = handle_add_ndriver_map,
    },
    {
        .method        = NEU_HTTP_METHOD_DELETE,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/ndriver/map",
        .value.handler = handle_del_ndriver_map,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/ndriver/map",
        .value.handler = handle_get_ndriver_maps,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/ndriver/tag/param",
        .value.handler = handle_put_ndriver_tag_param,
    },
    {
        .method        = NEU_HTTP_METHOD_PUT,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/ndriver/tag/info",
        .value.handler = handle_put_ndriver_tag_info,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/ndriver/tag",
        .value.handler = handle_get_ndriver_tags,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/metrics",
        .value.handler = handle_get_metric,
    },
    {
        .method        = NEU_HTTP_METHOD_GET,
        .type          = NEU_HTTP_HANDLER_FUNCTION,
        .url           = "/api/v2/status",
        .value.handler = handle_status,
    },
};

void neu_rest_handler(const struct neu_http_handler **handlers, uint32_t *size)
{
    *handlers = rest_handlers;
    *size     = sizeof(rest_handlers) / sizeof(struct neu_http_handler);
}

void neu_rest_api_cors_handler(const struct neu_http_handler **handlers,
                               uint32_t *                      size)
{
    *handlers = cors_handler;
    *size     = sizeof(cors_handler) / sizeof(struct neu_http_handler);

    for (uint32_t i = 0; i < *size; i++) {
        cors_handler[i].method        = NEU_HTTP_METHOD_OPTIONS;
        cors_handler[i].type          = NEU_HTTP_HANDLER_FUNCTION;
        cors_handler[i].value.handler = neu_http_handle_cors;
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
