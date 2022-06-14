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

#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "adapter/adapter_internal.h"

#include "node_manager.h"

typedef struct node_entity {
    char *name;

    neu_adapter_t *adapter;
    bool           is_static;
    uint32_t       adapter_id;
    nng_pipe       pipe;

    UT_hash_handle hh;
    UT_hash_handle hh_id;
} node_entity_t;

struct neu_node_manager {
    nng_mtx *mtx;

    uint32_t       adapter_id;
    node_entity_t *nodes;
    node_entity_t *nodes_by_id;
};

neu_node_manager_t *neu_node_manager_create()
{
    neu_node_manager_t *node_manager = calloc(1, sizeof(neu_node_manager_t));

    nng_mtx_alloc(&node_manager->mtx);
    node_manager->adapter_id = 1;

    return node_manager;
}

void neu_node_manager_destroy(neu_node_manager_t *mgr)
{
    node_entity_t *el = NULL, *tmp = NULL;

    nng_mtx_lock(mgr->mtx);
    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        HASH_DELETE(hh_id, mgr->nodes_by_id, el);
        HASH_DEL(mgr->nodes, el);
        neu_adapter_uninit(el->adapter);
        neu_adapter_destroy(el->adapter);
        free(el->name);
        free(el);
    }
    nng_mtx_unlock(mgr->mtx);

    nng_mtx_free(mgr->mtx);
    free(mgr);
}

uint32_t neu_node_manager_add(neu_node_manager_t *mgr, neu_adapter_t *adapter)
{
    node_entity_t *node = calloc(1, sizeof(node_entity_t));

    node->adapter    = adapter;
    node->adapter_id = mgr->adapter_id++;
    node->name       = strdup(adapter->name);

    nng_mtx_lock(mgr->mtx);
    HASH_ADD_STR(mgr->nodes, name, node);
    HASH_ADD(hh_id, mgr->nodes_by_id, adapter_id, sizeof(node->adapter_id),
             node);
    nng_mtx_unlock(mgr->mtx);

    return node->adapter_id;
}

uint32_t neu_node_manager_add_static(neu_node_manager_t *mgr,
                                     neu_adapter_t *     adapter)
{
    node_entity_t *node = calloc(1, sizeof(node_entity_t));

    node->adapter    = adapter;
    node->adapter_id = mgr->adapter_id++;
    node->name       = strdup(adapter->name);
    node->is_static  = true;

    nng_mtx_lock(mgr->mtx);
    HASH_ADD_STR(mgr->nodes, name, node);
    HASH_ADD(hh_id, mgr->nodes_by_id, adapter_id, sizeof(node->adapter_id),
             node);
    nng_mtx_unlock(mgr->mtx);

    return node->adapter_id;
}

int neu_node_manager_update(neu_node_manager_t *mgr, const char *name,
                            nng_pipe pipe)
{
    node_entity_t *node = NULL;

    nng_mtx_lock(mgr->mtx);
    HASH_FIND_STR(mgr->nodes, name, node);
    assert(node != NULL);
    node->pipe = pipe;
    nng_mtx_unlock(mgr->mtx);

    return 0;
}

void neu_node_manager_del(neu_node_manager_t *mgr, const char *name)
{
    node_entity_t *node = NULL;

    nng_mtx_lock(mgr->mtx);
    HASH_FIND_STR(mgr->nodes, name, node);
    if (node != NULL) {
        HASH_DEL(mgr->nodes, node);
        HASH_DELETE(hh_id, mgr->nodes_by_id, node);
        free(node->name);
        free(node);
    }
    nng_mtx_unlock(mgr->mtx);
}

UT_array *neu_node_manager_get(neu_node_manager_t *mgr, neu_node_type_e type)
{
    UT_array *     array = NULL;
    UT_icd         icd   = { sizeof(neu_resp_node_info_t), NULL, NULL, NULL };
    node_entity_t *el = NULL, *tmp = NULL;

    utarray_new(array, &icd);

    nng_mtx_lock(mgr->mtx);
    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        if (!el->is_static) {
            if (el->adapter->plugin_info.module->type == type) {
                neu_resp_node_info_t info = { 0 };
                strcpy(info.node, el->adapter->name);
                strcpy(info.plugin,
                       el->adapter->plugin_info.module->module_name);
                utarray_push_back(array, &info);
            }
        }
    }
    nng_mtx_unlock(mgr->mtx);

    return array;
}

neu_adapter_t *neu_node_manager_find(neu_node_manager_t *mgr, const char *name)
{
    neu_adapter_t *adapter = NULL;
    node_entity_t *node    = NULL;

    nng_mtx_lock(mgr->mtx);
    HASH_FIND_STR(mgr->nodes, name, node);
    if (node != NULL) {
        adapter = node->adapter;
    }
    nng_mtx_unlock(mgr->mtx);

    return adapter;
}

neu_adapter_t *neu_node_manager_find_by_id(neu_node_manager_t *mgr, uint32_t id)
{
    neu_adapter_t *adapter = NULL;
    node_entity_t *node    = NULL;

    nng_mtx_lock(mgr->mtx);
    HASH_FIND(hh_id, mgr->nodes_by_id, &id, sizeof(id), node);
    if (node != NULL) {
        adapter = node->adapter;
    }
    nng_mtx_unlock(mgr->mtx);

    return adapter;
}

UT_array *neu_node_manager_get_pipes(neu_node_manager_t *mgr,
                                     neu_node_type_e     type)
{
    UT_icd         icd   = { sizeof(nng_pipe), NULL, NULL, NULL };
    UT_array *     pipes = NULL;
    node_entity_t *el = NULL, *tmp = NULL;

    utarray_new(pipes, &icd);

    nng_mtx_lock(mgr->mtx);
    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        if (!el->is_static) {
            if (el->adapter->plugin_info.module->type == type) {
                nng_pipe pipe = el->pipe;
                utarray_push_back(pipes, &pipe);
            }
        }
    }
    nng_mtx_unlock(mgr->mtx);

    return pipes;
}

nng_pipe neu_node_manager_get_pipe(neu_node_manager_t *mgr, const char *name)
{
    nng_pipe       pipe = { 0 };
    node_entity_t *node = NULL;

    nng_mtx_lock(mgr->mtx);
    HASH_FIND_STR(mgr->nodes, name, node);
    if (node != NULL) {
        pipe = node->pipe;
    }
    nng_mtx_unlock(mgr->mtx);

    return pipe;
}

nng_pipe neu_node_manager_get_pipe_by_id(neu_node_manager_t *mgr, uint32_t id)
{
    nng_pipe       pipe = { 0 };
    node_entity_t *node = NULL;

    nng_mtx_lock(mgr->mtx);
    HASH_FIND(hh_id, mgr->nodes_by_id, &id, sizeof(id), node);
    if (node != NULL) {
        pipe = node->pipe;
    }
    nng_mtx_unlock(mgr->mtx);

    return pipe;
}