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
 * @file node_manager.c
 * @brief 节点管理器实现
 *
 * 本文件实现了Neuron系统的节点管理器，负责管理系统中的所有节点（驱动和应用）。
 * 节点管理器提供节点的注册、查找、更新和删除等功能，同时维护节点的网络地址信息。
 */

#include <stdlib.h>
#include <string.h>

#include "errcodes.h"

#include "adapter/adapter_internal.h"
#include "node_manager.h"

/**
 * @brief 节点实体结构
 *
 * 表示系统中的一个节点实体，包含节点名称、适配器指针和网络地址等信息
 */
typedef struct node_entity {
    char *name; ///< 节点名称

    neu_adapter_t     *adapter;    ///< 节点对应的适配器
    bool               is_static;  ///< 是否为静态节点
    bool               display;    ///< 是否在界面上显示
    bool               single;     ///< 是否为单例节点
    bool               is_monitor; ///< 是否为监控节点
    struct sockaddr_in addr;       ///< 节点的网络地址

    UT_hash_handle hh; ///< 哈希表句柄，用于uthash
} node_entity_t;

/**
 * @brief 节点管理器结构
 *
 * 管理系统中的所有节点实体
 */
struct neu_node_manager {
    node_entity_t *nodes;    ///< 节点哈希表
    UT_array      *monitors; ///< 监控节点数组
};

/**
 * @brief 创建节点管理器
 *
 * 分配并初始化节点管理器对象
 *
 * @return 返回新创建的节点管理器对象，失败则返回NULL
 */
neu_node_manager_t *neu_node_manager_create()
{
    neu_node_manager_t *node_manager = calloc(1, sizeof(neu_node_manager_t));

    if (node_manager) {
        utarray_new(node_manager->monitors, &ut_ptr_icd);
    }

    return node_manager;
}

/**
 * @brief 销毁节点管理器
 *
 * 释放节点管理器及其管理的所有节点资源
 *
 * @param mgr 节点管理器对象
 */
void neu_node_manager_destroy(neu_node_manager_t *mgr)
{
    node_entity_t *el = NULL, *tmp = NULL;

    // 遍历并释放所有节点实体
    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        HASH_DEL(mgr->nodes, el);
        free(el->name);
        free(el);
    }

    utarray_free(mgr->monitors);
    free(mgr);
}

/**
 * @brief 添加节点到管理器
 *
 * 将适配器对应的节点添加到节点管理器中
 *
 * @param mgr 节点管理器对象
 * @param adapter 适配器对象
 * @return 成功返回0，失败返回错误码
 */
int neu_node_manager_add(neu_node_manager_t *mgr, neu_adapter_t *adapter)
{
    node_entity_t *node = calloc(1, sizeof(node_entity_t));

    node->adapter = adapter;
    node->name    = strdup(adapter->name);
    node->display = true;
    // 检查是否为监控节点
    node->is_monitor =
        (0 == strcmp(node->adapter->module->module_name, "Monitor"));

    // 添加到哈希表
    HASH_ADD_STR(mgr->nodes, name, node);

    // 如果是监控节点，添加到监控节点列表
    if (node->is_monitor) {
        utarray_push_back(mgr->monitors, &node);
    }

    return 0;
}

/**
 * @brief 添加静态节点到管理器
 *
 * 将静态适配器对应的节点添加到节点管理器中。静态节点通常是系统内置的节点。
 *
 * @param mgr 节点管理器对象
 * @param adapter 适配器对象
 * @return 成功返回0，失败返回错误码
 */
int neu_node_manager_add_static(neu_node_manager_t *mgr, neu_adapter_t *adapter)
{
    node_entity_t *node = calloc(1, sizeof(node_entity_t));

    node->adapter   = adapter;
    node->name      = strdup(adapter->name);
    node->is_static = true;
    node->display   = true;

    HASH_ADD_STR(mgr->nodes, name, node);

    return 0;
}

/**
 * @brief 添加单例节点到管理器
 *
 * 将单例适配器对应的节点添加到节点管理器中。单例节点是特定插件只能有一个实例的节点。
 *
 * @param mgr 节点管理器对象
 * @param adapter 适配器对象
 * @param display 是否在界面上显示该节点
 * @return 成功返回0，失败返回错误码
 */
int neu_node_manager_add_single(neu_node_manager_t *mgr, neu_adapter_t *adapter,
                                bool display)
{
    node_entity_t *node = calloc(1, sizeof(node_entity_t));

    node->adapter = adapter;
    node->name    = strdup(adapter->name);
    node->display = display;
    node->single  = true;

    HASH_ADD_STR(mgr->nodes, name, node);

    return 0;
}

/**
 * @brief 更新节点名称
 *
 * 修改节点的名称，并更新哈希表索引
 *
 * @param mgr 节点管理器对象
 * @param node_name 当前节点名称
 * @param new_node_name 新的节点名称
 * @return 成功返回0，失败返回错误码
 */
int neu_node_manager_update_name(neu_node_manager_t *mgr, const char *node_name,
                                 const char *new_node_name)
{
    node_entity_t *node = NULL;

    // 查找节点
    HASH_FIND_STR(mgr->nodes, node_name, node);
    if (NULL == node) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    char *new_name = strdup(new_node_name);
    if (NULL == new_name) {
        return NEU_ERR_EINTERNAL;
    }

    // 从哈希表删除，更新名称后重新添加
    HASH_DEL(mgr->nodes, node);
    free(node->name);
    node->name = new_name;
    HASH_ADD_STR(mgr->nodes, name, node);

    return 0;
}

/**
 * @brief 更新节点网络地址
 *
 * 更新指定节点的网络地址信息
 *
 * @param mgr 节点管理器对象
 * @param name 节点名称
 * @param addr 新的网络地址
 * @return 成功返回0，失败返回-1
 */
int neu_node_manager_update(neu_node_manager_t *mgr, const char *name,
                            struct sockaddr_in addr)
{
    node_entity_t *node = NULL;

    // 查找节点
    HASH_FIND_STR(mgr->nodes, name, node);
    if (NULL == node) {
        return -1;
    }
    // 更新地址
    node->addr = addr;

    return 0;
}

/**
 * @brief 删除节点
 *
 * 从节点管理器中删除指定的节点
 *
 * @param mgr 节点管理器对象
 * @param name 要删除的节点名称
 */
void neu_node_manager_del(neu_node_manager_t *mgr, const char *name)
{
    node_entity_t *node = NULL;

    HASH_FIND_STR(mgr->nodes, name, node);
    if (node != NULL) {
        HASH_DEL(mgr->nodes, node);
        // 如果是监控节点，从监控节点列表中移除
        if (node->is_monitor) {
            utarray_erase(mgr->monitors, utarray_eltidx(mgr->monitors, node),
                          1);
        }
        free(node->name);
        free(node);
    }
}

/**
 * @brief 获取节点数量
 *
 * 返回节点管理器中的节点总数
 *
 * @param mgr 节点管理器对象
 * @return 节点数量
 */
uint16_t neu_node_manager_size(neu_node_manager_t *mgr)
{
    return HASH_COUNT(mgr->nodes);
}

/**
 * @brief 检查是否存在未初始化节点
 *
 * 检查节点管理器中是否存在未初始化的节点（没有网络地址的节点）
 *
 * @param mgr 节点管理器对象
 * @return 如果存在未初始化节点返回true，否则返回false
 */
bool neu_node_manager_exist_uninit(neu_node_manager_t *mgr)
{
    node_entity_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        // 端口为0表示节点未初始化
        if (el->addr.sin_port == 0) {
            return true;
        }
    }

    return false;
}

/**
 * @brief 获取特定类型的节点信息
 *
 * 根据类型获取节点信息列表，不包括静态节点和不显示的节点
 *
 * @param mgr 节点管理器对象
 * @param type 节点类型过滤条件
 * @return 节点信息数组
 */
UT_array *neu_node_manager_get(neu_node_manager_t *mgr, int type)
{
    UT_array      *array = NULL;
    UT_icd         icd   = { sizeof(neu_resp_node_info_t), NULL, NULL, NULL };
    node_entity_t *el = NULL, *tmp = NULL;

    utarray_new(array, &icd);

    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        // 排除静态节点和不显示的节点
        if (!el->is_static && el->display) {
            // 按类型过滤
            if (el->adapter->module->type & type) {
                neu_resp_node_info_t info = { 0 };
                strcpy(info.node, el->adapter->name);
                strcpy(info.plugin, el->adapter->module->module_name);
                utarray_push_back(array, &info);
            }
        }
    }

    return array;
}

/**
 * @brief 过滤获取节点信息
 *
 * 根据类型、插件名称和节点名称过滤获取节点信息列表
 *
 * @param mgr 节点管理器对象
 * @param type 节点类型过滤条件
 * @param plugin 插件名称过滤条件，空字符串表示不过滤
 * @param node 节点名称过滤条件，空字符串表示不过滤
 * @return 过滤后的节点信息数组
 */
UT_array *neu_node_manager_filter(neu_node_manager_t *mgr, int type,
                                  const char *plugin, const char *node)
{
    UT_array      *array = NULL;
    UT_icd         icd   = { sizeof(neu_resp_node_info_t), NULL, NULL, NULL };
    node_entity_t *el = NULL, *tmp = NULL;

    utarray_new(array, &icd);

    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        if (!el->is_static && el->display) {
            if (el->adapter->module->type & type) {
                // 按插件名称过滤
                if (strlen(plugin) > 0 &&
                    strcmp(el->adapter->module->module_name, plugin) != 0) {
                    continue;
                }
                // 按节点名称过滤
                if (strlen(node) > 0 &&
                    strstr(el->adapter->name, node) == NULL) {
                    continue;
                }
                neu_resp_node_info_t info = { 0 };
                strcpy(info.node, el->adapter->name);
                strcpy(info.plugin, el->adapter->module->module_name);
                utarray_push_back(array, &info);
            }
        }
    }

    return array;
}

/**
 * @brief 获取所有节点信息
 *
 * 获取系统中所有节点的信息列表，包括静态节点和不显示的节点
 *
 * @param mgr 节点管理器对象
 * @return 所有节点信息数组
 */
UT_array *neu_node_manager_get_all(neu_node_manager_t *mgr)
{
    UT_array      *array = NULL;
    UT_icd         icd   = { sizeof(neu_resp_node_info_t), NULL, NULL, NULL };
    node_entity_t *el = NULL, *tmp = NULL;

    utarray_new(array, &icd);

    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        neu_resp_node_info_t info = { 0 };
        strcpy(info.node, el->adapter->name);
        strcpy(info.plugin, el->adapter->module->module_name);
        utarray_push_back(array, &info);
    }

    return array;
}

/**
 * @brief 获取特定类型的适配器
 *
 * 根据类型获取适配器对象列表，不包括静态节点和不显示的节点
 *
 * @param mgr 节点管理器对象
 * @param type 节点类型过滤条件
 * @return 适配器指针数组
 */
UT_array *neu_node_manager_get_adapter(neu_node_manager_t *mgr, int type)
{
    UT_array      *array = NULL;
    node_entity_t *el = NULL, *tmp = NULL;

    utarray_new(array, &ut_ptr_icd);

    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        if (!el->is_static && el->display) {
            if (el->adapter->module->type & type) {
                utarray_push_back(array, &el->adapter);
            }
        }
    }

    return array;
}

/**
 * @brief 查找节点对应的适配器
 *
 * 根据节点名称查找对应的适配器对象
 *
 * @param mgr 节点管理器对象
 * @param name 节点名称
 * @return 成功返回适配器指针，失败返回NULL
 */
neu_adapter_t *neu_node_manager_find(neu_node_manager_t *mgr, const char *name)
{
    neu_adapter_t *adapter = NULL;
    node_entity_t *node    = NULL;

    HASH_FIND_STR(mgr->nodes, name, node);
    if (node != NULL) {
        adapter = node->adapter;
    }

    return adapter;
}

/**
 * @brief 检查节点是否为单例节点
 *
 * 判断指定节点是否为单例节点
 *
 * @param mgr 节点管理器对象
 * @param name 节点名称
 * @return 如果是单例节点返回true，否则返回false
 */
bool neu_node_manager_is_single(neu_node_manager_t *mgr, const char *name)
{
    node_entity_t *node = NULL;

    HASH_FIND_STR(mgr->nodes, name, node);
    if (node != NULL) {
        return node->single;
    }

    return false;
}

/**
 * @brief 检查节点是否为驱动类型
 *
 * 判断指定节点是否为驱动节点
 *
 * @param mgr 节点管理器对象
 * @param name 节点名称
 * @return 如果是驱动节点返回true，否则返回false
 */
bool neu_node_manager_is_driver(neu_node_manager_t *mgr, const char *name)
{
    node_entity_t *node = NULL;

    HASH_FIND_STR(mgr->nodes, name, node);
    if (node != NULL) {
        return NEU_NA_TYPE_DRIVER == node->adapter->module->type;
    }

    return false;
}

/**
 * @brief 获取特定类型节点的网络地址
 *
 * 根据类型获取节点的网络地址列表，不包括静态节点
 *
 * @param mgr 节点管理器对象
 * @param type 节点类型过滤条件
 * @return 网络地址数组
 */
UT_array *neu_node_manager_get_addrs(neu_node_manager_t *mgr, int type)
{
    UT_icd         icd   = { sizeof(struct sockaddr_in), NULL, NULL, NULL };
    UT_array      *addrs = NULL;
    node_entity_t *el = NULL, *tmp = NULL;

    utarray_new(addrs, &icd);

    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        if (!el->is_static) {
            if (el->adapter->module->type & type) {
                struct sockaddr_in addr = el->addr;
                utarray_push_back(addrs, &addr);
            }
        }
    }

    return addrs;
}

/**
 * @brief 获取所有节点的网络地址
 *
 * 获取所有节点的网络地址列表，包括静态节点
 *
 * @param mgr 节点管理器对象
 * @return 所有节点的网络地址数组
 */
UT_array *neu_node_manager_get_addrs_all(neu_node_manager_t *mgr)
{
    UT_icd         icd   = { sizeof(struct sockaddr_in), NULL, NULL, NULL };
    UT_array      *addrs = NULL;
    node_entity_t *el = NULL, *tmp = NULL;

    utarray_new(addrs, &icd);

    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        struct sockaddr_in addr = el->addr;
        utarray_push_back(addrs, &addr);
    }

    return addrs;
}

/**
 * @brief 获取指定节点的网络地址
 *
 * 根据节点名称获取其网络地址
 *
 * @param mgr 节点管理器对象
 * @param name 节点名称
 * @return 节点的网络地址，如果节点不存在则返回全零地址
 */
struct sockaddr_in neu_node_manager_get_addr(neu_node_manager_t *mgr,
                                             const char         *name)
{
    struct sockaddr_in addr = { 0 };
    node_entity_t     *node = NULL;

    HASH_FIND_STR(mgr->nodes, name, node);
    if (node != NULL) {
        addr = node->addr;
    }

    return addr;
}

/**
 * @brief 检查节点是否为监控节点
 *
 * 判断指定节点是否为监控节点
 *
 * @param mgr 节点管理器对象
 * @param name 节点名称
 * @return 如果是监控节点返回true，否则返回false
 */
bool neu_node_manager_is_monitor(neu_node_manager_t *mgr, const char *name)
{
    node_entity_t *node = NULL;
    HASH_FIND_STR(mgr->nodes, name, node);
    return node ? node->is_monitor : false;
}

/**
 * @brief 遍历所有监控节点
 *
 * 对每个监控节点执行回调函数
 *
 * @param mgr 节点管理器对象
 * @param cb 回调函数，接收节点名称、网络地址和用户数据
 * @param data 传递给回调函数的用户数据
 * @return 成功返回0，如果回调函数返回非0值则中断遍历并返回该值
 */
int neu_node_manager_for_each_monitor(neu_node_manager_t *mgr,
                                      int (*cb)(const char        *name,
                                                struct sockaddr_in addr,
                                                void              *data),
                                      void *data)
{
    int rv = 0;

    utarray_foreach(mgr->monitors, node_entity_t **, node)
    {
        rv = cb((*node)->name, (*node)->addr, data);
        if (0 != rv) {
            break;
        }
    }

    return rv;
}

/**
 * @brief 获取所有节点的状态
 *
 * 获取非静态且可显示节点的状态信息，包括运行状态、链接状态、日志级别和RTT值
 *
 * @param mgr 节点管理器对象
 * @return 节点状态信息数组
 */
UT_array *neu_node_manager_get_state(neu_node_manager_t *mgr)
{
    UT_icd         icd    = { sizeof(neu_nodes_state_t), NULL, NULL, NULL };
    UT_array      *states = NULL;
    node_entity_t *el = NULL, *tmp = NULL;

    utarray_new(states, &icd);

    HASH_ITER(hh, mgr->nodes, el, tmp)
    {
        neu_nodes_state_t state = { 0 };

        if (!el->is_static && el->display) {
            strcpy(state.node, el->adapter->name);
            // 获取运行状态
            state.state.running = el->adapter->state;
            // 获取连接状态
            state.state.link =
                neu_plugin_to_plugin_common(el->adapter->plugin)->link_state;
            // 获取日志级别
            state.state.log_level = el->adapter->log_level;
            // 获取RTT值
            neu_metric_entry_t *e = NULL;
            if (NULL != el->adapter->metrics) {
                HASH_FIND_STR(el->adapter->metrics->entries,
                              NEU_METRIC_LAST_RTT_MS, e);
            }
            state.rtt = NULL != e ? e->value : 0;

            utarray_push_back(states, &state);
        }
    }

    return states;
}