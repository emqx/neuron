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

#ifndef _NEU_MANAGER_H_
#define _NEU_MANAGER_H_

#include "utils/utextend.h"

#include "persist/persist.h"

typedef struct neu_manager neu_manager_t;

neu_manager_t *neu_manager_create();
void           neu_manager_destroy(neu_manager_t *manager);

const char *neu_manager_get_url(neu_manager_t *manager);

int       neu_manager_add_node(neu_manager_t *manager, const char *node_name,
                               const char *plugin_name);
int       neu_manager_del_node(neu_manager_t *manager, const char *node_name);
UT_array *neu_manager_get_nodes(neu_manager_t *manager, neu_node_type_e type);

int       neu_manager_subscribe(neu_manager_t *manager, const char *app,
                                const char *driver, const char *group);
int       neu_manager_unsubscribe(neu_manager_t *manager, const char *app,
                                  const char *driver, const char *group);
UT_array *neu_manager_get_sub_group(neu_manager_t *manager, const char *app);

int neu_manager_add_group(neu_manager_t *manager, const char *driver,
                          const char *group, uint32_t interval);
int neu_manager_del_group(neu_manager_t *manager, const char *driver,
                          const char *group);
int neu_manager_update_group(neu_manager_t *manager, const char *driver,
                             const char *group, uint32_t interval);
int neu_manager_get_group(neu_manager_t *manager, const char *driver,
                          UT_array **groups);

int neu_manager_add_plugin(neu_manager_t *manager, const char *plugin_library);
int neu_manager_del_plugin(neu_manager_t *manager, const char *plugin);
// plugin_lib_info_t array
UT_array *neu_manager_get_plugins(neu_manager_t *manager);

int neu_manager_start_node(neu_manager_t *manager, const char *node);
int neu_manager_stop_node(neu_manager_t *manager, const char *node);

int neu_manager_node_setting(neu_manager_t *manager, const char *node,
                             const char *setting);
int neu_manager_node_get_setting(neu_manager_t *manager, const char *node,
                                 char **setting);

int       neu_manager_get_adapter_info(neu_manager_t *manager, const char *name,
                                       neu_persist_adapter_info_t *info);
UT_array *neu_manager_get_plugin_info(neu_manager_t *manager);

#endif
