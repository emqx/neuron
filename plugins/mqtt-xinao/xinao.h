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

#ifndef NEURON_PLUGIN_MQTT_XINAO
#define NEURON_PLUGIN_MQTT_XINAO

#include "msg.h"
#include "plugin.h"
#include "tag.h"
#include "utils/uthash.h"
#include "json/json.h"

#include "mqtt_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *   group;
    int            ind;
    UT_hash_handle hh;
} group_index_t;

typedef struct {
    const char *   node;
    int            ind;
    group_index_t *grp_indices;
    UT_hash_handle hh;
} node_index_t;

typedef node_index_t *index_map_t;

typedef struct {
    void *             root;
    index_map_t        map;
    neu_write_batch_t *batch;
} xinao_ctx_t;

static inline void index_map_free(index_map_t *map)
{
    node_index_t * n = NULL, *ntmp = NULL;
    group_index_t *g = NULL, *gtmp = NULL;
    HASH_ITER(hh, *map, n, ntmp)
    {
        HASH_ITER(hh, n->grp_indices, g, gtmp)
        {
            HASH_DEL(n->grp_indices, g);
            free(g);
        }
        HASH_DEL(*map, n);
        free(n);
    }
}

static inline int index_map_add(index_map_t *map, const char *node,
                                const char *group)
{
    node_index_t *n = NULL;
    HASH_FIND_STR(*map, node, n);
    if (NULL == n) {
        n = calloc(1, sizeof(*n));
        if (NULL == n) {
            return -1;
        }
        n->node = node;
        n->ind  = HASH_COUNT(*map);
        HASH_ADD_KEYPTR(hh, *map, n->node, strlen(n->node), n);
    }

    group_index_t *g = NULL;
    HASH_FIND_STR(n->grp_indices, group, g);
    if (NULL != g) {
        return -1;
    }

    g = calloc(1, sizeof(*g));
    if (NULL == g) {
        return -1;
    }
    g->ind   = HASH_COUNT(n->grp_indices);
    g->group = group;
    HASH_ADD_KEYPTR(hh, n->grp_indices, g->group, strlen(g->group), g);

    return 0;
}

int neu_json_decode_write_batch_req_json(neu_plugin_t *plugin, void *json_root,
                                         neu_write_batch_t **batch_p,
                                         index_map_t *       map_p);

xinao_ctx_t *xinao_ctx_from_json_buf(neu_plugin_t *plugin, char *buf,
                                     size_t len);
char *       xinao_ctx_gen_resp_json(xinao_ctx_t *ctx);

static inline void xinao_ctx_free(xinao_ctx_t *ctx)
{
    if (ctx) {
        neu_json_decode_free(ctx->root);
        index_map_free(&ctx->map);
        neu_write_batch_free(ctx->batch);
        free(ctx);
    }
}

#ifdef __cplusplus
}
#endif

#endif
