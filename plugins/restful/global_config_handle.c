/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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

#include <stdlib.h>

#include <jansson.h>

#include "adapter.h"
#include "define.h"
#include "errcodes.h"
#include "parser/neu_json_group_config.h"
#include "parser/neu_json_node.h"
#include "plugin.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "utils/http.h"

#include "group_config_handle.h"

typedef enum {
    STATE_START,
    STATE_GET_APP,
    STATE_GET_DRIVER,
    STATE_END,
} context_state_e;

typedef struct {
    nng_aio *       aio;
    context_state_e state;
    int             error;
    void *          json;
    UT_array *      apps;
    UT_array *      drivers;
} context_t;

static int get_nodes(context_t *ctx, neu_node_type_e type);
static int get_nodes_resp(context_t *ctx, neu_resp_get_node_t *nodes);

static context_t *context_new(nng_aio *aio)
{
    context_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->aio   = aio;
    ctx->state = STATE_START;

    ctx->json = neu_json_encode_new();
    if (!ctx->json) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

static void context_free(context_t *ctx)
{
    if (ctx) {
        neu_json_encode_free(ctx->json);
        if (ctx->apps) {
            utarray_free(ctx->apps);
        }
        if (ctx->drivers) {
            utarray_free(ctx->drivers);
        }
        free(ctx);
    }
}

#define NEXT(ctx, action, ...)                     \
    {                                              \
        (ctx)->error = (action)(ctx, __VA_ARGS__); \
        if (ctx->error) {                          \
            (ctx)->state = STATE_END;              \
            break;                                 \
        }                                          \
    }

static void context_next(context_t *ctx, neu_reqresp_type_e type, void *data)
{
    char *result = NULL;

    switch (ctx->state) {
    case STATE_START:
        NEXT(ctx, get_nodes, NEU_NA_TYPE_APP);
        ctx->state = STATE_GET_APP;
        break;
    case STATE_GET_APP:
        NEXT(ctx, get_nodes_resp, data);
        NEXT(ctx, get_nodes, NEU_NA_TYPE_DRIVER);
        ctx->state = STATE_GET_DRIVER;
        break;
    case STATE_GET_DRIVER:
        NEXT(ctx, get_nodes_resp, data);
        ctx->state = STATE_END;
        break;
    default:
        ctx->error = NEU_ERR_EINTERNAL;
        ctx->state = STATE_END;
        break;
    }

    if (STATE_END == ctx->state) {
        if (ctx->error) {
            NEU_JSON_RESPONSE_ERROR(ctx->error, {
                neu_http_response(ctx->aio, ctx->error, result_error);
            });
        } else {
            neu_json_encode(ctx->json, &result);
            neu_http_ok(ctx->aio, result);
            free(result);
        }
        context_free(ctx);
    }
}

static int get_nodes(context_t *ctx, neu_node_type_e type)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx  = ctx->aio,
        .type = NEU_REQ_GET_NODE,
    };

    neu_req_get_node_t cmd = {
        .type = type,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

static int get_nodes_resp(context_t *ctx, neu_resp_get_node_t *nodes)
{
    int                       rv        = 0;
    neu_json_get_nodes_resp_t nodes_res = { 0 };

    nodes_res.n_node = utarray_len(nodes->nodes);
    nodes_res.nodes =
        calloc(nodes_res.n_node, sizeof(neu_json_get_nodes_resp_node_t));
    if (NULL == nodes_res.nodes) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    utarray_foreach(nodes->nodes, neu_resp_node_info_t *, info)
    {
        int index = utarray_eltidx(nodes->nodes, info);

        nodes_res.nodes[index].name   = info->node;
        nodes_res.nodes[index].plugin = info->plugin;
    }

    if (0 != neu_json_encode_get_nodes_resp(ctx->json, &nodes_res)) {
        rv = NEU_ERR_EINTERNAL;
    }

end:
    if (STATE_GET_APP == ctx->state) {
        ctx->apps = nodes->nodes;
    } else {
        ctx->drivers = nodes->nodes;
    }

    free(nodes_res.nodes);
    return rv;
}

void handle_get_global_config(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);

    context_t *ctx = context_new(aio);
    if (NULL == ctx) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
        return;
    }

    nng_aio_set_input(aio, 3, ctx);
    context_next(ctx, NEU_REQ_GET_NODE, NULL);
}

void handle_global_config_resp(nng_aio *aio, neu_reqresp_type_e type,
                               void *data)
{
    context_t *ctx = nng_aio_get_input(aio, 3);
    context_next(ctx, type, data);
}
