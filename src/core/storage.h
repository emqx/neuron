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

#ifndef _NEU_MANAGER_STORAGE_H_
#define _NEU_MANAGER_STORAGE_H_

#include "persist/persist.h"

#include "manager_internal.h"

void manager_strorage_plugin(neu_manager_t *manager);
void manager_storage_del_node(neu_manager_t *manager, const char *node);
void manager_storage_add_node(neu_manager_t *manager, const char *node,
                              const char *tags);
void manager_storage_update_node_tags(neu_manager_t *manager, const char *node,
                                      const char *new_tags);
void manager_storage_update_node(neu_manager_t *manager, const char *node,
                                 const char *new_name);
void manager_storage_subscribe(neu_manager_t *manager, const char *app,
                               const char *driver, const char *group,
                               const char *params, const char *static_tags);
void manager_storage_update_subscribe(neu_manager_t *manager, const char *app,
                                      const char *driver, const char *group,
                                      const char *params,
                                      const char *static_tags);
void manager_storage_unsubscribe(neu_manager_t *manager, const char *app,
                                 const char *driver, const char *group);

void manager_storage_inst_node(neu_manager_t *manager, const char *tmpl_name,
                               const char *node);

int manager_load_plugin(neu_manager_t *manager);
int manager_load_node(neu_manager_t *manager);
int manager_load_subscribe(neu_manager_t *manager);

#endif
