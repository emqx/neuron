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
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <unistd.h>

#include "errcodes.h"
#include "utils/log.h"
#include "utils/utextend.h"

#include "adapter.h"
#include "plugin.h"

#include "restful/rest.h"

#include "argparse.h"
#include "plugin_manager.h"

typedef struct plugin_entity {
    char *schema;
    char *name;
    char *lib_name;
    char *description;
    char *description_zh;

    neu_plugin_kind_e kind;
    neu_node_type_e   type;
    uint32_t          version;

    bool display;

    bool  single;
    char *single_name;

    UT_hash_handle hh;
} plugin_entity_t;

struct neu_plugin_manager {
    plugin_entity_t *plugins;
};

static void entity_free(plugin_entity_t *entity);

neu_plugin_manager_t *neu_plugin_manager_create()
{
    neu_plugin_manager_t *manager = calloc(1, sizeof(neu_plugin_manager_t));

    return manager;
}

void neu_plugin_manager_destroy(neu_plugin_manager_t *mgr)
{
    plugin_entity_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, mgr->plugins, el, tmp)
    {
        HASH_DEL(mgr->plugins, el);
        entity_free(el);
    }

    free(mgr);
}

int neu_plugin_manager_add(neu_plugin_manager_t *mgr,
                           const char *          plugin_lib_name)
{
    char                 lib_path[256]    = { 0 };
    void *               handle           = NULL;
    void *               module           = NULL;
    neu_plugin_module_t *pm               = NULL;
    plugin_entity_t *    plugin           = NULL;
    char                 lib_paths[3][64] = { 0 };
    snprintf(lib_paths[0], sizeof(lib_paths[0]), "%s", g_plugin_dir);
    snprintf(lib_paths[1], sizeof(lib_paths[1]), "%s/system", g_plugin_dir);
    snprintf(lib_paths[2], sizeof(lib_paths[2]), "%s/custom", g_plugin_dir);

    assert(strlen(plugin_lib_name) <= NEU_PLUGIN_LIBRARY_LEN);

    for (size_t i = 0; i < sizeof(lib_paths) / sizeof(lib_paths[0]); i++) {

        snprintf(lib_path, sizeof(lib_path) - 1, "%s/%s", lib_paths[i],
                 plugin_lib_name);

        handle = dlopen(lib_path, RTLD_NOW);

        if (handle == NULL) {
            nlog_warn("failed to open library %s, error: %s", lib_path,
                      dlerror());
        } else {
            break;
        }
    }

    if (handle == NULL) {
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

    uint32_t major = NEU_GET_VERSION_MAJOR(pm->version);
    uint32_t minor = NEU_GET_VERSION_MINOR(pm->version);

    if (NEU_VERSION_MAJOR != major || NEU_VERSION_MINOR != minor) {
        nlog_warn("library %s plugin version error, major:%d minor:%d",
                  lib_path, major, minor);
        return NEU_ERR_LIBRARY_MODULE_VERSION_NOT_MATCH;
    }

    HASH_FIND_STR(mgr->plugins, pm->module_name, plugin);
    if (plugin != NULL) {
        dlclose(handle);
        return NEU_ERR_LIBRARY_NAME_CONFLICT;
    }
    plugin = calloc(1, sizeof(plugin_entity_t));

    plugin->version        = pm->version;
    plugin->display        = pm->display;
    plugin->type           = pm->type;
    plugin->kind           = pm->kind;
    plugin->schema         = strdup(pm->schema);
    plugin->name           = strdup(pm->module_name);
    plugin->lib_name       = strdup(plugin_lib_name);
    plugin->description    = strdup(pm->module_descr);
    plugin->description_zh = strdup(pm->module_descr_zh);
    plugin->single         = pm->single;
    if (plugin->single) {
        plugin->single_name = strdup(pm->single_name);
    }

    HASH_ADD_STR(mgr->plugins, name, plugin);

    nlog_notice("add plugin, name: %s, library: %s, kind: %d, type: %d",
                plugin->name, plugin->lib_name, plugin->kind, plugin->type);

    dlclose(handle);
    return NEU_ERR_SUCCESS;
}

int neu_plugin_manager_update(neu_plugin_manager_t *mgr,
                              const char *          plugin_lib_name)
{
    char                 lib_path[256]    = { 0 };
    void *               handle           = NULL;
    void *               module           = NULL;
    neu_plugin_module_t *pm               = NULL;
    plugin_entity_t *    plugin           = NULL;
    char                 lib_paths[3][64] = { 0 };
    snprintf(lib_paths[0], sizeof(lib_paths[0]), "%s", g_plugin_dir);
    snprintf(lib_paths[1], sizeof(lib_paths[1]), "%s/system", g_plugin_dir);
    snprintf(lib_paths[2], sizeof(lib_paths[2]), "%s/custom", g_plugin_dir);

    assert(strlen(plugin_lib_name) <= NEU_PLUGIN_LIBRARY_LEN);

    for (size_t i = 0; i < sizeof(lib_paths) / sizeof(lib_paths[0]); i++) {

        snprintf(lib_path, sizeof(lib_path) - 1, "%s/%s", lib_paths[i],
                 plugin_lib_name);

        handle = dlopen(lib_path, RTLD_NOW);

        if (handle == NULL) {
            nlog_warn("failed to open library %s, error: %s", lib_path,
                      dlerror());
        } else {
            break;
        }
    }

    if (handle == NULL) {
        return NEU_ERR_LIBRARY_FAILED_TO_OPEN;
    }

    module = dlsym(handle, "neu_plugin_module");
    if (module == NULL) {
        dlclose(handle);
        nlog_warn("failed to find neu_plugin_module symbol in %s", lib_path);
        return NEU_ERR_LIBRARY_MODULE_INVALID;
    }

    pm = (neu_plugin_module_t *) module;

    HASH_FIND_STR(mgr->plugins, pm->module_name, plugin);
    if (plugin == NULL) {
        dlclose(handle);
        return NEU_ERR_LIBRARY_MODULE_NOT_EXISTS;
    }

    plugin->version = pm->version;
    plugin->display = pm->display;
    plugin->type    = pm->type;
    plugin->kind    = pm->kind;
    free(plugin->schema);
    free(plugin->description);
    free(plugin->description_zh);
    plugin->schema         = strdup(pm->schema);
    plugin->description    = strdup(pm->module_descr);
    plugin->description_zh = strdup(pm->module_descr_zh);
    plugin->single         = pm->single;
    if (plugin->single) {
        free(plugin->single_name);
        plugin->single_name = strdup(pm->single_name);
    }

    nlog_notice("update plugin, name: %s, library: %s, kind: %d, type: %d",
                plugin->name, plugin->lib_name, plugin->kind, plugin->type);

    dlclose(handle);
    return NEU_ERR_SUCCESS;
}

int neu_plugin_manager_del(neu_plugin_manager_t *mgr, const char *plugin_name)
{
    plugin_entity_t *plugin = NULL;
    int              ret    = NEU_ERR_LIBRARY_SYSTEM_NOT_ALLOW_DEL;

    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    if (plugin != NULL) {
        if (plugin->kind != NEU_PLUGIN_KIND_SYSTEM) {

            char so_file_path[128]     = { 0 };
            char schema_file_path[128] = { 0 };
            snprintf(so_file_path, sizeof(so_file_path), "%s/custom/%s",
                     g_plugin_dir, plugin->lib_name);
            snprintf(schema_file_path, sizeof(schema_file_path),
                     "%s/custom/schema/%s.json", g_plugin_dir, plugin->schema);
            if (remove(so_file_path) != 0) {
                nlog_error("rm %s file fail!", so_file_path);
            }
            if (remove(schema_file_path) != 0) {
                nlog_error("rm %s file fail!", schema_file_path);
            }

            HASH_DEL(mgr->plugins, plugin);
            entity_free(plugin);
            ret = NEU_ERR_SUCCESS;
        }
    }

    return ret;
}

UT_array *neu_plugin_manager_get(neu_plugin_manager_t *mgr)
{
    UT_array *       plugins;
    UT_icd           icd = { sizeof(neu_resp_plugin_info_t), NULL, NULL, NULL };
    plugin_entity_t *el = NULL, *tmp = NULL;

    utarray_new(plugins, &icd);
    HASH_ITER(hh, mgr->plugins, el, tmp)
    {
        if (el->single) {
            continue;
        }
        neu_resp_plugin_info_t info = {
            .kind    = el->kind,
            .type    = el->type,
            .version = el->version,
        };

        info.display = el->display;
        strncpy(info.schema, el->schema, sizeof(info.schema));
        strncpy(info.name, el->name, sizeof(info.name));
        strncpy(info.library, el->lib_name, sizeof(info.library));
        strncpy(info.description, el->description, sizeof(info.description));
        strncpy(info.description_zh, el->description_zh,
                sizeof(info.description_zh));

        utarray_push_back(plugins, &info);
    }

    return plugins;
}

UT_array *neu_plugin_manager_get_single(neu_plugin_manager_t *mgr)
{
    UT_array *       plugins;
    UT_icd           icd = { sizeof(neu_resp_plugin_info_t), NULL, NULL, NULL };
    plugin_entity_t *el = NULL, *tmp = NULL;

    utarray_new(plugins, &icd);
    HASH_ITER(hh, mgr->plugins, el, tmp)
    {
        if (el->single) {
            neu_resp_plugin_info_t info = {
                .kind = el->kind,
                .type = el->type,
            };

            info.display = el->display;
            info.single  = el->single;

            strncpy(info.schema, el->schema, sizeof(info.schema));
            strncpy(info.single_name, el->single_name,
                    sizeof(info.single_name));
            strncpy(info.name, el->name, sizeof(info.name));
            strncpy(info.library, el->lib_name, sizeof(info.library));
            strncpy(info.description, el->description,
                    sizeof(info.description));
            strncpy(info.description_zh, el->description_zh,
                    sizeof(info.description_zh));

            utarray_push_back(plugins, &info);
        }
    }

    return plugins;
}

int neu_plugin_manager_find(neu_plugin_manager_t *mgr, const char *plugin_name,
                            neu_resp_plugin_info_t *info)
{
    plugin_entity_t *plugin = NULL;
    int              ret    = -1;

    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    if (plugin != NULL) {
        ret           = 0;
        info->single  = plugin->single;
        info->display = plugin->display;
        info->type    = plugin->type;
        info->kind    = plugin->kind;

        if (plugin->single_name != NULL) {
            strcpy(info->single_name, plugin->single_name);
        }
        strncpy(info->name, plugin->name, sizeof(info->name));
        strncpy(info->library, plugin->lib_name, sizeof(info->library));
        strncpy(info->description, plugin->description,
                sizeof(info->description));
    }

    return ret;
}

bool neu_plugin_manager_exists(neu_plugin_manager_t *mgr,
                               const char *          plugin_name)
{
    plugin_entity_t *plugin = NULL;
    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    return NULL != plugin;
}

bool neu_plugin_manager_is_single(neu_plugin_manager_t *mgr,
                                  const char *          plugin_name)
{
    plugin_entity_t *plugin = NULL;
    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);

    if (NULL == plugin) {
        return false;
    }

    return plugin->single;
}

int neu_plugin_manager_create_instance(neu_plugin_manager_t * mgr,
                                       const char *           plugin_name,
                                       neu_plugin_instance_t *instance)
{
    plugin_entity_t *plugin           = NULL;
    char             lib_paths[3][64] = { 0 };
    snprintf(lib_paths[0], sizeof(lib_paths[0]), "%s", g_plugin_dir);
    snprintf(lib_paths[1], sizeof(lib_paths[1]), "%s/system", g_plugin_dir);
    snprintf(lib_paths[2], sizeof(lib_paths[2]), "%s/custom", g_plugin_dir);

    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    if (plugin != NULL) {

        for (size_t i = 0; i < sizeof(lib_paths) / sizeof(lib_paths[0]); i++) {
            char lib_path[256] = { 0 };

            snprintf(lib_path, sizeof(lib_path) - 1, "%s/%s", lib_paths[i],
                     plugin->lib_name);
            instance->handle = dlopen(lib_path, RTLD_NOW);

            if (instance->handle == NULL) {
                continue;
            }

            instance->module = (neu_plugin_module_t *) dlsym(
                instance->handle, "neu_plugin_module");
            assert(instance->module != NULL);
            return 0;
        }
    }

    assert(instance->handle != NULL);

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
    if (entity->single) {
        free(entity->single_name);
    }
    free(entity->schema);
    free(entity->name);
    free(entity->lib_name);
    free(entity->description);
    free(entity->description_zh);
    free(entity);
}

bool neu_plugin_manager_create_instance_by_path(neu_plugin_manager_t *mgr,
                                                const char *plugin_path,
                                                neu_plugin_instance_t *instance,
                                                int *                  error)
{

    (void) mgr;

    instance->handle = dlopen(plugin_path, RTLD_NOW);

    if (instance->handle == NULL) {
        const char *dl_err = dlerror();
        if (strstr(dl_err, "cannot open shared object file") != NULL) {
            *error = NEU_ERR_LIBRARY_ARCH_NOT_SUPPORT;
        } else if (strstr(dl_err, "GLIBC") != NULL &&
                   strstr(dl_err, "not found") != NULL) {
            *error = NEU_ERR_LIBRARY_CLIB_NOT_MATCH;
        } else {
            *error = NEU_ERR_LIBRARY_MODULE_INVALID;
        }

        nlog_debug("create instance error:%s", dl_err);
        (void) dlerror();
        return false;
    }

    instance->module =
        (neu_plugin_module_t *) dlsym(instance->handle, "neu_plugin_module");

    if (instance->module == NULL) {
        dlclose(instance->handle);
        *error           = NEU_ERR_LIBRARY_MODULE_INVALID;
        instance->handle = NULL;
        return false;
    }

    return true;
}

bool neu_plugin_manager_create_instance_by_lib_name(
    neu_plugin_manager_t *mgr, const char *lib_name,
    neu_plugin_instance_t *instance)
{
    (void) mgr;
    char lib_paths[3][64] = { 0 };
    snprintf(lib_paths[0], sizeof(lib_paths[0]), "%s", g_plugin_dir);
    snprintf(lib_paths[1], sizeof(lib_paths[1]), "%s/system", g_plugin_dir);
    snprintf(lib_paths[2], sizeof(lib_paths[2]), "%s/custom", g_plugin_dir);

    for (size_t i = 0; i < sizeof(lib_paths) / sizeof(lib_paths[0]); i++) {
        char lib_path[256] = { 0 };

        snprintf(lib_path, sizeof(lib_path) - 1, "%s/%s", lib_paths[i],
                 lib_name);
        instance->handle = dlopen(lib_path, RTLD_NOW);

        if (instance->handle == NULL) {
            continue;
        }

        instance->module = (neu_plugin_module_t *) dlsym(instance->handle,
                                                         "neu_plugin_module");
        assert(instance->module != NULL);
        return true;
    }

    assert(instance->handle != NULL);

    return false;
}

bool neu_plugin_manager_remove_library(neu_plugin_manager_t *mgr,
                                       const char *          library)
{
    bool ret = true;
    (void) mgr;
    char lib_paths[3][64] = { 0 };
    snprintf(lib_paths[0], sizeof(lib_paths[0]), "%s", g_plugin_dir);
    snprintf(lib_paths[1], sizeof(lib_paths[1]), "%s/system", g_plugin_dir);
    snprintf(lib_paths[2], sizeof(lib_paths[2]), "%s/custom", g_plugin_dir);
    char lib_path[256] = { 0 };
    for (size_t i = 0; i < sizeof(lib_paths) / sizeof(lib_paths[0]); i++) {
        snprintf(lib_path, sizeof(lib_path), "%s/%s", lib_paths[i], library);
        if (access(lib_path, F_OK) != -1 && remove(lib_path) != 0) {
            nlog_debug("library %s remove fail", lib_path);
            ret = false;
            break;
        }
    }

    return ret;
}
