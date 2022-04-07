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

#define NEURON_LOG_LABEL "plugin_manager"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "adapter/adapter_internal.h"
#include "config.h"
#include "dummy/dummy.h"
#include "neu_vector.h"
#include "panic.h"
#include "plugin_manager.h"
#include "restful/rest.h"

typedef struct plugin_reg_entity {
    plugin_id_t    plugin_id;
    plugin_kind_e  plugin_kind;
    adapter_type_e adapter_type;
    const char *   plugin_name;
    const char *   plugin_lib_name;
} plugin_reg_entity_t;

struct plugin_manager {
    nng_mtx *   mtx;
    plugin_id_t new_plugin_id;
    // for cache availabale id
    vector_t available_ids;
    vector_t system_plugins;
    vector_t custom_plugins;
};

/*
 * manage builtin static plugins
 */
static const plugin_reg_entity_t builtin_static_plugins[] = {
    {
        .plugin_id       = { 1 },
        .plugin_kind     = PLUGIN_KIND_STATIC,
        .adapter_type    = ADAPTER_TYPE_WEBSERVER,
        .plugin_name     = DEFAULT_DASHBOARD_PLUGIN_NAME,
        .plugin_lib_name = DEFAULT_DASHBOARD_PLUGIN_LIB_NAME,
    },
    {
        .plugin_id       = { 2 },
        .plugin_kind     = PLUGIN_KIND_STATIC,
        .adapter_type    = ADAPTER_TYPE_FUNCTIONAL,
        .plugin_name     = DEFAULT_DUMMY_PLUGIN_NAME,
        .plugin_lib_name = DEFAULT_DUMMY_PLUGIN_LIB_NAME,
    },
};
#define BUILTIN_STATIC_PLUGIN_COUNT \
    (sizeof(builtin_static_plugins) / sizeof(builtin_static_plugins[0]))

#define MIN_DYNAMIC_PLUGIN_ID (BUILTIN_STATIC_PLUGIN_COUNT + 1)

/*
 * manage dynamic plugins
 */
#define SYSTEM_PLUGIN_COUNT_DEFAULT 8
static int init_system_plugins(vector_t *plugins_vec)
{
    int rv = 0;

    rv = vector_init(plugins_vec, SYSTEM_PLUGIN_COUNT_DEFAULT,
                     sizeof(plugin_reg_entity_t));
    return rv;
}

static void uninit_system_plugins(vector_t *plugins_vec)
{
    vector_uninit(plugins_vec);
}

#define CUSTOM_PLUGIN_COUNT_DEFAULT 16
static int init_custom_plugins(vector_t *plugins_vec)
{
    int rv = 0;

    rv = vector_init(plugins_vec, CUSTOM_PLUGIN_COUNT_DEFAULT,
                     sizeof(plugin_reg_entity_t));
    return rv;
}

static void uninit_custom_plugins(vector_t *plugins_vec)
{
    plugin_reg_entity_t *reg_entity;

    VECTOR_FOR_EACH(plugins_vec, iter)
    {
        reg_entity = (plugin_reg_entity_t *) iterator_get(&iter);
        free((void *) reg_entity->plugin_name);
        free((void *) reg_entity->plugin_lib_name);
    }
    vector_uninit(plugins_vec);
}

// Return SIZE_MAX if can't find a plugin
static size_t find_plugin_by_id(vector_t *plugins, plugin_id_t id)
{
    size_t               index = SIZE_MAX;
    plugin_reg_entity_t *reg_entity;

    VECTOR_FOR_EACH(plugins, iter)
    {
        reg_entity = (plugin_reg_entity_t *) iterator_get(&iter);
        if (reg_entity->plugin_id.id_val == id.id_val) {
            index = iterator_index(plugins, &iter);
            break;
        }
    }

    return index;
}

// Return SIZE_MAX if can't find a plugin
static size_t find_static_plugin_by_id(plugin_id_t id)
{
    size_t               i;
    plugin_reg_entity_t *reg_entity;

    for (i = 0; i < BUILTIN_STATIC_PLUGIN_COUNT; i++) {
        reg_entity = (plugin_reg_entity_t *) &builtin_static_plugins[i];
        if (reg_entity->plugin_id.id_val == id.id_val) {
            return i;
        }
    }

    return SIZE_MAX;
}

// Return SIZE_MAX if can't find a plugin
static size_t find_plugin_by_name(vector_t *plugins, const char *plugin_name)
{
    size_t               index = SIZE_MAX;
    plugin_reg_entity_t *reg_entity;

    VECTOR_FOR_EACH(plugins, iter)
    {
        reg_entity = (plugin_reg_entity_t *) iterator_get(&iter);
        if (strcmp(reg_entity->plugin_name, plugin_name) == 0) {
            index = iterator_index(plugins, &iter);
            break;
        }
    }

    return index;
}

// Return SIZE_MAX if can't find a plugin
static size_t find_static_plugin_by_name(const char *plugin_name)
{
    size_t               i;
    plugin_reg_entity_t *reg_entity;

    for (i = 0; i < BUILTIN_STATIC_PLUGIN_COUNT; i++) {
        reg_entity = (plugin_reg_entity_t *) &builtin_static_plugins[i];
        if (strcmp(reg_entity->plugin_name, plugin_name) == 0) {
            return i;
        }
    }

    return SIZE_MAX;
}

static plugin_id_t plugin_manager_get_plugin_id(plugin_manager_t *plugin_mng,
                                                const plugin_reg_param_t *param)
{
    size_t      index;
    plugin_id_t plugin_id;
    vector_t *  plugins;

    nng_mtx_lock(plugin_mng->mtx);
    plugin_id = plugin_mng->new_plugin_id;
    plugin_mng->new_plugin_id.id_val++;
    nng_mtx_unlock(plugin_mng->mtx);

    plugins = &plugin_mng->system_plugins;
    if (param->plugin_kind == PLUGIN_KIND_CUSTOM) {
        plugins = &plugin_mng->custom_plugins;
    }

    // Check if there is a plugin with same name
    nng_mtx_lock(plugin_mng->mtx);
    index = find_plugin_by_name(plugins, param->plugin_name);
    if (index != SIZE_MAX) {
        plugin_id.id_val = 0;
        nng_mtx_unlock(plugin_mng->mtx);
        return plugin_id;
    }
    nng_mtx_unlock(plugin_mng->mtx);

    // TODO: get unique id for plugin
    return plugin_id;
}

static void reg_entity_init(plugin_reg_entity_t *     reg_entity,
                            plugin_id_t               plugin_id,
                            const plugin_reg_param_t *param)
{
    reg_entity->plugin_id       = plugin_id;
    reg_entity->plugin_kind     = param->plugin_kind;
    reg_entity->adapter_type    = param->adapter_type;
    reg_entity->plugin_name     = strdup(param->plugin_name);
    reg_entity->plugin_lib_name = strdup(param->plugin_lib_name);
    return;
}

static void reg_entity_uninit(plugin_reg_entity_t *reg_entity)
{
    reg_entity->plugin_id.id_val = 0;
    reg_entity->adapter_type     = 0;
    if (reg_entity->plugin_name != NULL) {
        free((void *) reg_entity->plugin_name);
    }
    if (reg_entity->plugin_lib_name != NULL) {
        free((void *) reg_entity->plugin_lib_name);
    }
    return;
}

plugin_manager_t *plugin_manager_create()
{
    int               rv, rv1;
    plugin_manager_t *plugin_mng;

    plugin_mng = (plugin_manager_t *) malloc(sizeof(plugin_manager_t));
    if (plugin_mng == NULL) {
        log_error("Out of memory for create plugin manager");
        return NULL;
    }

    rv = nng_mtx_alloc(&plugin_mng->mtx);
    if (rv != 0) {
        log_error("Failed to initialize mutex in plugin manager");
        return NULL;
    }

    plugin_mng->new_plugin_id.id_val = MIN_DYNAMIC_PLUGIN_ID;
    rv  = init_system_plugins(&plugin_mng->system_plugins);
    rv1 = init_custom_plugins(&plugin_mng->custom_plugins);
    if (rv != 0 || rv1 != 0) {
        log_error("Failed to initialize register table of plugins");
        return NULL;
    }

    rv = vector_init(&plugin_mng->available_ids, CUSTOM_PLUGIN_COUNT_DEFAULT,
                     sizeof(plugin_id_t));
    if (rv != 0) {
        log_error("Failed to initialize vector of available ids");
        return NULL;
    }

    return plugin_mng;
}

void plugin_manager_destroy(plugin_manager_t *plugin_mng)
{
    if (plugin_mng == NULL) {
        return;
    }

    vector_uninit(&plugin_mng->available_ids);
    uninit_custom_plugins(&plugin_mng->custom_plugins);
    uninit_system_plugins(&plugin_mng->system_plugins);
    nng_mtx_free(plugin_mng->mtx);
    free(plugin_mng);
}

/* If there already has same plugin registered, then return 0
 */
int plugin_manager_reg_plugin(plugin_manager_t *        plugin_mng,
                              const plugin_reg_param_t *param,
                              plugin_id_t *             p_plugin_id)
{
    int                 rv;
    int                 retry_count;
    vector_t *          plugins;
    plugin_reg_entity_t reg_entity;

    if (plugin_mng == NULL || param == NULL || p_plugin_id == NULL) {
        log_error("Register plugin with NULL pointer");
        return -1;
    }

    if (param->plugin_kind == PLUGIN_KIND_STATIC) {
        size_t index;
        /* static plugin had already been registered */
        p_plugin_id->id_val = 0;
        index               = find_static_plugin_by_name(param->plugin_name);
        if (index != SIZE_MAX) {
            *p_plugin_id = builtin_static_plugins[index].plugin_id;
        }
        return 0;
    }

    plugins = &plugin_mng->system_plugins;
    if (param->plugin_kind == PLUGIN_KIND_CUSTOM) {
        plugins = &plugin_mng->custom_plugins;
    }

    *p_plugin_id = plugin_manager_get_plugin_id(plugin_mng, param);
    if (p_plugin_id->id_val == 0) {
        // There has same registered plugin
        log_warn("A plugin with same name has already been registered");
        return NEU_ERR_LIBRARY_NAME_CONFLICT;
    }

    reg_entity_init(&reg_entity, *p_plugin_id, param);

    rv                    = -1;
    retry_count           = 3;
    useconds_t sleep_time = 300;
    while (retry_count-- > 0 && rv != 0) {
        nng_mtx_lock(plugin_mng->mtx);
        rv = vector_push_back(plugins, &reg_entity);
        nng_mtx_unlock(plugin_mng->mtx);
        if (rv != 0) {
            usleep(sleep_time);
            sleep_time *= 2;
        }
    }
    if (rv == 0) {
        log_info("Register the plugin: %s", reg_entity.plugin_name);
    } else {
        log_error("Failed to register plugin: %s", reg_entity.plugin_name);
    }
    return rv;
}

int plugin_manager_unreg_plugin(plugin_manager_t *plugin_mng,
                                plugin_id_t       plugin_id)
{
    size_t               index;
    vector_t *           plugins;
    plugin_reg_entity_t *reg_entity;

    if (plugin_id.id_val <= BUILTIN_STATIC_PLUGIN_COUNT) {
        /* Don't to unregister the static plugin */
        return 0;
    }

    int       i;
    vector_t *plugins_array[2] = { &plugin_mng->system_plugins,
                                   &plugin_mng->custom_plugins };
    for (i = 0; i < 2; i++) {
        plugins = plugins_array[i];
        nng_mtx_lock(plugin_mng->mtx);
        index = find_plugin_by_id(plugins, plugin_id);
        if (index != SIZE_MAX) {
            reg_entity = (plugin_reg_entity_t *) vector_get(plugins, index);
            log_info("Unregister the plugin: %s", reg_entity->plugin_name);
            reg_entity_uninit(reg_entity);
            vector_erase(plugins, index);
            nng_mtx_unlock(plugin_mng->mtx);
            return 0;
        }
        nng_mtx_unlock(plugin_mng->mtx);
    }

    log_warn("Can't find plugin with plugin_id: %d", plugin_id.id_val);
    return -1;
}

void plugin_manager_unreg_all_plugins(plugin_manager_t *plugin_mng)
{
    vector_t *           plugins;
    plugin_reg_entity_t *reg_entity;
    int                  i;

    vector_t *plugins_array[2] = { &plugin_mng->system_plugins,
                                   &plugin_mng->custom_plugins };
    for (i = 0; i < 2; i++) {
        plugins = plugins_array[i];
        VECTOR_FOR_EACH(plugins, iter)
        {
            reg_entity = (plugin_reg_entity_t *) iterator_get(&iter);
            reg_entity_uninit(reg_entity);
        }
        vector_clear(plugins);
    }

    return;
}

int plugin_manager_update_plugin(plugin_manager_t *        plugin_mng,
                                 const plugin_reg_param_t *param)
{
    int       rv = 0;
    size_t    index;
    vector_t *plugins;

    if (param->plugin_kind == PLUGIN_KIND_STATIC) {
        /* Can't to be updated for static plugin */
        return -1;
    }

    plugins = &plugin_mng->system_plugins;
    if (param->plugin_kind == PLUGIN_KIND_CUSTOM) {
        plugins = &plugin_mng->custom_plugins;
    }

    nng_mtx_lock(plugin_mng->mtx);
    index = find_plugin_by_name(plugins, (const char *) param->plugin_name);
    if (index != SIZE_MAX) {
        plugin_id_t          plugin_id;
        plugin_reg_entity_t *old_reg_entity;
        plugin_reg_entity_t  reg_entity;

        old_reg_entity = (plugin_reg_entity_t *) vector_get(plugins, index);
        plugin_id      = old_reg_entity->plugin_id;
        reg_entity_uninit(old_reg_entity);

        reg_entity_init(&reg_entity, plugin_id, param);
        vector_assign(plugins, index, &reg_entity);
        log_info("Update the plugin: %s", reg_entity.plugin_name);
    } else {
        // Nothing to update
        log_error("Failed to update the plugin: %s", param->plugin_name);
        rv = -1;
    }
    nng_mtx_unlock(plugin_mng->mtx);
    return rv;
}

void reg_info_from_reg_entity(plugin_reg_info_t *  reg_info,
                              plugin_reg_entity_t *reg_entity)
{
    reg_info->plugin_id       = reg_entity->plugin_id;
    reg_info->plugin_kind     = reg_entity->plugin_kind;
    reg_info->adapter_type    = reg_entity->adapter_type;
    reg_info->plugin_name     = reg_entity->plugin_name;
    reg_info->plugin_lib_name = reg_entity->plugin_lib_name;
    return;
}

int plugin_manager_get_reg_info(plugin_manager_t * plugin_mng,
                                plugin_id_t        plugin_id,
                                plugin_reg_info_t *reg_info)
{
    size_t               index;
    vector_t *           plugins;
    plugin_reg_entity_t *reg_entity;

    if (plugin_mng == NULL || reg_info == NULL) {
        return -1;
    }

    index = find_static_plugin_by_id(plugin_id);
    if (index != SIZE_MAX) {
        reg_entity = (plugin_reg_entity_t *) &builtin_static_plugins[index];
        reg_info_from_reg_entity(reg_info, reg_entity);
        return 0;
    }

    int       i;
    vector_t *plugins_array[2] = { &plugin_mng->system_plugins,
                                   &plugin_mng->custom_plugins };
    for (i = 0; i < 2; i++) {
        plugins = plugins_array[i];
        nng_mtx_lock(plugin_mng->mtx);
        index = find_plugin_by_id(plugins, plugin_id);
        if (index != SIZE_MAX) {
            reg_entity = (plugin_reg_entity_t *) vector_get(plugins, index);
            reg_info_from_reg_entity(reg_info, reg_entity);
            nng_mtx_unlock(plugin_mng->mtx);
            return 0;
        }
        nng_mtx_unlock(plugin_mng->mtx);
    }

    return -1;
}

int plugin_manager_get_reg_info_by_name(plugin_manager_t * plugin_mng,
                                        const char *       plugin_name,
                                        plugin_reg_info_t *reg_info)
{
    size_t               index;
    vector_t *           plugins;
    plugin_reg_entity_t *reg_entity;

    if (plugin_mng == NULL || plugin_name == NULL || reg_info == NULL) {
        return -1;
    }

    index = find_static_plugin_by_name(plugin_name);
    if (index != SIZE_MAX) {
        reg_entity = (plugin_reg_entity_t *) &builtin_static_plugins[index];
        reg_info_from_reg_entity(reg_info, reg_entity);
        return 0;
    }

    int       i;
    vector_t *plugins_array[2] = { &plugin_mng->system_plugins,
                                   &plugin_mng->custom_plugins };
    for (i = 0; i < 2; i++) {
        plugins = plugins_array[i];
        nng_mtx_lock(plugin_mng->mtx);
        index = find_plugin_by_name(plugins, plugin_name);
        if (index != SIZE_MAX) {
            reg_entity = (plugin_reg_entity_t *) vector_get(plugins, index);
            reg_info_from_reg_entity(reg_info, reg_entity);
            nng_mtx_unlock(plugin_mng->mtx);
            return 0;
        }
        nng_mtx_unlock(plugin_mng->mtx);
    }

    return -1;
}

vector_t *plugin_manager_get_all_plugins(plugin_manager_t *plugin_mng)
{
    size_t               count;
    vector_t *           plugin_info_vec;
    vector_t *           plugins;
    plugin_reg_info_t    reg_info;
    plugin_reg_entity_t *reg_entity;

    if (plugin_mng == NULL) {
        return NULL;
    }

    count = BUILTIN_STATIC_PLUGIN_COUNT;
    count += plugin_mng->system_plugins.size + plugin_mng->custom_plugins.size;
    plugin_info_vec = vector_new(count, sizeof(plugin_reg_info_t));
    if (plugin_info_vec == NULL) {
        log_error("No memory to new vector of plugin infos");
        return NULL;
    }

    size_t i;
    for (i = 0; i < BUILTIN_STATIC_PLUGIN_COUNT; i++) {
        reg_entity = (plugin_reg_entity_t *) &builtin_static_plugins[i];
        reg_info_from_reg_entity(&reg_info, reg_entity);
        vector_push_back(plugin_info_vec, &reg_info);
    }

    vector_t *plugins_array[2] = { &plugin_mng->system_plugins,
                                   &plugin_mng->custom_plugins };
    for (i = 0; i < 2; i++) {
        plugins = plugins_array[i];
        nng_mtx_lock(plugin_mng->mtx);
        VECTOR_FOR_EACH(plugins, iter)
        {
            reg_entity = (plugin_reg_entity_t *) iterator_get(&iter);
            reg_info_from_reg_entity(&reg_info, reg_entity);
            vector_push_back(plugin_info_vec, &reg_info);
        }
        nng_mtx_unlock(plugin_mng->mtx);
    }

    return plugin_info_vec;
}

void plugin_manager_dump(plugin_manager_t *plugin_mng)
{
    // TODO: dump system plugins and custom plugins
    (void) plugin_mng;
    return;
}

/*
 * load plugin and unload plugin
 */

static void *load_dyn_plugin_library(char *driver_path, char *plugin_lib_name,
                                     neu_plugin_module_t **p_plugin_module)
{
    void *lib_handle;

    if (driver_path == NULL) {
        lib_handle = dlopen(plugin_lib_name, RTLD_NOW | RTLD_NODELETE);
    } else {
        char lib_path[128] = { 0 };
        snprintf(lib_path, sizeof(lib_path) - 1, "%s%s", driver_path,
                 plugin_lib_name);
        lib_handle = dlopen(lib_path, RTLD_NOW | RTLD_NODELETE);
    }

    if (lib_handle == NULL) {
        log_error("Failed to open dynamic library %s: %s", plugin_lib_name,
                  dlerror());

        return NULL;
    }

    void *module_info;
    module_info = dlsym(lib_handle, "neu_plugin_module");
    if (module_info == NULL) {
        dlclose(lib_handle);
        log_error("Failed to get neu_plugin_module from %s", plugin_lib_name);
        return NULL;
    }

    if (p_plugin_module != NULL) {
        *p_plugin_module = (neu_plugin_module_t *) module_info;
    }
    return lib_handle;
}

static int unload_dyn_plugin_library(void *lib_handle)
{
    return dlclose(lib_handle);
}

typedef struct plugin_module_map {
    const char *               plugin_lib_name;
    const neu_plugin_module_t *plugin_module;
} plugin_module_map_t;

static const plugin_module_map_t plugin_module_maps[] = {
    {
        DEFAULT_DASHBOARD_PLUGIN_LIB_NAME,
        &default_dashboard_plugin_module,

    },
    {
        DEFAULT_DUMMY_PLUGIN_LIB_NAME,
        &default_dummy_plugin_module,
    },
};
#define PLUGIN_MODULE_MAPS_COUNT \
    (sizeof(plugin_module_maps) / sizeof(plugin_module_maps[0]))

// return index of plugin in static plugin list
static void *load_static_plugin(char *                plugin_lib_name,
                                neu_plugin_module_t **p_plugin_module)
{
    size_t i;

    for (i = 0; i < PLUGIN_MODULE_MAPS_COUNT; i++) {
        if (strcmp(plugin_lib_name, plugin_module_maps[i].plugin_lib_name) ==
            0) {
            if (p_plugin_module != NULL) {
                *p_plugin_module =
                    (neu_plugin_module_t *) plugin_module_maps[i].plugin_module;
            }
            return (void *) (i + 1); // the pointer must not be NULL
        }
    }
    return NULL;
}

static int unload_static_plugin(void *lib_handle)
{
    /* do nothing */
    (void) lib_handle;
    return 0;
}

// if plugin is static plugin then return index of plugin in static plugin list
void *load_plugin_library(char *plugin_lib_name, plugin_kind_e plugin_kind,
                          neu_plugin_module_t **p_plugin_module)
{
    void *result = NULL;

    switch (plugin_kind) {
    case PLUGIN_KIND_STATIC:
        result = load_static_plugin(plugin_lib_name, p_plugin_module);
        break;
    case PLUGIN_KIND_SYSTEM:
    case PLUGIN_KIND_CUSTOM:
        result = load_dyn_plugin_library("./plugins/", plugin_lib_name,
                                         p_plugin_module);
        break;
    }

    return result;
}

int unload_plugin_library(void *lib_handle, plugin_kind_e plugin_kind)
{
    if (plugin_kind == PLUGIN_KIND_STATIC) {
        return unload_static_plugin(lib_handle);
    } else {
        return unload_dyn_plugin_library(lib_handle);
    }
}
