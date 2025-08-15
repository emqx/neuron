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
 * @file subscribe.c
 * @brief 订阅管理模块，负责管理Neuron系统中的订阅关系
 *
 * 该文件实现了订阅管理器，用于管理应用节点对驱动节点数据组的订阅关系。
 * 订阅管理器提供了创建、查找、添加、更新和删除订阅关系的功能。
 * 订阅关系是Neuron系统中数据流转的基础，应用节点通过订阅驱动节点的数据组来接收数据。
 */
#include <stdlib.h>

#include "adapter.h"
#include "errcodes.h"
#include "utils/log.h"

#include "subscribe.h"

/**
 * @brief 订阅元素键结构体
 *
 * 用于在哈希表中唯一标识一个数据源(驱动+组)
 */
typedef struct sub_elem_key {
    char driver[NEU_NODE_NAME_LEN]; /**< 驱动节点名称 */
    char group[NEU_GROUP_NAME_LEN]; /**< 组名称 */
} sub_elem_key_t;

/**
 * @brief 订阅元素结构体
 *
 * 表示一个数据源(驱动+组)的所有订阅应用信息
 */
typedef struct sub_elem {
    sub_elem_key_t key; /**< 订阅键(驱动+组) */

    UT_array *apps; /**< 订阅该数据源的应用列表 */

    UT_hash_handle hh; /**< UT哈希表句柄 */
} sub_elem_t;

/**
 * @brief 应用订阅数组元素操作描述符
 */
static const UT_icd app_sub_icd = { sizeof(neu_app_subscribe_t), NULL, NULL,
                                    NULL };

/**
 * @brief 订阅管理器结构体
 *
 * 管理所有订阅关系的核心数据结构
 */
struct neu_subscribe_mgr {
    sub_elem_t *ss; /**< 订阅元素哈希表 */
};

/**
 * @brief 创建订阅管理器
 *
 * 分配并初始化一个新的订阅管理器实例
 *
 * @return 新创建的订阅管理器指针
 */
neu_subscribe_mgr_t *neu_subscribe_manager_create()
{
    neu_subscribe_mgr_t *mgr = calloc(1, sizeof(neu_subscribe_mgr_t));

    return mgr;
}

/**
 * @brief 销毁订阅管理器
 *
 * 释放订阅管理器及其所有资源，包括所有订阅元素和应用订阅信息
 *
 * @param mgr 要销毁的订阅管理器指针
 */
void neu_subscribe_manager_destroy(neu_subscribe_mgr_t *mgr)
{
    sub_elem_t *el = NULL, *tmp = NULL;

    // 遍历哈希表中的所有元素
    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        // 从哈希表中删除当前元素
        HASH_DEL(mgr->ss, el);
        // 释放每个应用订阅的资源
        utarray_foreach(el->apps, neu_app_subscribe_t *, sub_app)
        {
            neu_app_subscribe_fini(sub_app);
        }
        // 释放应用数组
        utarray_free(el->apps);
        // 释放元素本身
        free(el);
    }

    // 释放管理器
    free(mgr);
}

/**
 * @brief 查找指定驱动和组的订阅应用
 *
 * 根据驱动名称和组名称查找所有订阅该数据源的应用
 *
 * @param mgr 订阅管理器指针
 * @param driver 驱动节点名称
 * @param group 组名称
 * @return 包含所有订阅应用的数组，如果没有找到则返回NULL
 * @note 返回的数组需要由调用者释放
 */
UT_array *neu_subscribe_manager_find(neu_subscribe_mgr_t *mgr,
                                     const char *driver, const char *group)
{
    sub_elem_t    *find = NULL;
    sub_elem_key_t key  = { 0 };

    // 构建查找键
    strncpy(key.driver, driver, sizeof(key.driver));
    strncpy(key.group, group, sizeof(key.group));

    // 在哈希表中查找
    HASH_FIND(hh, mgr->ss, &key, sizeof(sub_elem_key_t), find);

    if (find) {
        // 找到后返回应用列表的克隆
        return utarray_clone(find->apps);
    } else {
        return NULL;
    }
}

/**
 * @brief 查找指定驱动的所有订阅应用
 *
 * 根据驱动名称查找所有订阅该驱动任意组的应用
 *
 * @param mgr 订阅管理器指针
 * @param driver 驱动节点名称
 * @return 包含所有订阅应用的数组，即使没有找到也会返回空数组
 * @note 返回的数组需要由调用者释放
 */
UT_array *neu_subscribe_manager_find_by_driver(neu_subscribe_mgr_t *mgr,
                                               const char          *driver)
{
    sub_elem_t *el = NULL, *tmp = NULL;
    UT_array   *apps = NULL;

    // 创建新的应用数组
    utarray_new(apps, &app_sub_icd);

    // 遍历哈希表中的所有元素
    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        // 如果驱动名称匹配，将该元素的所有应用添加到结果数组
        if (strcmp(driver, el->key.driver) == 0) {
            utarray_concat(apps, el->apps);
        }
    }

    return apps;
}

/**
 * @brief 获取指定应用的所有订阅信息
 *
 * 根据应用名称获取其所有订阅的驱动和组信息
 *
 * @param mgr 订阅管理器指针
 * @param app 应用节点名称
 * @return 包含所有订阅信息的数组，即使没有找到也会返回空数组
 * @note 返回的数组需要由调用者释放
 */
UT_array *neu_subscribe_manager_get(neu_subscribe_mgr_t *mgr, const char *app)
{
    sub_elem_t *el = NULL, *tmp = NULL;
    UT_array   *groups = NULL;
    UT_icd      icd = { sizeof(neu_resp_subscribe_info_t), NULL, NULL, NULL };

    // 创建新的订阅信息数组
    utarray_new(groups, &icd);

    // 遍历哈希表中的所有元素
    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        // 检查每个元素中的应用列表
        utarray_foreach(el->apps, neu_app_subscribe_t *, sub_app)
        {
            // 如果找到匹配的应用名称
            if (strcmp(sub_app->app_name, app) == 0) {
                neu_resp_subscribe_info_t info = { 0 };

                // 填充订阅信息
                strncpy(info.driver, el->key.driver, sizeof(info.driver));
                strncpy(info.app, app, sizeof(info.app));
                strncpy(info.group, el->key.group, sizeof(info.group));
                info.params = sub_app->params; // 借用引用，不复制字符串

                // 添加到结果数组
                utarray_push_back(groups, &info);
            }
        }
    }

    return groups;
}

/**
 * @brief 获取北向驱动的映射关系
 *
 * 获取指定北向驱动与所有南向驱动组的映射关系
 *
 * @param mgr 订阅管理器指针
 * @param ndriver 北向驱动节点名称
 * @return 包含所有映射关系的数组，即使没有找到也会返回空数组
 * @note 返回的数组需要由调用者释放
 */
UT_array *neu_subscribe_manager_get_ndriver_maps(neu_subscribe_mgr_t *mgr,
                                                 const char          *ndriver)
{
    UT_array *groups = NULL;
    UT_icd    icd = { sizeof(neu_resp_ndriver_map_info_t), NULL, NULL, NULL };
    utarray_new(groups, &icd);

    sub_elem_t *el = NULL, *tmp = NULL;
    // 遍历哈希表中的所有元素
    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        // 检查每个元素中的应用列表
        utarray_foreach(el->apps, neu_app_subscribe_t *, sub_app)
        {
            // 如果找到匹配的北向驱动名称
            if (strcmp(sub_app->app_name, ndriver) == 0) {
                neu_resp_ndriver_map_info_t info = { 0 };
                // 填充映射信息
                strncpy(info.driver, el->key.driver, sizeof(info.driver));
                strncpy(info.group, el->key.group, sizeof(info.group));
                // 添加到结果数组
                utarray_push_back(groups, &info);
            }
        }
    }

    return groups;
}

/**
 * @brief 取消指定应用的所有订阅
 *
 * 取消指定应用对所有驱动和组的订阅关系
 *
 * @param mgr 订阅管理器指针
 * @param app 应用节点名称
 */
void neu_subscribe_manager_unsub_all(neu_subscribe_mgr_t *mgr, const char *app)
{
    sub_elem_t *el = NULL, *tmp = NULL;

    // 遍历哈希表中的所有元素
    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        // 检查每个元素中的应用列表
        utarray_foreach(el->apps, neu_app_subscribe_t *, sub_app)
        {
            // 如果找到匹配的应用名称
            if (strcmp(sub_app->app_name, app) == 0) {
                // 取消该应用对当前驱动和组的订阅
                neu_subscribe_manager_unsub(mgr, el->key.driver, app,
                                            el->key.group);
                break; // 取消后跳出内层循环，继续检查下一个元素
            }
        }
    }
}

/**
 * @brief 添加订阅关系
 *
 * 添加应用对驱动组的订阅关系
 *
 * @param mgr 订阅管理器指针
 * @param driver 驱动节点名称
 * @param app 应用节点名称
 * @param group 组名称
 * @param params 订阅参数，可以为NULL
 * @param addr 应用节点的套接字地址
 * @return 成功返回NEU_ERR_SUCCESS，已订阅返回NEU_ERR_GROUP_ALREADY_SUBSCRIBED，
 *         内存分配失败返回NEU_ERR_EINTERNAL
 */
int neu_subscribe_manager_sub(neu_subscribe_mgr_t *mgr, const char *driver,
                              const char *app, const char *group,
                              const char *params, struct sockaddr_in addr)
{
    sub_elem_t         *find    = NULL;
    sub_elem_key_t      key     = { 0 };
    neu_app_subscribe_t app_sub = { 0 };

    // 构建查找键和应用订阅信息
    strncpy(key.driver, driver, sizeof(key.driver));
    strncpy(key.group, group, sizeof(key.group));
    strncpy(app_sub.app_name, app, sizeof(app_sub.app_name));
    app_sub.addr = addr;

    // 如果有参数，复制参数字符串
    if (params && NULL == (app_sub.params = strdup(params))) {
        return NEU_ERR_EINTERNAL;
    }

    // 在哈希表中查找驱动和组
    HASH_FIND(hh, mgr->ss, &key, sizeof(sub_elem_key_t), find);

    if (find) {
        // 如果找到，检查应用是否已订阅
        utarray_foreach(find->apps, neu_app_subscribe_t *, sub)
        {
            if (strcmp(sub->app_name, app) == 0) {
                neu_app_subscribe_fini(&app_sub);
                return NEU_ERR_GROUP_ALREADY_SUBSCRIBED;
            }
        }
    } else {
        // 如果没有找到，创建新的订阅元素
        find = calloc(1, sizeof(sub_elem_t));
        utarray_new(find->apps, &app_sub_icd);
        find->key = key;
        HASH_ADD(hh, mgr->ss, key, sizeof(sub_elem_key_t), find);
    }

    // 添加应用订阅信息到列表
    utarray_push_back(find->apps, &app_sub);
    return NEU_ERR_SUCCESS;
}

/**
 * @brief 更新订阅参数
 *
 * 更新应用对驱动组订阅的参数
 *
 * @param mgr 订阅管理器指针
 * @param app 应用节点名称
 * @param driver 驱动节点名称
 * @param group 组名称
 * @param params 新的订阅参数，可以为NULL
 * @return 成功返回NEU_ERR_SUCCESS，未订阅返回NEU_ERR_GROUP_NOT_SUBSCRIBE，
 *         内存分配失败返回NEU_ERR_EINTERNAL
 */
int neu_subscribe_manager_update_params(neu_subscribe_mgr_t *mgr,
                                        const char *app, const char *driver,
                                        const char *group, const char *params)
{
    sub_elem_key_t key = { 0 };
    // 构建查找键
    strncpy(key.driver, driver, sizeof(key.driver));
    strncpy(key.group, group, sizeof(key.group));

    // 在哈希表中查找驱动和组
    sub_elem_t *find = NULL;
    HASH_FIND(hh, mgr->ss, &key, sizeof(sub_elem_key_t), find);

    if (NULL == find) {
        return NEU_ERR_GROUP_NOT_SUBSCRIBE;
    }

    // 查找指定的应用订阅
    neu_app_subscribe_t *app_sub = NULL;
    utarray_foreach(find->apps, neu_app_subscribe_t *, sub)
    {
        if (strcmp(sub->app_name, app) == 0) {
            app_sub = sub;
            break;
        }
    }

    if (NULL == app_sub) {
        return NEU_ERR_GROUP_NOT_SUBSCRIBE;
    }

    // 复制新的参数字符串
    char *p = NULL;
    if (params && NULL == (p = strdup(params))) {
        return NEU_ERR_EINTERNAL;
    }

    // 更新参数
    free(app_sub->params);
    app_sub->params = p;
    return NEU_ERR_SUCCESS;
}

/**
 * @brief 取消订阅关系
 *
 * 取消应用对驱动组的订阅关系
 *
 * @param mgr 订阅管理器指针
 * @param driver 驱动节点名称
 * @param app 应用节点名称
 * @param group 组名称
 * @return 成功返回NEU_ERR_SUCCESS，未订阅返回NEU_ERR_GROUP_NOT_SUBSCRIBE
 */
int neu_subscribe_manager_unsub(neu_subscribe_mgr_t *mgr, const char *driver,
                                const char *app, const char *group)
{
    sub_elem_t    *find = NULL;
    sub_elem_key_t key  = { 0 };

    // 构建查找键
    strncpy(key.driver, driver, sizeof(key.driver));
    strncpy(key.group, group, sizeof(key.group));

    // 在哈希表中查找驱动和组
    HASH_FIND(hh, mgr->ss, &key, sizeof(sub_elem_key_t), find);

    if (find) {
        // 查找指定的应用订阅
        utarray_foreach(find->apps, neu_app_subscribe_t *, sub)
        {
            if (strcmp(sub->app_name, app) == 0) {
                // 释放资源并从数组中删除
                neu_app_subscribe_fini(sub);
                utarray_erase(find->apps, utarray_eltidx(find->apps, sub), 1);
                return NEU_ERR_SUCCESS;
            }
        }
    }

    return NEU_ERR_GROUP_NOT_SUBSCRIBE;
}

/**
 * @brief 移除驱动的所有订阅关系
 *
 * 移除指定驱动的所有订阅关系，如果指定了组，则只移除该组的订阅关系
 *
 * @param mgr 订阅管理器指针
 * @param driver 驱动节点名称
 * @param group 组名称，如果为NULL则移除所有组
 */
void neu_subscribe_manager_remove(neu_subscribe_mgr_t *mgr, const char *driver,
                                  const char *group)
{
    sub_elem_t *el = NULL, *tmp = NULL;

    // 遍历哈希表中的所有元素
    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        // 如果驱动名称匹配，且组名称匹配或组为NULL
        if (strcmp(driver, el->key.driver) == 0 &&
            (group == NULL || strcmp(group, el->key.group) == 0)) {
            // 从哈希表中删除
            HASH_DEL(mgr->ss, el);
            // 释放所有应用订阅的资源
            utarray_foreach(el->apps, neu_app_subscribe_t *, sub_app)
            {
                neu_app_subscribe_fini(sub_app);
            }
            // 释放应用数组和元素本身
            utarray_free(el->apps);
            free(el);
        }
    }
}

/**
 * @brief 更新应用名称
 *
 * 更新所有订阅关系中指定应用的名称
 *
 * @param mgr 订阅管理器指针
 * @param app 原应用节点名称
 * @param new_name 新的应用节点名称
 * @return 始终返回0
 */
int neu_subscribe_manager_update_app_name(neu_subscribe_mgr_t *mgr,
                                          const char *app, const char *new_name)
{
    sub_elem_t *el = NULL, *tmp = NULL;

    // 遍历哈希表中的所有元素
    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        // 查找并更新每个匹配的应用名称
        utarray_foreach(el->apps, neu_app_subscribe_t *, sub_app)
        {
            if (strcmp(app, sub_app->app_name) == 0) {
                strncpy(sub_app->app_name, new_name, sizeof(sub_app->app_name));
            }
        }
    }

    return 0;
}

/**
 * @brief 更新驱动名称
 *
 * 更新所有订阅关系中指定驱动的名称
 *
 * @param mgr 订阅管理器指针
 * @param driver 原驱动节点名称
 * @param new_name 新的驱动节点名称
 * @return 始终返回0
 */
int neu_subscribe_manager_update_driver_name(neu_subscribe_mgr_t *mgr,
                                             const char          *driver,
                                             const char          *new_name)
{
    sub_elem_t *el = NULL, *tmp = NULL;

    // 遍历哈希表中的所有元素
    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        // 如果驱动名称匹配
        if (strcmp(driver, el->key.driver) == 0) {
            // 从哈希表中删除
            HASH_DEL(mgr->ss, el);
            // 更新驱动名称
            strncpy(el->key.driver, new_name, sizeof(el->key.driver));
            // 重新添加到哈希表
            HASH_ADD(hh, mgr->ss, key, sizeof(sub_elem_key_t), el);
        }
    }

    return 0;
}

/**
 * @brief 更新组名称
 *
 * 更新指定驱动和组的名称
 *
 * @param mgr 订阅管理器指针
 * @param driver 驱动节点名称
 * @param group 原组名称
 * @param new_name 新的组名称
 * @return 成功返回0，未订阅返回NEU_ERR_GROUP_NOT_SUBSCRIBE
 */
int neu_subscribe_manager_update_group_name(neu_subscribe_mgr_t *mgr,
                                            const char          *driver,
                                            const char          *group,
                                            const char          *new_name)
{
    sub_elem_t    *find = NULL;
    sub_elem_key_t key  = { 0 };

    // 构建查找键
    strncpy(key.driver, driver, sizeof(key.driver));
    strncpy(key.group, group, sizeof(key.group));

    // 在哈希表中查找
    HASH_FIND(hh, mgr->ss, &key, sizeof(sub_elem_key_t), find);

    if (NULL == find) {
        return NEU_ERR_GROUP_NOT_SUBSCRIBE;
    }

    // 从哈希表中删除
    HASH_DEL(mgr->ss, find);
    // 更新组名称
    strncpy(find->key.group, new_name, sizeof(find->key.group));
    // 重新添加到哈希表
    HASH_ADD(hh, mgr->ss, key, sizeof(sub_elem_key_t), find);

    return 0;
}
