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

#ifndef _PLUGIN_MANAGER_H
#define _PLUGIN_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "adapter/adapter_internal.h"
#include "plugin.h"
#include "plugin_info.h"

#define DEFAULT_DASHBOARD_PLUGIN_NAME "default-dashboard-plugin"
#define DEFAULT_DASHBOARD_PLUGIN_LIB_NAME "libplugin_default_dashboard"
#define DEFAULT_DUMMY_PLUGIN_NAME "default-dummy-plugin"
#define DEFAULT_DUMMY_PLUGIN_LIB_NAME "libplugin_default_dummy"

typedef struct plugin_reg_param {
    plugin_kind_e  plugin_kind;
    adapter_type_e adapter_type;
    // The buffer should be move to plugin manager, don't free it
    const char *plugin_name;
    // The buffer should be move to plugin manager, don't free it
    char *plugin_lib_name;
} plugin_reg_param_t;

typedef struct plugin_reg_info {
    plugin_id_t    plugin_id;
    plugin_kind_e  plugin_kind;
    adapter_type_e adapter_type;
    // The buffer is reference from plugin entity of register table,
    // don't free it
    const char *plugin_name;
    // The buffer is reference from plugin entity of register table,
    // don't free it
    const char *plugin_lib_name;
} plugin_reg_info_t;

typedef struct plugin_manager plugin_manager_t;

plugin_manager_t *plugin_manager_create();
void              plugin_manager_destroy(plugin_manager_t *plugin_mng);
int               plugin_manager_reg_plugin(plugin_manager_t *        plugin_mng,
                                            const plugin_reg_param_t *param,
                                            plugin_id_t *             p_plugin_id);
int               plugin_manager_unreg_plugin(plugin_manager_t *plugin_mng,
                                              plugin_id_t       plugin_id);
int               plugin_manager_update_plugin(plugin_manager_t *        plugin_mng,
                                               const plugin_reg_param_t *param);
int               plugin_manager_get_reg_info(plugin_manager_t * plugin_mng,
                                              plugin_id_t        plugin_id,
                                              plugin_reg_info_t *reg_info);
int       plugin_manager_get_reg_info_by_name(plugin_manager_t * plugin_mng,
                                              const char *       plugin_name,
                                              plugin_reg_info_t *reg_info);
vector_t *plugin_manager_get_all_plugins(plugin_manager_t *plugin_mng);

void plugin_manager_unreg_all_plugins(plugin_manager_t *plugin_mng);
void plugin_manager_dump(plugin_manager_t *plugin_mng);

void *load_plugin_library(char *plugin_lib_name, plugin_kind_e plugin_kind,
                          neu_plugin_module_t **p_plugin_module);
int   unload_plugin_library(void *lib_handle, plugin_kind_e plugin_kind);

#ifdef __cplusplus
}
#endif

#endif
