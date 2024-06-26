/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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
#include <string.h>

#include <jansson.h>

#include "define.h"
#include "metrics.h"
#include "msg.h"
#include "tag.h"
#include "type.h"
#include "utils/log.h"
#include "utils/uthash.h"

#include "xinao.h"

static inline int json_value_to_tag_value(neu_dvalue_t *          value,
                                          neu_json_type_e         t,
                                          const neu_json_value_u *jval)
{
    switch (t) {
    case NEU_JSON_INT:
        value->type      = NEU_TYPE_INT64;
        value->value.i64 = jval->val_int;
        break;
    case NEU_JSON_STR:
        value->type = NEU_TYPE_STRING;
        strcpy(value->value.str, jval->val_str);
        break;
    case NEU_JSON_BYTES:
        value->type               = NEU_TYPE_BYTES;
        value->value.bytes.length = jval->val_bytes.length;
        memcpy(value->value.bytes.bytes, jval->val_bytes.bytes,
               jval->val_bytes.length);
        break;
    case NEU_JSON_DOUBLE:
        value->type      = NEU_TYPE_DOUBLE;
        value->value.d64 = jval->val_double;
        break;
    case NEU_JSON_BOOL:
        value->type          = NEU_TYPE_BOOL;
        value->value.boolean = jval->val_bool;
        break;
    default:
        return -1;
    }

    return 0;
}

int neu_json_decode_xinao_write_tag_req_json(neu_plugin_t *    plugin,
                                             void *            json_root,
                                             xinao_wtag_ctx_t *ctx)
{
    json_t *data = json_object_get(json_root, "data");
    if (NULL == data || !json_is_object(data)) {
        plog_error(plugin, "no `data` object");
        return -1;
    }

    json_t *node_name = json_object_get(data, "sysId");
    if (NULL == node_name || !json_is_string(node_name)) {
        plog_error(plugin, "data no valid key `sysId`");
        return -1;
    }

    json_t *group_name = json_object_get(data, "dev");
    if (NULL == group_name || !json_is_string(group_name)) {
        plog_error(plugin, "data no valid key `dev`");
        return -1;
    }

    json_t *tag = json_object_get(data, "m");

    neu_json_elem_t elem = {
        .name = "v",
        .t    = NEU_JSON_VALUE,
    };

    if (neu_json_decode_by_json(data, 1, &elem)) {
        plog_error(plugin, "sysId:%s dev:%s m:%s decode v fail",
                   json_string_value(node_name), json_string_value(group_name),
                   json_string_value(tag));
        return -1;
    }

    int ret = json_value_to_tag_value(&ctx->wtag.value, elem.t, &elem.v);
    if (NEU_JSON_STR == elem.t) {
        free(elem.v.val_str);
    }
    if (0 != ret) {
        return -1;
    }

    ctx->type        = NEU_REQ_WRITE_TAG;
    ctx->wtag.driver = strdup(json_string_value(node_name));
    ctx->wtag.group  = strdup(json_string_value(group_name));
    ctx->wtag.tag    = strdup(json_string_value(tag));

    if (NULL == ctx->wtag.driver || NULL == ctx->wtag.group ||
        NULL == ctx->wtag.tag) {
        free(ctx->wtag.driver);
        free(ctx->wtag.group);
        free(ctx->wtag.tag);
        return -1;
    }

    return 0;
}

int neu_json_decode_xinao_write_batch_req_json(neu_plugin_t *     plugin,
                                               void *             json_root,
                                               xinao_batch_ctx_t *ctx)
{
    json_t *cmd = json_object_get(json_root, "cmd");
    if (NULL == cmd || !json_is_array(cmd)) {
        plog_error(plugin, "no `cmd` array");
        return -1;
    }

    int n_cmd = json_array_size(cmd);
    if (0 == n_cmd) {
        plog_error(plugin, "empty `cmd` array");
        return -1;
    }

    index_map_t        map   = NULL;
    neu_write_batch_t *batch = NULL;

    // quick check
    for (int i = 0; i < n_cmd; ++i) {
        json_t *a_cmd = json_array_get(cmd, i);

        json_t *node_name = json_object_get(a_cmd, "sysId");
        if (NULL == node_name || !json_is_string(node_name)) {
            plog_error(plugin, "cmd[%d] no valid key `sysId`", i);
            goto error;
        }

        json_t *group_name = json_object_get(a_cmd, "dev");
        if (NULL == group_name || !json_is_string(group_name)) {
            plog_error(plugin, "cmd[%d] no valid key `dev`", i);
            goto error;
        }

        json_t *tags = json_object_get(a_cmd, "d");
        if (NULL == tags || !json_is_array(tags)) {
            plog_error(plugin, "cmd[%d] no `d` array", i);
            goto error;
        }

        int n_tag = json_array_size(tags);
        if (0 == n_tag) {
            plog_error(plugin, "cmd[%d] empty `d` array", i);
            goto error;
        }

        for (int j = 0; j < n_tag; ++j) {
            json_t *tag_value = json_array_get(tags, j);
            json_t *tag       = json_object_get(tag_value, "m");
            json_t *value     = json_object_get(tag_value, "v");

            if (NULL == tag || !json_is_string(tag)) {
                plog_error(plugin, "cmd[%d].d[%d] no valid key `m`", i, j);
                goto error;
            }

            if (NULL == value || json_is_object(value)) {
                plog_error(plugin, "cmd[%d].d[%d] no valid key `v`", i, j);
                goto error;
            }
        }

        if (0 !=
            index_map_add(&map, json_string_value(node_name),
                          json_string_value(group_name))) {
            plog_error(plugin, "cmd[%d] {sysId:%s, dev:%s} duplicate", i,
                       json_string_value(node_name),
                       json_string_value(group_name));
            goto error;
        }
    }

    batch =
        calloc(1, sizeof(*batch) + HASH_COUNT(map) * sizeof(batch->drivers[0]));
    if (NULL == batch) {
        return -1;
    }

    batch->n_driver = HASH_COUNT(map);

    for (int i = 0; i < n_cmd; ++i) {
        json_t *a_cmd      = json_array_get(cmd, i);
        json_t *node_name  = json_object_get(a_cmd, "sysId");
        json_t *group_name = json_object_get(a_cmd, "dev");

        node_index_t * n = NULL;
        group_index_t *g = NULL;
        HASH_FIND_STR(map, json_string_value(node_name), n);
        HASH_FIND_STR(n->grp_indices, json_string_value(group_name), g);

        neu_write_batch_driver_t *batch_driver = &batch->drivers[n->ind];
        if (NULL == batch_driver->driver) {
            batch_driver->driver = strdup(json_string_value(node_name));
            if (NULL == batch_driver->driver) {
                goto error;
            }
        }

        if (NULL == batch_driver->groups) {
            batch_driver->n_group = HASH_COUNT(g);
            batch_driver->groups =
                calloc(HASH_COUNT(g), sizeof(batch_driver->groups[0]));
            if (NULL == batch_driver->groups) {
                goto error;
            }
        }

        neu_write_batch_group_t *batch_group = &batch_driver->groups[g->ind];
        batch_group->group = strdup(json_string_value(group_name));
        if (NULL == batch_group->group) {
            goto error;
        }

        json_t *tags  = json_object_get(a_cmd, "d");
        int     n_tag = json_array_size(tags);

        batch_group->tags = calloc(n_tag, sizeof(batch_group->tags[0]));
        if (NULL == batch_group->tags) {
            goto error;
        }

        batch_group->n_tag = n_tag;

        for (int j = 0; j < n_tag; ++j) {
            json_t *tag_value = json_array_get(tags, j);
            json_t *tag       = json_object_get(tag_value, "m");

            batch_group->tags[j].name = strdup(json_string_value(tag));
            if (NULL == batch_group->tags[j].name) {
                goto error;
            }

            neu_json_elem_t elem = {
                .name = "v",
                .t    = NEU_JSON_VALUE,
            };

            int ret = neu_json_decode_by_json(tag_value, 1, &elem);
            if (0 != ret) {
                plog_error(plugin, "sysId:%s dev:%s m:%s decode v fail",
                           batch_driver->driver, batch_group->group,
                           batch_group->tags[j].name);
                goto error;
            }

            ret = json_value_to_tag_value(&batch_group->tags[j].value, elem.t,
                                          &elem.v);
            if (NEU_JSON_STR == elem.t) {
                free(elem.v.val_str);
            }
            if (0 != ret) {
                goto error;
            }
        }
    }

    ctx->type        = NEU_REQ_WRITE_BATCH;
    ctx->batch.batch = batch;
    ctx->map         = map;
    return 0;

error:
    index_map_free(&map);
    neu_write_batch_free(batch);
    free(batch);
    return -1;
}

int neu_json_decode_xinao_ctx_json(neu_plugin_t *plugin, void *json_root,
                                   xinao_ctx_t **ctx_p)
{
    json_t *ver  = json_object_get(json_root, "ver");
    json_t *pKey = json_object_get(json_root, "pKey");
    json_t *sn   = json_object_get(json_root, "sn");
    json_t *type = json_object_get(json_root, "type");
    json_t *seq  = json_object_get(json_root, "seq");
    json_t *ts   = json_object_get(json_root, "ts");

    if (NULL == ver || !json_is_string(ver)) {
        plog_error(plugin, "no valid key `ver`");
        return -1;
    }

    if (NULL == pKey || !json_is_string(pKey)) {
        plog_error(plugin, "no valid key `pKey`");
        return -1;
    }

    if (NULL == sn || !json_is_string(sn)) {
        plog_error(plugin, "no valid key `sn`");
        return -1;
    }

    if (NULL == seq || !json_is_integer(seq)) {
        plog_error(plugin, "no valid key `seq`");
        return -1;
    }

    if (NULL == ts || !json_is_integer(ts)) {
        plog_error(plugin, "no valid key `ts`");
        return -1;
    }

    if (NULL == type || !json_is_string(type)) {
        plog_error(plugin, "no valid key `type`");
        return -1;
    }

    int ret = -1;
    if (0 == strcmp("algset", json_string_value(type))) {
        xinao_batch_ctx_t *batch_ctx = calloc(1, sizeof(*batch_ctx));
        if (NULL != batch_ctx) {
            ret = neu_json_decode_xinao_write_batch_req_json(plugin, json_root,
                                                             batch_ctx);
            if (0 == ret) {
                *ctx_p = (xinao_ctx_t *) batch_ctx;
            } else {
                plog_error(plugin, "decode write batch json fail");
                free(batch_ctx);
            }
        }
    } else if (0 == strcmp("cmd/set", json_string_value(type))) {
        xinao_wtag_ctx_t *wtag_ctx = calloc(1, sizeof(*wtag_ctx));
        if (NULL != wtag_ctx) {
            ret = neu_json_decode_xinao_write_tag_req_json(plugin, json_root,
                                                           wtag_ctx);
            if (0 == ret) {
                *ctx_p = (xinao_ctx_t *) wtag_ctx;
            } else {
                plog_error(plugin, "decode write tag json fail");
                free(wtag_ctx);
            }
        }
    } else {
        plog_error(plugin, "unknown key `type`:%s", json_string_value(type));
    }

    return ret;
}

xinao_ctx_t *xinao_ctx_from_json_buf(neu_plugin_t *plugin, char *buf,
                                     size_t len)
{
    void *root = neu_json_decode_newb(buf, len);
    if (NULL == root) {
        plog_error(plugin, "invalid batch req json");
        return NULL;
    }

    xinao_ctx_t *ctx = NULL;
    if (0 != neu_json_decode_xinao_ctx_json(plugin, root, &ctx)) {
        plog_error(plugin, "decode write req json fail");
        neu_json_decode_free(root);
        return NULL;
    }

    ctx->root = root;
    return ctx;
}

char *xinao_ctx_gen_wtag_resp_json(xinao_wtag_ctx_t *ctx,
                                   neu_resp_error_t *resp)
{
    json_object_set_new(ctx->root, "ts", json_integer(global_timestamp));
    json_object_set_new(ctx->root, "type", json_string("cmd/set/cack"));

    json_t *data = json_object_get(ctx->root, "data");

    int  valid    = true;
    char error[6] = { 0 };
    if (0 != resp->error) {
        valid = false;
        snprintf(error, sizeof(error), "%d", resp->error);
    }

    json_object_set_new(data, "valid", json_integer(valid));
    json_object_set_new(data, "remark", json_string(error));

    char *result = json_dumps(ctx->root, 0);
    return result;
}

char *xinao_ctx_gen_batch_resp_json(xinao_batch_ctx_t *ctx)
{
    json_t *cmd   = json_object_get(ctx->root, "cmd");
    int     n_cmd = json_array_size(cmd);

    // rename to `data`
    json_incref(cmd);
    json_object_del(ctx->root, "cmd");
    json_object_set_new(ctx->root, "data", cmd);

    json_object_set_new(ctx->root, "ts", json_integer(global_timestamp));
    json_object_set_new(ctx->root, "type", json_string("algset/cack"));

    for (int i = 0; i < n_cmd; ++i) {
        json_t *a_cmd      = json_array_get(cmd, i);
        json_t *node_name  = json_object_get(a_cmd, "sysId");
        json_t *group_name = json_object_get(a_cmd, "dev");

        node_index_t * n = NULL;
        group_index_t *g = NULL;
        HASH_FIND_STR(ctx->map, json_string_value(node_name), n);
        HASH_FIND_STR(n->grp_indices, json_string_value(group_name), g);
        neu_write_batch_driver_t *batch_driver =
            &ctx->batch.batch->drivers[n->ind];
        neu_write_batch_group_t *batch_group = &batch_driver->groups[g->ind];

        json_t *tags     = json_object_get(a_cmd, "d");
        int     n_tag    = json_array_size(tags);
        char    error[6] = { 0 };

        for (int j = 0; j < n_tag; ++j) {
            json_t *tag_value = json_array_get(tags, j);
            int     valid;
            if (0 != batch_driver->error) {
                valid = false;
                snprintf(error, sizeof(error), "%d", batch_driver->error);
            } else if (0 != batch_group->error) {
                valid = false;
                snprintf(error, sizeof(error), "%d", batch_group->error);
            } else if (NEU_TYPE_ERROR == batch_group->tags[j].value.type) {
                valid = false;
                snprintf(error, sizeof(error), "%d",
                         batch_group->tags[j].value.value.i32);
            } else {
                valid    = true;
                error[0] = '\0';
            }

            json_object_set_new(tag_value, "valid", json_integer(valid));
            json_object_set_new(tag_value, "remark", json_string(error));
        }
    }

    char *result = json_dumps(ctx->root, 0);
    return result;
}
