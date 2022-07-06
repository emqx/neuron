/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2021-2022 EMQ Technologies Co., Ltd All rights reserved.
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

typedef struct neu_node_manager neu_node_manager_t;

neu_node_manager_t *neu_node_manager_create();
void                neu_node_manager_destroy(neu_node_manager_t *mgr);

int      neu_node_manager_add(neu_node_manager_t *mgr, neu_adapter_t *adapter);
int      neu_node_manager_add_static(neu_node_manager_t *mgr,
                                     neu_adapter_t *     adapter);
int      neu_node_manager_update(neu_node_manager_t *mgr, const char *name,
                                 nng_pipe pipe);
bool     neu_node_manager_exist_uninit(neu_node_manager_t *mgr);
void     neu_node_manager_del(neu_node_manager_t *mgr, const char *name);
uint16_t neu_node_manager_size(neu_node_manager_t *mgr);

// neu_resp_node_info array
UT_array *neu_node_manager_get(neu_node_manager_t *mgr, neu_node_type_e type);
UT_array *neu_node_manager_get_all(neu_node_manager_t *mgr);
neu_adapter_t *neu_node_manager_find(neu_node_manager_t *mgr, const char *name);

// nng_pipe array
UT_array *neu_node_manager_get_pipes(neu_node_manager_t *mgr,
                                     neu_node_type_e     type);
UT_array *neu_node_manager_get_pipes_all(neu_node_manager_t *mgr);
nng_pipe  neu_node_manager_get_pipe(neu_node_manager_t *mgr, const char *name);

#endif