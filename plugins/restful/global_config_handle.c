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
#include "parser/neu_json_global_config.h"
#include "parser/neu_json_group_config.h"
#include "parser/neu_json_node.h"
#include "parser/neu_json_tag.h"
#include "plugin.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "tag.h"
#include "utils/http.h"
#include "utils/set.h"
#include "utils/utarray.h"

#include "group_config_handle.h"

typedef enum {
    GET_GLOBAL,
    PUT_GLOBAL,
    GET_DRIVERS,
} context_type_e;

typedef enum {
    STATE_START,
    STATE_GET_APP,
    STATE_GET_DRIVER,
    STATE_GET_GROUP,
    STATE_GET_TAG,
    STATE_GET_SUBSCRIPTION,
    STATE_GET_APP_SETTING,
    STATE_GET_DRIVER_SETTING,
    STATE_DEL_APP,
    STATE_DEL_DRIVER,
    STATE_ADD_NODE,
    STATE_ADD_GROUP,
    STATE_ADD_TAG,
    STATE_ADD_SUBSCRIPTION,
    STATE_ADD_SETTING,
    STATE_END,
} context_state_e;

typedef struct {
    nng_aio *       aio;
    context_type_e  type;
    context_state_e state;
    int             error;
    union {
        // get
        struct {
            void *                  json;
            UT_array *              apps;
            UT_array *              drivers;
            UT_array *              groups;
            void *                  iter;
            neu_json_driver_array_t jdrivers; // get drivers
            char *                  names;    //
            neu_strset_t            filter;   //
        };
        // put
        struct {
            neu_json_global_config_req_t *req;
            UT_array *                    nodes;
            size_t                        idx;
        };
    };
} context_t;

static int get_nodes(context_t *ctx, neu_node_type_e type);
static int get_nodes_resp(context_t *ctx, neu_resp_get_node_t *nodes);
static int get_drivers_resp(context_t *ctx, neu_resp_get_node_t *nodes);
static int get_groups(context_t *ctx, int unused);
static int get_groups_resp(context_t *ctx, neu_resp_get_driver_group_t *groups);
static int get_driver_groups_resp(context_t *                  ctx,
                                  neu_resp_get_driver_group_t *groups);
static int get_tags(context_t *ctx, neu_resp_driver_group_info_t *info);
static int get_tags_resp(context_t *ctx, neu_resp_get_tag_t *tags);
static int get_driver_tags_resp(context_t *ctx, neu_resp_get_tag_t *tags);
static int get_subscriptions(context_t *ctx, neu_resp_node_info_t *info);
static int get_subscriptions_resp(context_t *                     ctx,
                                  neu_resp_get_subscribe_group_t *groups);
static int get_setting(context_t *ctx, neu_resp_node_info_t *info);
static int get_setting_resp(context_t *                  ctx,
                            neu_resp_get_node_setting_t *setting);
static int get_driver_setting_resp(context_t *                  ctx,
                                   neu_resp_get_node_setting_t *setting);

static int del_node(context_t *ctx, neu_resp_node_info_t *info);
static int add_node(context_t *ctx, neu_json_get_nodes_resp_node_t *req);
static int add_group(context_t *                             ctx,
                     neu_json_get_driver_group_resp_group_t *data);
static int add_tag(context_t *ctx, neu_json_add_tags_req_t *data);
static int add_subscription(context_t *ctx, neu_json_subscribe_req_t *data);
static int add_setting(context_t *ctx, neu_json_node_setting_req_t *data);

static inline bool is_static_node(const char *name, const char *plugin)
{
    return 0 == strcmp("monitor", name) && 0 == strcmp("Monitor", plugin);
}

static context_t *context_new(nng_aio *aio, context_type_e t)
{
    context_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->aio   = aio;
    ctx->type  = t;
    ctx->state = STATE_START;

    if (PUT_GLOBAL != ctx->type && !(ctx->json = neu_json_encode_new())) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

static void context_free(context_t *ctx)
{
    if (NULL == ctx) {
        return;
    }

    if (GET_GLOBAL == ctx->type) {
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
    } else if (PUT_GLOBAL == ctx->type) {
        if (ctx->nodes) {
            utarray_free(ctx->nodes);
        }
        neu_json_decode_global_config_req_free(ctx->req);
    } else {
        neu_json_encode_free(ctx->json);
        if (ctx->drivers) {
            utarray_free(ctx->drivers);
        }
        if (ctx->groups) {
            utarray_free(ctx->groups);
        }
        free(ctx->names);
        neu_strset_free(&ctx->filter);
        for (int i = 0; i < ctx->jdrivers.n_driver; ++i) {
            neu_json_driver_t *driver = &ctx->jdrivers.drivers[i];
            free(driver->node.setting);
            for (int j = 0; j < driver->gtags.len; ++j) {
                for (int k = 0; k < driver->gtags.gtags[j].n_tag; k++) {
                    neu_json_decode_tag_fini(&(driver->gtags.gtags[j].tags[k]));
                }
                free(driver->gtags.gtags[j].tags);
            }
            free(driver->gtags.gtags);
        }
        free(ctx->jdrivers.drivers);
    }

    nng_aio_set_input(ctx->aio, 3, NULL);
    free(ctx);
}

#define NEXT(ctx, action, ...)                     \
    {                                              \
        (ctx)->error = (action)(ctx, __VA_ARGS__); \
        if (ctx->error) {                          \
            (ctx)->state = STATE_END;              \
            break;                                 \
        }                                          \
    }

static void get_global_context_next(context_t *ctx, neu_reqresp_type_e type,
                                    void *data)
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
        ctx->state = STATE_GET_APP_SETTING;
        // fall through
    case STATE_GET_APP_SETTING:
        if (NULL == ctx->iter) {
            // generate empty array on fall through
            NEXT(ctx, get_setting_resp, NULL);
        } else if (NEU_RESP_ERROR != type) {
            NEXT(ctx, get_setting_resp, data);
        }
        // ignore error response message, keep going
        ctx->iter = utarray_next(ctx->apps, ctx->iter);
        if (ctx->iter) {
            NEXT(ctx, get_setting, ctx->iter);
            break;
        }
        ctx->state = STATE_GET_DRIVER_SETTING;
        // fall through
    case STATE_GET_DRIVER_SETTING:
        if (NULL == ctx->iter) {
            // generate empty array on fall through
            NEXT(ctx, get_setting_resp, NULL);
        } else if (NEU_RESP_ERROR != type) {
            NEXT(ctx, get_setting_resp, data);
        }
        ctx->iter = utarray_next(ctx->drivers, ctx->iter);
        if (ctx->iter) {
            NEXT(ctx, get_setting, ctx->iter);
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

static void put_global_context_next(context_t *ctx, neu_reqresp_type_e type,
                                    void *data)
{
    (void) type;

    switch (ctx->state) {
    case STATE_START:
        NEXT(ctx, get_nodes, NEU_NA_TYPE_APP);
        ctx->state = STATE_GET_APP;
        break;

    case STATE_GET_APP:
    case STATE_GET_DRIVER:
        ctx->nodes = ((neu_resp_get_node_t *) data)->nodes;
        ctx->idx   = 0;
        ctx->state =
            STATE_GET_APP == ctx->state ? STATE_DEL_APP : STATE_DEL_DRIVER;
        // fall through

    case STATE_DEL_APP:
    case STATE_DEL_DRIVER: {
        neu_resp_error_t *    err  = data;
        neu_resp_node_info_t *info = NULL;

        if (ctx->idx > 0 && 0 != err->error) {
            // received error response
            ctx->error = err->error;
            ctx->state = STATE_END;
            break;
        }

        // skip static nodes
        for (; ctx->idx < utarray_len(ctx->nodes); ++ctx->idx, info = NULL) {
            info = utarray_eltptr(ctx->nodes, ctx->idx);
            if (!is_static_node(info->node, info->plugin)) {
                break;
            }
        }

        if (info) {
            NEXT(ctx, del_node, info);
            ++ctx->idx;
            break;
        }

        utarray_free(ctx->nodes);
        ctx->nodes = NULL;

        if (STATE_DEL_APP == ctx->state) {
            NEXT(ctx, get_nodes, NEU_NA_TYPE_DRIVER);
            ctx->state = STATE_GET_DRIVER;
            break;
        }

        ctx->idx   = 0;
        ctx->state = STATE_ADD_NODE;
    }
        // fall through

    case STATE_ADD_NODE: {
        neu_resp_error_t *         err   = data;
        neu_json_get_nodes_resp_t *nodes = ctx->req->nodes;

        if (ctx->idx > 0 && 0 != err->error) {
            // received error response
            ctx->error = err->error;
            ctx->state = STATE_END;
            break;
        }

        // skip static nodes
        for (; ctx->idx < (size_t) nodes->n_node; ++ctx->idx) {
            if (!is_static_node(nodes->nodes[ctx->idx].name,
                                nodes->nodes[ctx->idx].plugin)) {
                break;
            }
        }

        if (ctx->idx < (size_t) nodes->n_node) {
            NEXT(ctx, add_node, &nodes->nodes[ctx->idx]);
            ++ctx->idx;
            break;
        }

        ctx->idx   = 0;
        ctx->state = STATE_ADD_GROUP;
    }
        // fall through

    case STATE_ADD_GROUP: {
        neu_resp_error_t *                err    = data;
        neu_json_get_driver_group_resp_t *groups = ctx->req->groups;

        if (ctx->idx > 0 && 0 != err->error) {
            // received error response
            ctx->error = err->error;
            ctx->state = STATE_END;
            break;
        }

        if (ctx->idx < (size_t) groups->n_group) {
            NEXT(ctx, add_group, &groups->groups[ctx->idx]);
            ++ctx->idx;
            break;
        }

        ctx->idx   = 0;
        ctx->state = STATE_ADD_TAG;
    }
        // fall through

    case STATE_ADD_TAG: {
        neu_resp_add_tag_t *               resp = data;
        neu_json_global_config_req_tags_t *tags = ctx->req->tags;

        if (ctx->idx > 0 && 0 != resp->error) {
            // received error response
            ctx->error = resp->error;
            ctx->state = STATE_END;
            break;
        }

        if (ctx->idx < (size_t) tags->n_tag) {
            NEXT(ctx, add_tag, &tags->tags[ctx->idx]);
            ++ctx->idx;
            break;
        }

        ctx->idx   = 0;
        ctx->state = STATE_ADD_SUBSCRIPTION;
    }
        // fall through

    case STATE_ADD_SUBSCRIPTION: {
        neu_resp_error_t *                          err = data;
        neu_json_global_config_req_subscriptions_t *subs =
            ctx->req->subscriptions;

        if (ctx->idx > 0 && 0 != err->error) {
            // received error response
            ctx->error = err->error;
            ctx->state = STATE_END;
            break;
        }

        if (ctx->idx < (size_t) subs->n_subscription) {
            NEXT(ctx, add_subscription, &subs->subscriptions[ctx->idx]);
            ++ctx->idx;
            break;
        }

        ctx->idx   = 0;
        ctx->state = STATE_ADD_SETTING;
    }
        // fall through

    case STATE_ADD_SETTING: {
        neu_resp_error_t *                     err      = data;
        neu_json_global_config_req_settings_t *settings = ctx->req->settings;

        if (ctx->idx > 0 && 0 != err->error) {
            // received error response
            ctx->error = err->error;
            ctx->state = STATE_END;
            break;
        }

        if (ctx->idx < (size_t) settings->n_setting) {
            NEXT(ctx, add_setting, &settings->settings[ctx->idx]);
            ++ctx->idx;
            break;
        }

        ctx->state = STATE_END;
        break;
    }

    default:
        ctx->error = NEU_ERR_EINTERNAL;
        ctx->state = STATE_END;
        break;
    }

    if (STATE_END == ctx->state) {
        NEU_JSON_RESPONSE_ERROR(ctx->error, {
            neu_http_response(ctx->aio, ctx->error, result_error);
        });
        context_free(ctx);
    }
}

static void get_drivers_context_next(context_t *ctx, neu_reqresp_type_e type,
                                     void *data)
{
    char *result = NULL;

    switch (ctx->state) {
    case STATE_START:
        NEXT(ctx, get_nodes, NEU_NA_TYPE_DRIVER);
        ctx->state = STATE_GET_DRIVER;
        break;
    case STATE_GET_DRIVER:
        NEXT(ctx, get_drivers_resp, data);
        ctx->iter  = NULL;
        ctx->state = STATE_GET_DRIVER_SETTING;
        // fall through
    case STATE_GET_DRIVER_SETTING:
        if (NULL != ctx->iter && NEU_RESP_ERROR != type) {
            NEXT(ctx, get_driver_setting_resp, data);
        }
        ctx->iter = utarray_next(ctx->drivers, ctx->iter);
        if (ctx->iter) {
            NEXT(ctx, get_setting, ctx->iter);
            break;
        }
        NEXT(ctx, get_groups, 0);
        ctx->state = STATE_GET_GROUP;
        break;
    case STATE_GET_GROUP:
        NEXT(ctx, get_driver_groups_resp, data);
        ctx->iter  = NULL;
        ctx->state = STATE_GET_TAG;
        // fall through
    case STATE_GET_TAG:
        if (NULL != ctx->iter && NEU_RESP_ERROR != type) {
            NEXT(ctx, get_driver_tags_resp, data);
        }
        // ignore error response message, keep going
        ctx->iter = utarray_next(ctx->groups, ctx->iter);
        if (ctx->iter) {
            NEXT(ctx, get_tags, ctx->iter);
            break;
        }
        if (0 != neu_json_encode_drivers_req(ctx->json, &ctx->jdrivers)) {
            ctx->error = NEU_ERR_EINTERNAL;
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

static void context_next(context_t *ctx, neu_reqresp_type_e type, void *data)
{
    if (GET_GLOBAL == ctx->type) {
        get_global_context_next(ctx, type, data);
    } else if (PUT_GLOBAL == ctx->type) {
        put_global_context_next(ctx, type, data);
    } else {
        get_drivers_context_next(ctx, type, data);
    }
}

static int get_nodes(context_t *ctx, neu_node_type_e type)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx             = ctx->aio,
        .type            = NEU_REQ_GET_NODE,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
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

static int get_drivers_resp(context_t *ctx, neu_resp_get_node_t *resp)
{
    int                     rv      = 0;
    neu_json_driver_array_t drivers = { 0 };
    UT_array *              nodes   = resp->nodes;

    if (ctx->filter) {
        utarray_new(nodes, &resp->nodes->icd);
        utarray_foreach(resp->nodes, neu_resp_node_info_t *, info)
        {
            if (neu_strset_test(&ctx->filter, info->node)) {
                utarray_push_back(nodes, info);
            }
        }
        utarray_free(resp->nodes);
    }

    drivers.n_driver = utarray_len(nodes);
    drivers.drivers  = calloc(drivers.n_driver, sizeof(drivers.drivers[0]));
    if (NULL == drivers.drivers) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    for (unsigned i = 0; i < utarray_len(nodes); ++i) {
        neu_resp_node_info_t *info     = utarray_eltptr(nodes, i);
        drivers.drivers[i].node.name   = info->node;
        drivers.drivers[i].node.plugin = info->plugin;
    }

end:
    ctx->drivers  = nodes;
    ctx->jdrivers = drivers;

    return rv;
}

static int get_groups(context_t *ctx, int unused)
{
    (void) unused;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx             = ctx->aio,
        .type            = NEU_REQ_GET_DRIVER_GROUP,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
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

static int get_driver_groups_resp(context_t *                  ctx,
                                  neu_resp_get_driver_group_t *resp)
{
    int       rv     = 0;
    UT_array *groups = resp->groups;

    if (ctx->filter) {
        utarray_new(groups, &resp->groups->icd);
        utarray_foreach(resp->groups, neu_resp_driver_group_info_t *, group)
        {
            if (neu_strset_test(&ctx->filter, group->driver)) {
                utarray_push_back(groups, group);
            }
        }
        utarray_free(resp->groups);
    }

    neu_resp_driver_group_info_t *group = utarray_front(groups);
    for (int i = 0; i < ctx->jdrivers.n_driver && group; ++i) {
        neu_json_driver_t *driver = &ctx->jdrivers.drivers[i];

        int                           len = 0;
        neu_resp_driver_group_info_t *g   = group;
        while (group && 0 == strcmp(driver->node.name, group->driver)) {
            group = utarray_next(groups, group);
            ++len;
        }

        driver->gtags.len   = len;
        driver->gtags.gtags = calloc(len, sizeof(driver->gtags.gtags[0]));
        if (NULL == driver->gtags.gtags) {
            rv = NEU_ERR_EINTERNAL;
            break;
        }

        for (int j = 0; j < len; ++j, ++g) {
            driver->gtags.gtags[j].group    = g->group;
            driver->gtags.gtags[j].interval = g->interval;
        }
    }

    ctx->groups = groups;
    return rv;
}

static int get_tags(context_t *ctx, neu_resp_driver_group_info_t *info)
{
    int           rv     = 0;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx             = ctx->aio,
        .type            = NEU_REQ_GET_TAG,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
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
    tags_res.tags  = calloc(tags_res.n_tag, sizeof(neu_json_tag_t));
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
        tags_res.tags[index].bias        = tag->bias;
        tags_res.tags[index].t           = NEU_JSON_UNDEFINE;
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

static int get_driver_tags_resp(context_t *ctx, neu_resp_get_tag_t *tags)
{
    int rv = 0;

    if (NULL == tags || 0 == utarray_len(tags->tags)) {
        // empty tags array, all done
        goto end;
    }

    neu_json_gtag_t *             gtag  = NULL;
    neu_resp_driver_group_info_t *group = ctx->iter;
    for (int i = 0; i < ctx->jdrivers.n_driver && NULL == gtag; ++i) {
        neu_json_driver_t *driver = &ctx->jdrivers.drivers[i];
        if (0 != strcmp(driver->node.name, group->driver)) {
            continue;
        }
        for (int j = 0; j < driver->gtags.len; ++j) {
            if (0 == strcmp(driver->gtags.gtags[j].group, group->group)) {
                gtag = &driver->gtags.gtags[j];
                break;
            }
        }
    }

    if (NULL == gtag) {
        goto end; // ignore
    }

    gtag->n_tag = utarray_len(tags->tags);
    gtag->tags  = calloc(gtag->n_tag, sizeof(neu_json_tag_t));
    if (NULL == gtag->tags) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    utarray_foreach(tags->tags, neu_datatag_t *, tag)
    {
        int index = utarray_eltidx(tags->tags, tag);

        gtag->tags[index].name        = tag->name;
        gtag->tags[index].address     = tag->address;
        gtag->tags[index].description = tag->description;
        gtag->tags[index].type        = tag->type;
        gtag->tags[index].attribute   = tag->attribute;
        gtag->tags[index].precision   = tag->precision;
        gtag->tags[index].decimal     = tag->decimal;
        gtag->tags[index].bias        = tag->bias;
        gtag->tags[index].t           = NEU_JSON_UNDEFINE;
        tag->name                     = NULL; // moved
        tag->address                  = NULL; // moved
        tag->description              = NULL; // moved
    }

end:
    if (tags && tags->tags) {
        utarray_free(tags->tags);
    }
    return rv;
}

static int get_subscriptions(context_t *ctx, neu_resp_node_info_t *info)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx             = ctx->aio,
        .type            = NEU_REQ_GET_SUBSCRIBE_GROUP,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
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

static int get_setting(context_t *ctx, neu_resp_node_info_t *info)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx             = ctx->aio,
        .type            = NEU_REQ_GET_NODE_SETTING,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    neu_req_get_node_setting_t cmd = { 0 };
    strcpy(cmd.node, info->node);

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

static int get_setting_resp(context_t *                  ctx,
                            neu_resp_get_node_setting_t *setting)
{
    int     rv          = 0;
    json_t *setting_obj = NULL;
    json_t *setting_arr = NULL;

    // allocate `settings` array if none
    if (NULL == (setting_arr = json_object_get(ctx->json, "settings")) &&
        (NULL == (setting_arr = json_array()) ||
         // tag array ownership moved
         0 != json_object_set_new(ctx->json, "settings", setting_arr))) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    if (NULL == setting) {
        // empty subscriptions array, all done
        goto end;
    }

    // accumulate setting object in `settings` array
    if (NULL == (setting_obj = json_object()) ||
        // setting object ownership moved
        0 != json_array_append_new(setting_arr, setting_obj)) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    neu_json_get_node_setting_resp_t resp = {
        .node    = setting->node,
        .setting = setting->setting,
    };

    if (0 != neu_json_encode_get_node_setting_resp(setting_obj, &resp)) {
        rv = NEU_ERR_EINTERNAL;
    }

end:
    if (setting) {
        free(setting->setting);
    }
    return rv;
}

static int get_driver_setting_resp(context_t *                  ctx,
                                   neu_resp_get_node_setting_t *setting)
{
    int i = utarray_eltidx(ctx->drivers, ctx->iter);
    if (i >= ctx->jdrivers.n_driver ||
        0 != strcmp(ctx->jdrivers.drivers[i].node.name, setting->node)) {
        if (setting) {
            free(setting->setting);
        }
        return NEU_ERR_NODE_NOT_EXIST;
    }

    ctx->jdrivers.drivers[i].node.setting = setting->setting;
    return 0;
}

static int del_node(context_t *ctx, neu_resp_node_info_t *info)
{
    neu_plugin_t *     plugin = neu_rest_get_plugin();
    neu_reqresp_head_t header = {
        .ctx             = ctx->aio,
        .type            = NEU_REQ_DEL_NODE,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    neu_req_del_node_t cmd = { 0 };
    strcpy(cmd.node, info->node);

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

static int add_node(context_t *ctx, neu_json_get_nodes_resp_node_t *req)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .type            = NEU_REQ_ADD_NODE,
        .ctx             = ctx->aio,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    neu_req_add_node_t cmd = { 0 };
    strcpy(cmd.node, req->name);
    strcpy(cmd.plugin, req->plugin);

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

static int add_group(context_t *                             ctx,
                     neu_json_get_driver_group_resp_group_t *data)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .type            = NEU_REQ_ADD_GROUP,
        .ctx             = ctx->aio,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    neu_req_add_group_t cmd = { 0 };
    strcpy(cmd.driver, data->driver);
    strcpy(cmd.group, data->group);
    cmd.interval = data->interval;

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

static int add_tag(context_t *ctx, neu_json_add_tags_req_t *data)
{
    int           ret    = 0;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .type            = NEU_REQ_ADD_TAG,
        .ctx             = ctx->aio,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    neu_req_add_tag_t cmd = { 0 };
    strcpy(cmd.driver, data->node);
    strcpy(cmd.group, data->group);
    cmd.n_tag = data->n_tag;
    cmd.tags  = calloc(data->n_tag, sizeof(neu_datatag_t));
    if (NULL == cmd.tags) {
        return NEU_ERR_EINTERNAL;
    }

    int i = 0;
    for (; i < data->n_tag; i++) {
        cmd.tags[i].attribute = data->tags[i].attribute;
        cmd.tags[i].type      = data->tags[i].type;
        cmd.tags[i].precision = data->tags[i].precision;
        cmd.tags[i].decimal   = data->tags[i].decimal;
        cmd.tags[i].bias      = data->tags[i].bias;
        cmd.tags[i].address   = strdup(data->tags[i].address);
        cmd.tags[i].name      = strdup(data->tags[i].name);
        cmd.tags[i].description =
            strdup(data->tags[i].description ? data->tags[i].description : "");

        if (NULL == cmd.tags[i].address || NULL == cmd.tags[i].name ||
            NULL == cmd.tags[i].description) {
            ret = NEU_ERR_EINTERNAL;
            goto error;
        }
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        ret = NEU_ERR_IS_BUSY;
        goto error;
    }

    return ret;

error:
    for (int j = 0; j < i && j < cmd.n_tag; ++j) {
        neu_tag_fini(&cmd.tags[j]);
    }
    free(cmd.tags);
    return ret;
}

static int add_subscription(context_t *ctx, neu_json_subscribe_req_t *data)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx             = ctx->aio,
        .type            = NEU_REQ_SUBSCRIBE_GROUP,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    neu_req_subscribe_t cmd = { 0 };
    strcpy(cmd.app, data->app);
    strcpy(cmd.driver, data->driver);
    strcpy(cmd.group, data->group);
    cmd.params   = data->params; // ownership moved
    data->params = NULL;

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

static int add_setting(context_t *ctx, neu_json_node_setting_req_t *data)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx             = ctx->aio,
        .type            = NEU_REQ_NODE_SETTING,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    neu_req_node_setting_t cmd = { 0 };
    strcpy(cmd.node, data->node);
    cmd.setting   = data->setting; // ownership moved
    data->setting = NULL;

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

void handle_get_global_config(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);

    context_t *ctx = context_new(aio, GET_GLOBAL);
    if (NULL == ctx) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
        return;
    }

    nng_aio_set_input(aio, 3, ctx);
    context_next(ctx, NEU_REQ_GET_NODE, NULL);
}

void handle_put_global_config(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_global_config_req_t, neu_json_decode_global_config_req, {
            context_t *ctx = context_new(aio, PUT_GLOBAL);
            if (NULL == ctx) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
                });
            } else {
                nng_aio_set_input(aio, 3, ctx);
                context_next(ctx, NEU_REQ_GET_NODE, NULL);
                ctx->req = req;
                req      = NULL;
            }
        });
}

void handle_get_drivers(nng_aio *aio)
{
    int rv = 0;

    NEU_VALIDATE_JWT(aio);

    context_t *ctx = context_new(aio, GET_DRIVERS);
    if (NULL == ctx) {
        rv = NEU_ERR_EINTERNAL;
        goto error;
    }

    size_t      len   = 0;
    const char *param = neu_http_get_param(aio, "name", &len);
    if (NULL != param) {
        char *names = NULL;

        len += 1; // for terminating null byte
        if (NULL == (names = calloc(len, 1))) {
            rv = NEU_ERR_EINTERNAL;
            goto error;
        }
        ctx->names = names;

        if (neu_url_decode(param, len, names, len) < 0) {
            rv = NEU_ERR_PARAM_IS_WRONG;
            goto error;
        }

        for (char *next = NULL; names; names = next) {
            // split names by ','
            if (NULL != (next = strchr(names, ','))) {
                // terminate each name with NULL byte, and move to next
                *next++ = '\0';
            }
            if (neu_strset_add(&ctx->filter, names) < 0) {
                rv = NEU_ERR_EINTERNAL;
                goto error;
            }
        }
    }

    (void) rv; // suppress cppcheck warning
    nng_aio_set_input(aio, 3, ctx);
    context_next(ctx, NEU_REQ_GET_NODE, NULL);
    return;

error:
    NEU_JSON_RESPONSE_ERROR(rv, { neu_http_response(aio, rv, result_error); });
    context_free(ctx);
}

void handle_global_config_resp(nng_aio *aio, neu_reqresp_type_e type,
                               void *data)
{
    context_t *ctx = nng_aio_get_input(aio, 3);
    context_next(ctx, type, data);
}
