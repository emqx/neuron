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

/**
 * @file plugin_manager.c
 * @brief 插件管理器实现
 *
 * 本文件实现了Neuron系统的插件管理器，负责管理系统中的所有插件。
 * 插件管理器提供插件的加载、查找、创建实例和删除等功能。
 */
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>

#include "errcodes.h"
#include "utils/log.h"
#include "utils/utextend.h"

#include "adapter.h"
#include "plugin.h"

#include "restful/rest.h"

#include "argparse.h"
#include "plugin_manager.h"

/**
 * @brief 插件实体结构
 *
 * 表示系统中的一个插件实体，包含插件名称、库名称和描述信息等
 */
typedef struct plugin_entity {
    char *schema;         ///< 插件模式
    char *name;           ///< 插件名称
    char *lib_name;       ///< 插件库文件名
    char *description;    ///< 插件英文描述
    char *description_zh; ///< 插件中文描述

    neu_plugin_kind_e kind; ///< 插件类型（系统或自定义）
    neu_node_type_e   type; ///< 节点类型（驱动或应用）

    bool display; ///< 是否在界面上显示

    bool  single;      ///< 是否为单例插件
    char *single_name; ///< 单例插件名称

    UT_hash_handle hh; ///< 哈希表句柄，用于uthash
} plugin_entity_t;

/**
 * @brief 插件管理器结构
 *
 * 管理系统中的所有插件实体
 */
struct neu_plugin_manager {
    plugin_entity_t *plugins; ///< 插件哈希表
};

/**
 * @brief 释放插件实体资源
 *
 * @param entity 插件实体指针
 */
static void entity_free(plugin_entity_t *entity);

/**
 * @brief 创建插件管理器
 *
 * 分配并初始化插件管理器对象
 *
 * @return 返回新创建的插件管理器对象，失败则返回NULL
 */
neu_plugin_manager_t *neu_plugin_manager_create()
{
    neu_plugin_manager_t *manager = calloc(1, sizeof(neu_plugin_manager_t));

    return manager;
}

/**
 * @brief 销毁插件管理器
 *
 * 释放插件管理器及其管理的所有插件资源
 *
 * @param mgr 插件管理器对象
 */
void neu_plugin_manager_destroy(neu_plugin_manager_t *mgr)
{
    plugin_entity_t *el = NULL, *tmp = NULL;

    // 遍历并释放所有插件实体
    HASH_ITER(hh, mgr->plugins, el, tmp)
    {
        HASH_DEL(mgr->plugins, el);
        entity_free(el);
    }

    free(mgr);
}

/**
 * @brief 添加插件
 *
 * 加载指定的插件库，并添加到插件管理器中
 *
 * @param mgr 插件管理器对象
 * @param plugin_lib_name 插件库文件名
 * @return 成功返回NEU_ERR_SUCCESS，失败返回错误码
 */
int neu_plugin_manager_add(neu_plugin_manager_t *mgr,
                           const char           *plugin_lib_name)
{
    char                 lib_path[256] = { 0 };
    void                *handle        = NULL;
    void                *module        = NULL;
    neu_plugin_module_t *pm            = NULL;
    plugin_entity_t     *plugin        = NULL;

    assert(strlen(plugin_lib_name) <= NEU_PLUGIN_LIBRARY_LEN);
    // 构建插件库路径
    snprintf(lib_path, sizeof(lib_path) - 1, "./plugins/%s", plugin_lib_name);

    // 加载动态库
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

    if (pm->type != NEU_NA_TYPE_APP && pm->type != NEU_NA_TYPE_NDRIVER &&
        pm->type != NEU_NA_TYPE_DRIVER) {
        dlclose(handle);
        nlog_warn("library: %s, type wrong: %d", lib_path, pm->type);
        return NEU_ERR_LIBRARY_INFO_INVALID;
    }

    HASH_FIND_STR(mgr->plugins, pm->module_name, plugin);
    if (plugin != NULL) {
        dlclose(handle);
        return NEU_ERR_LIBRARY_NAME_CONFLICT;
    }
    plugin = calloc(1, sizeof(plugin_entity_t));

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

/**
 * @brief 删除插件
 *
 * 从插件管理器中删除指定的插件，系统插件不允许删除
 *
 * @param mgr 插件管理器对象
 * @param plugin_name 插件名称
 * @return
 * 成功返回NEU_ERR_SUCCESS，系统插件返回NEU_ERR_LIBRARY_SYSTEM_NOT_ALLOW_DEL
 */
int neu_plugin_manager_del(neu_plugin_manager_t *mgr, const char *plugin_name)
{
    plugin_entity_t *plugin = NULL;
    int              ret    = NEU_ERR_LIBRARY_SYSTEM_NOT_ALLOW_DEL;

    // 查找插件
    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    if (plugin != NULL) {
        // 只允许删除非系统插件
        if (plugin->kind != NEU_PLUGIN_KIND_SYSTEM) {
            HASH_DEL(mgr->plugins, plugin);
            entity_free(plugin);
            ret = NEU_ERR_SUCCESS;
        }
    }

    return ret;
}

/**
 * @brief 获取非单例插件列表
 *
 * 获取系统中所有非单例插件的信息列表
 *
 * @param mgr 插件管理器对象
 * @return 非单例插件信息数组
 */
UT_array *neu_plugin_manager_get(neu_plugin_manager_t *mgr)
{
    UT_array        *plugins;
    UT_icd           icd = { sizeof(neu_resp_plugin_info_t), NULL, NULL, NULL };
    plugin_entity_t *el = NULL, *tmp = NULL;

    utarray_new(plugins, &icd);
    HASH_ITER(hh, mgr->plugins, el, tmp)
    {
        // 跳过单例插件
        if (el->single) {
            continue;
        }
        neu_resp_plugin_info_t info = {
            .kind = el->kind,
            .type = el->type,
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

/**
 * @brief 获取单例插件列表
 *
 * 获取系统中所有单例插件的信息列表
 *
 * @param mgr 插件管理器对象
 * @return 单例插件信息数组
 */
UT_array *neu_plugin_manager_get_single(neu_plugin_manager_t *mgr)
{
    UT_array        *plugins;
    UT_icd           icd = { sizeof(neu_resp_plugin_info_t), NULL, NULL, NULL };
    plugin_entity_t *el = NULL, *tmp = NULL;

    utarray_new(plugins, &icd);
    HASH_ITER(hh, mgr->plugins, el, tmp)
    {
        // 只处理单例插件
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

/**
 * @brief 查找插件
 *
 * 根据插件名称查找插件信息
 *
 * @param mgr 插件管理器对象
 * @param plugin_name 插件名称
 * @param info 用于返回插件信息的结构体指针
 * @return 成功返回0，失败返回-1
 */
int neu_plugin_manager_find(neu_plugin_manager_t *mgr, const char *plugin_name,
                            neu_resp_plugin_info_t *info)
{
    plugin_entity_t *plugin = NULL;
    int              ret    = -1;

    // 查找插件
    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    if (plugin != NULL) {
        ret           = 0;
        info->single  = plugin->single;
        info->display = plugin->display;
        info->type    = plugin->type;
        info->kind    = plugin->kind;

        // 如果是单例插件，复制单例名称
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

/**
 * @brief 检查插件是否存在
 *
 * 判断指定名称的插件是否存在
 *
 * @param mgr 插件管理器对象
 * @param plugin_name 插件名称
 * @return 如果插件存在返回true，否则返回false
 */
bool neu_plugin_manager_exists(neu_plugin_manager_t *mgr,
                               const char           *plugin_name)
{
    plugin_entity_t *plugin = NULL;
    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    return NULL != plugin;
}

/**
 * @brief 检查插件是否为单例插件
 *
 * 判断指定插件是否为单例插件
 *
 * @param mgr 插件管理器对象
 * @param plugin_name 插件名称
 * @return 如果是单例插件返回true，否则返回false
 */
bool neu_plugin_manager_is_single(neu_plugin_manager_t *mgr,
                                  const char           *plugin_name)
{
    plugin_entity_t *plugin = NULL;
    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);

    if (NULL == plugin) {
        return false;
    }

    return plugin->single;
}

/**
 * @brief 创建插件实例
 *
 * 根据插件名称加载插件库并创建插件实例
 *
 * @param mgr 插件管理器对象
 * @param plugin_name 插件名称
 * @param instance 用于返回插件实例的指针
 * @return 成功返回0，失败返回-1
 */
int neu_plugin_manager_create_instance(neu_plugin_manager_t  *mgr,
                                       const char            *plugin_name,
                                       neu_plugin_instance_t *instance)
{
    plugin_entity_t *plugin = NULL;

    // 查找插件
    HASH_FIND_STR(mgr->plugins, plugin_name, plugin);
    if (plugin != NULL) {
        char lib_path[256] = { 0 };

        // 构建插件库路径
        snprintf(lib_path, sizeof(lib_path) - 1, "%s/%s", g_plugin_dir,
                 plugin->lib_name);
        // 加载动态库
        instance->handle = dlopen(lib_path, RTLD_NOW | RTLD_NODELETE);
        assert(instance->handle != NULL);

        // 获取插件模块指针
        instance->module = (neu_plugin_module_t *) dlsym(instance->handle,
                                                         "neu_plugin_module");
        assert(instance->module != NULL);
        return 0;
    }

    return -1;
}

/**
 * @brief 加载静态插件
 *
 * 加载系统内置的静态插件（不需要动态库）
 *
 * @param mgr 插件管理器对象（未使用）
 * @param plugin_name 插件名称
 * @param instance 用于返回插件实例的指针
 */
void neu_plugin_manager_load_static(neu_plugin_manager_t  *mgr,
                                    const char            *plugin_name,
                                    neu_plugin_instance_t *instance)
{
    (void) mgr;
    instance->handle = NULL;

    // 为仪表板插件设置静态模块
    if (strcmp(DEFAULT_DASHBOARD_PLUGIN_NAME, plugin_name) == 0) {
        instance->module =
            (neu_plugin_module_t *) &default_dashboard_plugin_module;
    }
}

/**
 * @brief 销毁插件实例
 *
 * 关闭插件动态库，释放插件实例资源
 *
 * @param mgr 插件管理器对象（未使用）
 * @param instance 要销毁的插件实例
 */
void neu_plugin_manager_destroy_instance(neu_plugin_manager_t  *mgr,
                                         neu_plugin_instance_t *instance)
{
    (void) mgr;
    nlog_notice("destroy plugin instance: %s, handle: %p",
                instance->module->module_name, instance->handle);
    // 如果有动态库句柄，关闭动态库
    if (instance->handle != NULL) {
        dlclose(instance->handle);
    }
}

/**
 * @brief 释放插件实体资源
 *
 * 释放插件实体结构体及其所有分配的内存
 *
 * @param entity 插件实体指针
 */
static void entity_free(plugin_entity_t *entity)
{
    nlog_notice("del plugin, name: %s, library: %s, kind: %d, type: %d",
                entity->name, entity->lib_name, entity->kind, entity->type);
    // 释放单例插件名称
    if (entity->single) {
        free(entity->single_name);
    }
    // 释放所有字符串资源
    free(entity->schema);
    free(entity->name);
    free(entity->lib_name);
    free(entity->description);
    free(entity->description_zh);
    free(entity);
}
