/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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
 * @file template_manager.c
 * @brief 模板管理器实现
 *
 * 本文件实现了Neuron系统的模板管理功能，管理设备模板的创建、查找、删除等操作。
 * 模板是一种可重用的数据点配置集合，用于快速创建具有相同数据点结构的设备实例。
 * 模板管理器维护了一个基于名称的模板映射表，提供高效的模板查询和管理功能。
 */

#include <dlfcn.h>

#include "base/template.h"
#include "errcodes.h"
#include "utils/uthash.h"

#include "template_manager.h"

/**
 * @brief 模板名称映射条目
 *
 * 用于在哈希表中存储模板和相关的插件实例
 */
typedef struct {
    neu_template_t        *tmpl;    /**< 模板对象 */
    neu_plugin_instance_t *inst;    /**< 关联的插件实例 */
    UT_hash_handle         hh_name; /**< UT哈希句柄，用于名称索引 */
} name_entry_t;

/**
 * @brief 释放插件实例资源
 *
 * @param inst 插件实例指针
 */
static inline void free_plugin_instance(neu_plugin_instance_t *inst)
{
    dlclose(inst->handle); // 关闭动态链接库句柄
    free(inst);            // 释放实例内存
}

/**
 * @brief 释放模板条目资源
 *
 * 释放模板条目及其关联的模板和插件实例资源
 *
 * @param ent 模板条目指针
 */
static inline void template_entry_free(name_entry_t *ent)
{
    neu_template_free(ent->tmpl);    // 释放模板
    free_plugin_instance(ent->inst); // 释放关联的插件实例
    free(ent);                       // 释放条目本身
}

/**
 * @brief 模板管理器结构体
 *
 * 管理所有模板的核心数据结构，通过哈希表实现快速查找
 */
struct neu_template_manager_s {
    name_entry_t *name_map; /**< 基于名称的模板映射哈希表 */
};

/**
 * @brief 创建模板管理器
 *
 * 分配并初始化一个新的模板管理器实例
 *
 * @return 新创建的模板管理器指针，内存不足时可能返回NULL
 */
neu_template_manager_t *neu_template_manager_create()
{
    neu_template_manager_t *mgr = calloc(1, sizeof(*mgr));
    return mgr;
}

/**
 * @brief 销毁模板管理器
 *
 * 释放模板管理器及其管理的所有模板资源
 *
 * @param mgr 要销毁的模板管理器指针
 */
void neu_template_manager_destroy(neu_template_manager_t *mgr)
{
    if (NULL == mgr) {
        return;
    }

    // 清除所有模板
    neu_template_manager_clear(mgr);

    // 释放管理器本身
    free(mgr);
}

/**
 * @brief 添加模板到模板管理器
 *
 * 将新创建的模板及其关联的插件实例添加到模板管理器中
 *
 * @param mgr 模板管理器指针
 * @param tmpl 模板指针
 * @param inst 关联的插件实例指针
 * @return 成功返回0，模板已存在返回NEU_ERR_TEMPLATE_EXIST，
 *         内存分配失败返回NEU_ERR_EINTERNAL
 */
int neu_template_manager_add(neu_template_manager_t *mgr, neu_template_t *tmpl,
                             neu_plugin_instance_t *inst)
{
    name_entry_t *ent  = NULL;
    const char   *name = neu_template_name(tmpl);

    // 检查模板名称是否已存在
    HASH_FIND(hh_name, mgr->name_map, name, strlen(name), ent);
    if (ent) {
        // 如果已存在，释放资源并返回错误
        neu_template_free(tmpl);
        free_plugin_instance(inst);
        return NEU_ERR_TEMPLATE_EXIST;
    }

    // 创建新的条目
    ent = calloc(1, sizeof(*ent));
    if (NULL == ent) {
        // 内存分配失败，释放资源并返回错误
        neu_template_free(tmpl);
        free_plugin_instance(inst);
        return NEU_ERR_EINTERNAL;
    }

    // 设置条目数据
    ent->tmpl = tmpl;
    ent->inst = inst;
    // 添加到哈希表
    HASH_ADD_KEYPTR(hh_name, mgr->name_map, name, strlen(name), ent);
    return 0;
}

/**
 * @brief 从模板管理器中删除模板
 *
 * 根据模板名称从模板管理器中删除模板及其关联资源
 *
 * @param mgr 模板管理器指针
 * @param name 要删除的模板名称
 * @return 成功返回0，模板不存在返回NEU_ERR_TEMPLATE_NOT_FOUND
 */
int neu_template_manager_del(neu_template_manager_t *mgr, const char *name)
{
    name_entry_t *ent = NULL;
    // 查找指定名称的模板
    HASH_FIND(hh_name, mgr->name_map, name, strlen(name), ent);
    if (NULL == ent) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    // 从哈希表中删除
    HASH_DELETE(hh_name, mgr->name_map, ent);
    // 释放条目资源
    template_entry_free(ent);
    return 0;
}

/**
 * @brief 获取模板管理器中的模板数量
 *
 * @param mgr 模板管理器指针
 * @return 管理器中模板的数量
 */
int neu_template_manager_count(const neu_template_manager_t *mgr)
{
    return HASH_CNT(hh_name, mgr->name_map);
}

/**
 * @brief 根据名称查找模板
 *
 * @param mgr 模板管理器指针
 * @param name 要查找的模板名称
 * @return 如果找到返回模板指针，否则返回NULL
 */
neu_template_t *neu_template_manager_find(const neu_template_manager_t *mgr,
                                          const char                   *name)
{
    name_entry_t *ent = NULL;
    // 在哈希表中查找指定名称的模板
    HASH_FIND(hh_name, mgr->name_map, name, strlen(name), ent);
    return ent ? ent->tmpl : NULL;
}

/**
 * @brief 在指定模板中查找组
 *
 * 根据模板名称和组名称查找特定的组
 *
 * @param mgr 模板管理器指针
 * @param name 模板名称
 * @param group_name 组名称
 * @param group_p 如果不为NULL，成功时将设置为找到的组指针
 * @return 成功返回0，模板不存在返回NEU_ERR_TEMPLATE_NOT_FOUND，
 *         组不存在返回NEU_ERR_GROUP_NOT_EXIST
 */
int neu_template_manager_find_group(const neu_template_manager_t *mgr,
                                    const char *name, const char *group_name,
                                    neu_group_t **group_p)
{
    // 查找模板
    neu_template_t *tmpl = neu_template_manager_find(mgr, name);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    // 在模板中查找组
    neu_group_t *grp = neu_template_get_group(tmpl, group_name);
    if (NULL == grp) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    // 如果提供了输出参数，设置找到的组
    if (group_p) {
        *group_p = grp;
    }

    return 0;
}

/**
 * @brief 遍历所有模板并执行回调函数
 *
 * 对模板管理器中的每个模板执行指定的回调函数
 *
 * @param mgr 模板管理器指针
 * @param cb 要执行的回调函数，接收模板指针和用户数据指针作为参数
 * @param data 传递给回调函数的用户数据指针
 * @return 如果所有回调都成功返回0，否则返回第一个非零的回调返回值
 */
int neu_template_manager_for_each(neu_template_manager_t *mgr,
                                  int (*cb)(neu_template_t *tmpl, void *data),
                                  void *data)
{
    int           rv  = 0;
    name_entry_t *ent = NULL, *tmp = NULL;

    // 遍历哈希表中的所有模板
    HASH_ITER(hh_name, mgr->name_map, ent, tmp)
    {
        // 对每个模板执行回调函数
        if (0 != (rv = cb(ent->tmpl, data))) {
            break; // 如果回调返回非零值，中断遍历
        }
    }

    return rv;
}

/**
 * @brief 清除模板管理器中的所有模板
 *
 * 删除并释放模板管理器中的所有模板及其关联资源
 *
 * @param mgr 模板管理器指针
 */
void neu_template_manager_clear(neu_template_manager_t *mgr)
{
    name_entry_t *ent = NULL, *tmp = NULL;
    // 遍历哈希表中的所有模板
    HASH_ITER(hh_name, mgr->name_map, ent, tmp)
    {
        // 从哈希表中删除
        HASH_DELETE(hh_name, mgr->name_map, ent);
        // 释放条目资源
        template_entry_free(ent);
    }
}
