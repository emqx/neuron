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

#ifndef _NEU_NODE_MANAGER_H_
#define _NEU_NODE_MANAGER_H_

#include <nng/nng.h>

#include "define.h"
#include "utils/utextend.h"

#include "adapter.h"
#include "adapter_msg.h"

typedef struct neu_node_manager neu_node_manager_t;

// typedef struct neu_node_info {
// char name[NEU_NODE_NAME_LEN];
// char plugin_name[NEU_PLUGIN_NAME_LEN];
//} neu_node_info_t;

neu_node_manager_t *neu_node_manager_create();
void                neu_node_manager_destroy(neu_node_manager_t *mgr);

uint32_t neu_node_manager_add(neu_node_manager_t *mgr, neu_adapter_t *adapter);
uint32_t neu_node_manager_add_static(neu_node_manager_t *mgr,
                                     neu_adapter_t *     adapter);
int      neu_node_manager_update(neu_node_manager_t *mgr, const char *name,
                                 nng_pipe pipe);
void     neu_node_manager_del(neu_node_manager_t *mgr, const char *name);

// neu_node_info array
UT_array *neu_node_manager_get(neu_node_manager_t *mgr, neu_node_type_e type);
neu_adapter_t *neu_node_manager_find(neu_node_manager_t *mgr, const char *name);
neu_adapter_t *neu_node_manager_find_by_id(neu_node_manager_t *mgr,
                                           uint32_t            id);

// nng_pipe array
UT_array *neu_node_manager_get_pipes(neu_node_manager_t *mgr,
                                     neu_node_type_e     type);
nng_pipe  neu_node_manager_get_pipe(neu_node_manager_t *mgr, const char *name);
nng_pipe  neu_node_manager_get_pipe_by_id(neu_node_manager_t *mgr, uint32_t id);

#endif