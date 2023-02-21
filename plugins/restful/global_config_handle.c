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
#include "parser/neu_json_tag.h"
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
    STATE_GET_GROUP,
    STATE_GET_TAG,
    STATE_GET_SUBSCRIPTION,
    STATE_END,
} context_state_e;

typedef struct {
    nng_aio *       aio;
    context_state_e state;
    int             error;
    void *          json;
    UT_array *      apps;
    UT_array *      drivers;
    UT_array *      groups;
    void *          iter;
} context_t;

static int get_nodes(context_t *ctx, neu_node_type_e type);
static int get_nodes_resp(context_t *ctx, neu_resp_get_node_t *nodes);
static int get_groups(context_t *ctx, int unused);
static int get_groups_resp(context_t *ctx, neu_resp_get_driver_group_t *groups);
static int get_tags(context_t *ctx, neu_resp_driver_group_info_t *info);
static int get_tags_resp(context_t *ctx, neu_resp_get_tag_t *tags);
static int get_subscriptions(context_t *ctx, neu_resp_node_info_t *info);
static int get_subscriptions_resp(context_t *                     ctx,
                                  neu_resp_get_subscribe_group_t *groups);

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
        if (ctx->groups) {
            utarray_free(ctx->groups);
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
        NEXT(ctx, get_groups, 0);
        ctx->state = STATE_GET_GROUP;
        break;
    case STATE_GET_GROUP:
        NEXT(ctx, get_groups_resp, data);
        ctx->iter  = NULL;
        ctx->state = STATE_GET_TAG;
        // fall through
    case STATE_GET_TAG:
        if (NULL == ctx->iter) {
            // generate empty array on fall through
            NEXT(ctx, get_tags_resp, NULL);
        } else if (NEU_RESP_ERROR != type) {
            NEXT(ctx, get_tags_resp, data);
        }
        // ignore error response message, keep going
        ctx->iter = utarray_next(ctx->groups, ctx->iter);
        if (ctx->iter) {
            NEXT(ctx, get_tags, ctx->iter);
            break;
        }
        ctx->state = STATE_GET_SUBSCRIPTION;
        // fall through
    case STATE_GET_SUBSCRIPTION:
        // generate empty array on fall through
        NEXT(ctx, get_subscriptions_resp, ctx->iter ? data : NULL);
        ctx->iter = utarray_next(ctx->apps, ctx->iter);
        if (ctx->iter) {
            NEXT(ctx, get_subscriptions, ctx->iter);
            break;
        }
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

static int get_groups(context_t *ctx, int unused)
{
    (void) unused;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx  = ctx->aio,
        .type = NEU_REQ_GET_DRIVER_GROUP,
    };
    neu_req_get_group_t cmd = { 0 };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

static int get_groups_resp(context_t *ctx, neu_resp_get_driver_group_t *groups)
{
    int                              rv          = 0;
    neu_json_get_driver_group_resp_t gconfig_res = { 0 };

    gconfig_res.n_group = utarray_len(groups->groups);
    gconfig_res.groups  = calloc(gconfig_res.n_group,
                                sizeof(neu_json_get_driver_group_resp_group_t));
    if (NULL == gconfig_res.groups) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    utarray_foreach(groups->groups, neu_resp_driver_group_info_t *, group)
    {
        int index = utarray_eltidx(groups->groups, group);
        gconfig_res.groups[index].driver    = group->driver;
        gconfig_res.groups[index].group     = group->group;
        gconfig_res.groups[index].interval  = group->interval;
        gconfig_res.groups[index].tag_count = group->tag_count;
    }

    if (0 != neu_json_encode_get_driver_group_resp(ctx->json, &gconfig_res)) {
        rv = NEU_ERR_EINTERNAL;
    }

end:
    free(gconfig_res.groups);
    ctx->groups = groups->groups;
    return rv;
}

static int get_tags(context_t *ctx, neu_resp_driver_group_info_t *info)
{
    int           rv     = 0;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx  = ctx->aio,
        .type = NEU_REQ_GET_TAG,
    };

    neu_req_get_tag_t cmd = { 0 };
    strcpy(cmd.driver, info->driver);
    strcpy(cmd.group, info->group);

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        rv = NEU_ERR_IS_BUSY;
    }

    return rv;
}

static int get_tags_resp(context_t *ctx, neu_resp_get_tag_t *tags)
{
    int                      rv       = 0;
    neu_json_get_tags_resp_t tags_res = { 0 };
    json_t *                 tag_obj  = NULL;
    void *                   tag_arr  = NULL;

    // allocate `tags` array if none
    if (NULL == (tag_arr = json_object_get(ctx->json, "tags")) &&
        (NULL == (tag_arr = json_array()) ||
         // tag array ownership moved
         0 != json_object_set_new(ctx->json, "tags", tag_arr))) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    if (NULL == tags || 0 == utarray_len(tags->tags)) {
        // empty tags array, all done
        goto end;
    }

    tags_res.n_tag = utarray_len(tags->tags);
    tags_res.tags =
        calloc(tags_res.n_tag, sizeof(neu_json_get_tags_resp_tag_t));
    if (NULL == tags_res.tags) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    utarray_foreach(tags->tags, neu_datatag_t *, tag)
    {
        int index = utarray_eltidx(tags->tags, tag);

        tags_res.tags[index].name        = tag->name;
        tags_res.tags[index].address     = tag->address;
        tags_res.tags[index].description = tag->description;
        tags_res.tags[index].type        = tag->type;
        tags_res.tags[index].attribute   = tag->attribute;
        tags_res.tags[index].precision   = tag->precision;
        tags_res.tags[index].decimal     = tag->decimal;
    }

    // accumulate tag object in `tags` array
    if (NULL == (tag_obj = json_object()) ||
        // tag object ownership moved
        0 != json_array_append_new(tag_arr, tag_obj)) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    // encode `tags` data
    if (0 != neu_json_encode_get_tags_resp(tag_obj, &tags_res)) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    neu_resp_driver_group_info_t *info = ctx->iter;
    // encode `driver` and `group`
    if (0 !=
            json_object_set_new(tag_obj, "driver", json_string(info->driver)) ||
        0 != json_object_set_new(tag_obj, "group", json_string(info->group))) {
        rv = NEU_ERR_EINTERNAL;
    }

end:
    free(tags_res.tags);
    if (tags && tags->tags) {
        utarray_free(tags->tags);
    }
    return rv;
}

static int get_subscriptions(context_t *ctx, neu_resp_node_info_t *info)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx  = ctx->aio,
        .type = NEU_REQ_GET_SUBSCRIBE_GROUP,
    };

    neu_req_get_subscribe_group_t cmd = { 0 };
    strcpy(cmd.app, info->node);

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

static int get_subscriptions_resp(context_t *                     ctx,
                                  neu_resp_get_subscribe_group_t *groups)
{
    int                           rv              = 0;
    neu_json_get_subscribe_resp_t sub_grp_configs = { 0 };
    json_t *                      sub_obj         = NULL;
    json_t *                      sub_arr         = NULL;

    // allocate `subscriptions` array if none
    if (NULL == (sub_arr = json_object_get(ctx->json, "subscriptions")) &&
        (NULL == (sub_arr = json_array()) ||
         // tag array ownership moved
         0 != json_object_set_new(ctx->json, "subscriptions", sub_arr))) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    if (NULL == groups || 0 == utarray_len(groups->groups)) {
        // empty subscriptions array, all done
        goto end;
    }

    sub_grp_configs.n_group = utarray_len(groups->groups);
    sub_grp_configs.groups  = calloc(
        sub_grp_configs.n_group, sizeof(neu_json_get_subscribe_resp_group_t));
    if (NULL == sub_grp_configs.groups) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    utarray_foreach(groups->groups, neu_resp_subscribe_info_t *, group)
    {
        int index = utarray_eltidx(groups->groups, group);
        sub_grp_configs.groups[index].driver = group->driver;
        sub_grp_configs.groups[index].group  = group->group;
        sub_grp_configs.groups[index].params = group->params;
    }

    // accumulate subscription object in `subscriptions` array
    if (NULL == (sub_obj = json_object()) ||
        // subscription object ownership moved
        0 != json_array_append_new(sub_arr, sub_obj)) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    if (0 != neu_json_encode_get_subscribe_resp(sub_obj, &sub_grp_configs)) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    neu_resp_node_info_t *info = ctx->iter;
    if (0 != json_object_set_new(sub_obj, "app", json_string(info->node))) {
        rv = NEU_ERR_EINTERNAL;
    }

end:
    free(sub_grp_configs.groups);
    if (groups && groups->groups) {
        utarray_free(groups->groups);
    }
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
