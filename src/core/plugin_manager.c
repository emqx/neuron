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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "adapter/adapter_internal.h"
#include "neu_panic.h"
#include "neu_vector.h"
#include "plugin_manager.h"

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
    reg_entity->plugin_name     = param->plugin_name;
    reg_entity->plugin_lib_name = param->plugin_lib_name;
    return;
}

static void reg_entity_uninit(plugin_reg_entity_t *reg_entity)
{
    reg_entity->plugin_id.id_val = 0;
    reg_entity->adapter_type     = 0;
    if (reg_entity->plugin_kind == PLUGIN_KIND_CUSTOM) {
        free((void *) reg_entity->plugin_name);
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

    plugin_mng->new_plugin_id.id_val = 1;
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
plugin_id_t plugin_manager_reg_plugin(plugin_manager_t *        plugin_mng,
                                      const plugin_reg_param_t *param)
{
    int                 rv;
    int                 retry_count;
    vector_t *          plugins;
    plugin_id_t         plugin_id;
    plugin_reg_entity_t reg_entity;

    plugins = &plugin_mng->system_plugins;
    if (param->plugin_kind == PLUGIN_KIND_CUSTOM) {
        plugins = &plugin_mng->custom_plugins;
    }

    plugin_id = plugin_manager_get_plugin_id(plugin_mng, param);
    if (plugin_id.id_val == 0) {
        // There has same registered plugin
        log_warn("A plugin with same name has already been registered");
        return plugin_id;
    }

    reg_entity_init(&reg_entity, plugin_id, param);
    log_info("Register the plugin: %s", reg_entity.plugin_name);

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
    if (rv != 0) {
        neu_panic("Failed to register plugin");
    }
    return plugin_id;
}

int plugin_manager_unreg_plugin(plugin_manager_t *        plugin_mng,
                                const plugin_reg_param_t *param)
{
    int                  rv = 0;
    size_t               index;
    vector_t *           plugins;
    plugin_reg_entity_t *reg_entity;

    plugins = &plugin_mng->system_plugins;
    if (param->plugin_kind == PLUGIN_KIND_CUSTOM) {
        plugins = &plugin_mng->custom_plugins;
    }

    nng_mtx_lock(plugin_mng->mtx);
    index = find_plugin_by_name(plugins, (const char *) param->plugin_name);
    if (index == SIZE_MAX) {
        reg_entity = (plugin_reg_entity_t *) vector_get(plugins, index);
        reg_entity_uninit(reg_entity);
        vector_erase(plugins, index);
    }
    nng_mtx_unlock(plugin_mng->mtx);
    if (index == SIZE_MAX) {
        log_info("Unregister the plugin: %s", reg_entity->plugin_name);
    }
    return rv;
}

void plugin_manager_unreg_all_plugins(plugin_manager_t *plugin_mng)
{
    vector_t *           plugins;
    plugin_reg_entity_t *reg_entity;

    plugins = &plugin_mng->system_plugins;
    VECTOR_FOR_EACH(plugins, iter)
    {
        reg_entity = (plugin_reg_entity_t *) iterator_get(&iter);
        reg_entity_uninit(reg_entity);
        break;
    }
    vector_clear(plugins);

    plugins = &plugin_mng->custom_plugins;
    VECTOR_FOR_EACH(plugins, iter)
    {
        reg_entity = (plugin_reg_entity_t *) iterator_get(&iter);
        reg_entity_uninit(reg_entity);
        break;
    }
    vector_clear(plugins);
    return;
}

int plugin_manager_update_plugin(plugin_manager_t *        plugin_mng,
                                 const plugin_reg_param_t *param)
{
    int       rv = 0;
    size_t    index;
    vector_t *plugins;

    plugins = &plugin_mng->system_plugins;
    if (param->plugin_kind == PLUGIN_KIND_CUSTOM) {
        plugins = &plugin_mng->custom_plugins;
    }

    nng_mtx_lock(plugin_mng->mtx);
    index = find_plugin_by_name(plugins, (const char *) param->plugin_name);
    if (index == SIZE_MAX) {
        plugin_id_t          plugin_id;
        plugin_reg_entity_t *old_reg_entity;
        plugin_reg_entity_t  reg_entity;

        old_reg_entity = (plugin_reg_entity_t *) vector_get(plugins, index);
        plugin_id      = old_reg_entity->plugin_id;
        reg_entity_uninit(old_reg_entity);

        reg_entity_init(&reg_entity, plugin_id, param);
        vector_assign(plugins, index, &reg_entity);
        reg_entity_uninit(&reg_entity);
    } else {
        // Nothing to update
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

void plugin_manager_dump(plugin_manager_t *plugin_mng)
{
    // TODO: dump system plugins and custom plugins
    (void) plugin_mng;
    return;
}
