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

#ifndef _NEU_PLUGIN_MANAGER_H_
#define _NEU_PLUGIN_MANAGER_H_

#include "utils/utextend.h"

#include "adapter.h"
#include "define.h"
#include "plugin.h"

#define DEFAULT_DASHBOARD_PLUGIN_NAME "default-dashboard"

typedef struct neu_plugin_instance {
    void *               handle;
    neu_plugin_module_t *module;
} neu_plugin_instance_t;

typedef struct neu_plugin_manager neu_plugin_manager_t;

neu_plugin_manager_t *neu_plugin_manager_create();
void                  neu_plugin_manager_destroy(neu_plugin_manager_t *mgr);

int neu_plugin_manager_add(neu_plugin_manager_t *mgr,
                           const char *          plugin_lib_name);
int neu_plugin_manager_del(neu_plugin_manager_t *mgr, const char *plugin_name);

// neu_resp_plugin_info_t array
UT_array *neu_plugin_manager_get(neu_plugin_manager_t *mgr);
// neu_resp_plugin_info_t array
UT_array *neu_plugin_manager_get_single(neu_plugin_manager_t *mgr);
int  neu_plugin_manager_find(neu_plugin_manager_t *mgr, const char *plugin_name,
                             neu_resp_plugin_info_t *info);
bool neu_plugin_manager_exists(neu_plugin_manager_t *mgr,
                               const char *          plugin_name);
bool neu_plugin_manager_is_single(neu_plugin_manager_t *mgr,
                                  const char *          plugin_name);

int  neu_plugin_manager_create_instance(neu_plugin_manager_t * mgr,
                                        const char *           plugin_name,
                                        neu_plugin_instance_t *instance);
void neu_plugin_manager_destroy_instance(neu_plugin_manager_t * mgr,
                                         neu_plugin_instance_t *instance);

void neu_plugin_manager_load_static(neu_plugin_manager_t * mgr,
                                    const char *           plugin_name,
                                    neu_plugin_instance_t *instance);

bool neu_plugin_manager_create_instance_by_path(neu_plugin_manager_t *mgr,
                                                const char *plugin_path,
                                                neu_plugin_instance_t *instance,
                                                int *                  error);

bool neu_plugin_manager_create_instance_by_lib_name(
    neu_plugin_manager_t *mgr, const char *lib_name,
    neu_plugin_instance_t *instance);

bool neu_plugin_manager_remove_library(neu_plugin_manager_t *mgr,
                                       const char *          library);

int neu_plugin_manager_update(neu_plugin_manager_t *mgr,
                              const char *          plugin_lib_name);

bool neu_plugin_manager_schema_exist(neu_plugin_manager_t *mgr,
                                     const char *          schema);

#endif