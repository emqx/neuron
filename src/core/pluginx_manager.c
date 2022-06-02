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
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "errcodes.h"
#include "utils/log.h"
#include "utils/utextend.h"

#include "adapter.h"
#include "plugin.h"
#include "plugin_info.h"

#include "dummy/dummy.h"
#include "restful/rest.h"

#include "pluginx_manager.h"

typedef struct plugin_entity {
    char *name;
    char *lib_name;
    char *description;

    neu_plugin_kind_e kind;
    neu_node_type_e   type;

    UT_hash_handle hh;
} plugin_entity_t;

struct neu_plugin_manager {
    nng_mtx *mtx;

    plugin_entity_t *plugins;
};

static void entity_free(plugin_entity_t *entity);

neu_plugin_manager_t *neu_plugin_manager_create()
{
    neu_plugin_manager_t *manager = calloc(1, sizeof(neu_plugin_manager_t));

    nng_mtx_alloc(&manager->mtx);

    return manager;
}

void neu_plugin_manager_destroy(neu_plugin_manager_t *mgr)
{
    plugin_entity_t *el = NULL, *tmp = NULL;

    nng_mtx_lock(mgr->mtx);
    HASH_ITER(hh, mgr->plugins, el, tmp)
    {
        HASH_DEL(mgr->plugins, el);
        entity_free(el);
    }

    nng_mtx_unlock(mgr->mtx);

    nng_mtx_free(mgr->mtx);
    free(mgr);
}

int neu_plugin_manager_add(neu_plugin_manager_t *mgr,
                           const char *          plugin_lib_name)
{
    char                 lib_path[256] = { 0 };
    void *               handle        = NULL;
    void *               module        = NULL;
    neu_plugin_module_t *pm            = NULL;
    plugin_entity_t *    plugin        = NULL;

    snprintf(lib_path, sizeof(lib_path) - 1, "./plugins/%s", plugin_lib_name);

    handle = dlopen(lib_path, RTLD_NOW | RTLD_NODELETE);

    if (handle == NULL) {
        nlog_warn("failed to open library %s, error: %s", lib_path, dlerror());
        return NEU_ERR_LIBRARY_FAILED_TO_OPEN;
    }

    module = dlsym(handle, "neu_plugin_module");
    if (module == NULL) {
        dlclose(handle);
        nlog_warn("failed to find neu_plugin_module symbol in %s", lib_path);
        return NEU_ERR_LIBRARY_MODULE_INVALID;
    }

    pm = (neu_plugin_module_t *) module;
    if (pm->kind != NEU_PLUGIN_KIND_CUSTOM &&
        pm->kind != NEU_PLUGIN_KIND_SYSTEM) {
        dlclose(handle);
        nlog_warn("library: %s, kind wrong: %d", lib_path, pm->kind);
        return NEU_ERR_LIBRARY_INFO_INVALID;
    }

    if (pm->type != NEU_NA_TYPE_APP && pm->type != NEU_NA_TYPE_DRIVER) {
        dlclose(handle);
        nlog_warn("library: %s, type wrong: %d", lib_path, pm->type);
        return NEU_ERR_LIBRARY_INFO_INVALID;
    }

    nng_mtx_lock(mgr->mtx);
    HASH_FIND_STR(mgr->plugins, pm->module_name, plugin);
    if (plugin != NULL) {
        dlclose(handle);
        nng_mtx_unlock(mgr->mtx);
        return NEU_ERR_LIBRARY_NAME_CONFLICT;
    }
    plugin = calloc(1, sizeof(plugin_entity_t));

    plugin->type        = pm->type;
    plugin->kind        = pm->kind;
    plugin->name        = strdup(pm->module_name);
    plugin->lib_name    = strdup(plugin_lib_name);
    plugin->description = strdup(pm->module_descr);

    HASH_ADD_STR(mgr->plugins, name, plugin);
    nng_mtx_unlock(mgr->mtx);

    nlog_notice("add plugin, name: %s, library: %s, kind: %d, type: %d",
                plugin->name, plugin->lib_name, plugin->kind, plugin->type);

    dlclose(handle);
    return NEU_ERR_SUCCESS;
}

int neu_plugin_manager_del(neu_plugin_manager_t *mgr, const char *plugin_name)
{
    plugin_entity_t *plugin = NULL;
    int              ret    = NEU_ERR_LIBRARY_SYSTEM_NOT_ALLOW_DEL;

    nng_mtx_lock(mgr->mtx);
    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    if (plugin != NULL) {
        if (plugin->kind != NEU_PLUGIN_KIND_SYSTEM) {
            HASH_DEL(mgr->plugins, plugin);
            entity_free(plugin);
            ret = NEU_ERR_SUCCESS;
        }
    }

    nng_mtx_unlock(mgr->mtx);
    return ret;
}

UT_array *neu_plugin_manager_get(neu_plugin_manager_t *mgr)
{
    UT_array *       plugins;
    UT_icd           icd = { sizeof(neu_plugin_lib_info_t), NULL, NULL, NULL };
    plugin_entity_t *el = NULL, *tmp = NULL;

    utarray_new(plugins, &icd);
    nng_mtx_lock(mgr->mtx);
    HASH_ITER(hh, mgr->plugins, el, tmp)
    {
        neu_plugin_lib_info_t info = {
            .kind = el->kind,
            .type = el->type,
        };

        strncpy(info.name, el->name, sizeof(info.name));
        strncpy(info.lib_name, el->lib_name, sizeof(info.lib_name));
        strncpy(info.description, el->description, sizeof(info.description));

        utarray_push_back(plugins, &info);
    }
    nng_mtx_unlock(mgr->mtx);

    return plugins;
}

int neu_plugin_manager_find(neu_plugin_manager_t *mgr, const char *plugin_name,
                            neu_plugin_lib_info_t *info)
{
    plugin_entity_t *plugin = NULL;
    int              ret    = -1;

    nng_mtx_lock(mgr->mtx);
    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    if (plugin != NULL) {
        ret        = 0;
        info->type = plugin->type;
        info->kind = plugin->kind;
        strncpy(info->name, plugin->name, sizeof(info->name));
        strncpy(info->lib_name, plugin->lib_name, sizeof(info->lib_name));
        strncpy(info->description, plugin->description,
                sizeof(info->description));
    }
    nng_mtx_unlock(mgr->mtx);

    return ret;
}

int neu_plugin_manager_create_instance(neu_plugin_manager_t * mgr,
                                       const char *           plugin_name,
                                       neu_plugin_instance_t *instance)
{
    plugin_entity_t *plugin = NULL;

    nng_mtx_lock(mgr->mtx);
    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    if (plugin != NULL) {
        char lib_path[256] = { 0 };

        snprintf(lib_path, sizeof(lib_path) - 1, "./plugins/%s",
                 plugin->lib_name);
        instance->handle = dlopen(lib_path, RTLD_NOW | RTLD_NODELETE);
        assert(instance->handle != NULL);

        instance->module = (neu_plugin_module_t *) dlsym(instance->handle,
                                                         "neu_plugin_module");
        assert(instance->module != NULL);
        nng_mtx_unlock(mgr->mtx);
        return 0;
    }
    nng_mtx_unlock(mgr->mtx);

    return -1;
}

void neu_plugin_manager_load_static(neu_plugin_manager_t * mgr,
                                    const char *           plugin_name,
                                    neu_plugin_instance_t *instance)
{
    (void) mgr;
    instance->handle = NULL;

    if (strcmp(DEFAULT_DASHBOARD_PLUGIN_NAME, plugin_name) == 0) {
        instance->module =
            (neu_plugin_module_t *) &default_dashboard_plugin_module;
    }
    if (strcmp(DEFAULT_DUMMY_PLUGIN_NAME, plugin_name) == 0) {
        instance->module = (neu_plugin_module_t *) &default_dummy_plugin_module;
    }
}

void neu_plugin_manager_destroy_instance(neu_plugin_manager_t * mgr,
                                         neu_plugin_instance_t *instance)
{
    (void) mgr;
    nlog_notice("destroy plugin instance: %s, handle: %p",
                instance->module->module_name, instance->handle);
    if (instance->handle != NULL) {
        dlclose(instance->handle);
    }
}

static void entity_free(plugin_entity_t *entity)
{
    nlog_notice("del plugin, name: %s, library: %s, kind: %d, type: %d",
                entity->name, entity->lib_name, entity->kind, entity->type);
    free(entity->name);
    free(entity->lib_name);
    free(entity->description);
    free(entity);
}
