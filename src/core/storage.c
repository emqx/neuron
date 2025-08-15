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
 * @file storage.c
 * @brief 存储管理模块，负责Neuron系统的持久化数据管理
 *
 * 该文件实现了与存储相关的函数，包括插件、节点、订阅关系、模板等数据的持久化存储和加载。
 * 它提供了一组接口，用于将内存中的配置数据保存到磁盘，以及在系统启动时从磁盘加载配置数据。
 *
 * 主要功能：
 * 1. 插件信息的存储与加载
 * 2. 节点(适配器)信息的存储、更新、删除与加载
 * 3. 订阅关系的存储、更新、删除与加载
 * 4. 模板及其组和标签的存储、更新、删除与加载
 */
#include "errcodes.h"
#include "utils/log.h"

#include "adapter/storage.h"
#include "storage.h"

/**
 * @brief 存储所有插件信息
 *
 * 从管理器获取所有插件信息，并将其保存到持久化存储中
 *
 * @param manager 管理器对象指针
 */
void manager_strorage_plugin(neu_manager_t *manager)
{
    UT_array *plugin_infos = NULL;

    // 获取所有插件信息
    plugin_infos = neu_manager_get_plugins(manager);

    // 将插件信息存储到持久化存储
    int rv = neu_persister_store_plugins(plugin_infos);
    if (0 != rv) {
        nlog_error("failed to store plugins infos");
    }

    // 释放插件信息列表
    neu_persist_plugin_infos_free(plugin_infos);
    return;
}

/**
 * @brief 添加节点信息到持久化存储
 *
 * 获取指定节点的信息，并将其保存到持久化存储中
 *
 * @param manager 管理器对象指针
 * @param node 节点名称
 */
void manager_storage_add_node(neu_manager_t *manager, const char *node)
{
    int                     rv        = 0;
    neu_persist_node_info_t node_info = {};

    // 获取节点信息
    rv = neu_manager_get_node_info(manager, node, &node_info);
    if (0 != rv) {
        nlog_error("unable to get adapter:%s info", node);
        return;
    }

    // 存储节点信息
    rv = neu_persister_store_node(&node_info);
    if (0 != rv) {
        nlog_error("failed to store adapter info");
    }

    // 释放节点信息资源
    neu_persist_node_info_fini(&node_info);
}

/**
 * @brief 更新节点信息
 *
 * 更新节点名称，将旧节点名称更改为新节点名称
 *
 * @param manager 管理器对象指针(未使用)
 * @param node 原节点名称
 * @param new_name 新节点名称
 */
void manager_storage_update_node(neu_manager_t *manager, const char *node,
                                 const char *new_name)
{
    (void) manager; // 未使用的参数
    neu_persister_update_node(node, new_name);
}

/**
 * @brief 从持久化存储中删除节点信息
 *
 * @param manager 管理器对象指针(未使用)
 * @param node 要删除的节点名称
 */
void manager_storage_del_node(neu_manager_t *manager, const char *node)
{
    (void) manager; // 未使用的参数
    neu_persister_delete_node(node);
}

/**
 * @brief 存储订阅关系
 *
 * 将应用节点对驱动节点组的订阅关系保存到持久化存储
 *
 * @param manager 管理器对象指针(未使用)
 * @param app 应用节点名称
 * @param driver 驱动节点名称
 * @param group 订阅的组名
 * @param params 订阅参数
 */
void manager_storage_subscribe(neu_manager_t *manager, const char *app,
                               const char *driver, const char *group,
                               const char *params)
{
    (void) manager; // 未使用的参数
    int rv = neu_persister_store_subscription(app, driver, group, params);
    if (0 != rv) {
        nlog_error("fail store subscription app:%s driver:%s group:%s", app,
                   driver, group);
    }
}

/**
 * @brief 更新订阅关系
 *
 * 更新应用节点对驱动节点组的订阅参数
 *
 * @param manager 管理器对象指针(未使用)
 * @param app 应用节点名称
 * @param driver 驱动节点名称
 * @param group 订阅的组名
 * @param params 更新后的订阅参数
 */
void manager_storage_update_subscribe(neu_manager_t *manager, const char *app,
                                      const char *driver, const char *group,
                                      const char *params)
{
    (void) manager; // 未使用的参数
    int rv = neu_persister_update_subscription(app, driver, group, params);
    if (0 != rv) {
        nlog_error("fail update subscription app:%s driver:%s group:%s", app,
                   driver, group);
    }
}

/**
 * @brief 删除订阅关系
 *
 * 从持久化存储中删除应用节点对驱动节点组的订阅关系
 *
 * @param manager 管理器对象指针(未使用)
 * @param app 应用节点名称
 * @param driver 驱动节点名称
 * @param group 订阅的组名
 */
void manager_storage_unsubscribe(neu_manager_t *manager, const char *app,
                                 const char *driver, const char *group)
{
    (void) manager; // 未使用的参数
    int rv = neu_persister_delete_subscription(app, driver, group);
    if (0 != rv) {
        nlog_error("fail delete subscription app:%s driver:%s group:%s", app,
                   driver, group);
    }
}

/**
 * @brief 添加北向驱动映射关系
 *
 * 将北向驱动对南向驱动组的映射关系保存到持久化存储，实际调用subscribe函数
 *
 * @param manager 管理器对象指针
 * @param ndriver 北向驱动节点名称
 * @param driver 南向驱动节点名称
 * @param group 订阅的组名
 */
void manager_storage_add_ndriver_map(neu_manager_t *manager,
                                     const char *ndriver, const char *driver,
                                     const char *group)
{
    return manager_storage_subscribe(manager, ndriver, driver, group, NULL);
}

/**
 * @brief 删除北向驱动映射关系
 *
 * 从持久化存储中删除北向驱动对南向驱动组的映射关系，实际调用unsubscribe函数
 *
 * @param manager 管理器对象指针
 * @param ndriver 北向驱动节点名称
 * @param driver 南向驱动节点名称
 * @param group 订阅的组名
 */
void manager_storage_del_ndriver_map(neu_manager_t *manager,
                                     const char *ndriver, const char *driver,
                                     const char *group)
{
    return manager_storage_unsubscribe(manager, ndriver, driver, group);
}

/**
 * @brief 保存节点的组和标签信息
 *
 * 用于遍历组并保存其信息的回调函数，从组中提取信息并保存到存储系统中
 *
 * @param grp 组对象指针
 * @param data 用户数据，实际上是节点名称
 * @return 成功返回0，失败返回错误码
 */
static int save_node_group_and_tags(neu_group_t *grp, void *data)
{
    const char *node     = data;
    const char *name     = neu_group_get_name(grp);
    uint32_t    interval = neu_group_get_interval(grp);

    // 获取组内所有标签
    UT_array *tags = neu_group_get_tag(grp);
    if (NULL == tags) {
        return NEU_ERR_EINTERNAL;
    }

    // 保存组和标签信息
    adapter_storage_add_group(node, name, interval);
    adapter_storage_add_tags(node, name, utarray_front(tags),
                             utarray_len(tags));

    // 释放标签数组
    utarray_free(tags);
    return 0;
}

/**
 * @brief 存储从模板实例化的节点信息
 *
 * 将基于模板创建的节点及其组和标签信息保存到持久化存储
 *
 * @param manager 管理器对象指针
 * @param tmpl_name 模板名称
 * @param node 实例化的节点名称
 */
void manager_storage_inst_node(neu_manager_t *manager, const char *tmpl_name,
                               const char *node)
{
    // 查找模板
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, tmpl_name);
    if (NULL == tmpl) {
        nlog_error("save instantiated node `%s`, fail, no template `%s`", node,
                   tmpl_name);
        return;
    }

    // 保存节点基本信息
    manager_storage_add_node(manager, node);

    // 遍历所有组，保存组和标签信息
    int rv = neu_template_for_each_group(tmpl, save_node_group_and_tags,
                                         (void *) node);
    if (0 != rv) {
        nlog_error("save instantiated node `%s` groups and tags fail", node);
    }
}

/**
 * @brief 保存模板的组和标签信息
 *
 * 用于遍历模板组并保存其信息的回调函数，从组中提取信息并保存到存储系统中
 *
 * @param grp 组对象指针
 * @param data 用户数据，实际上是模板名称
 * @return 成功返回0，失败返回错误码
 */
static int save_tmpl_group_and_tags(neu_group_t *grp, void *data)
{
    const char *tmpl_name = data;
    const char *name      = neu_group_get_name(grp);
    uint32_t    interval  = neu_group_get_interval(grp);

    // 获取组内所有标签
    UT_array *tags = neu_group_get_tag(grp);
    if (NULL == tags) {
        return NEU_ERR_EINTERNAL;
    }

    // 保存模板组和标签信息
    manager_storage_add_template_group(tmpl_name, name, interval);
    manager_storage_add_template_tags(tmpl_name, name, utarray_front(tags),
                                      utarray_len(tags));

    // 释放标签数组
    utarray_free(tags);
    return 0;
}

/**
 * @brief 添加模板信息到持久化存储
 *
 * 将模板及其组和标签信息保存到持久化存储
 *
 * @param manager 管理器对象指针
 * @param tmpl_name 模板名称
 */
void manager_storage_add_template(neu_manager_t *manager, const char *tmpl_name)
{
    // 查找模板
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, tmpl_name);
    if (NULL == tmpl) {
        nlog_error("no template `%s` to fail", tmpl_name);
        return;
    }

    // 保存模板基本信息
    int rv = neu_persister_store_template(tmpl_name, neu_template_plugin(tmpl));
    if (0 != rv) {
        nlog_error("failed to store template info");
        return;
    }

    // 遍历所有组，保存组和标签信息
    rv = neu_template_for_each_group(tmpl, save_tmpl_group_and_tags,
                                     (void *) tmpl_name);
    if (0 != rv) {
        nlog_error("save template `%s` groups and tags fail", tmpl_name);
    }
}

/**
 * @brief 从持久化存储中删除模板信息
 *
 * @param manager 管理器对象指针(未使用)
 * @param tmpl_name 要删除的模板名称
 */
void manager_storage_del_template(neu_manager_t *manager, const char *tmpl_name)
{
    (void) manager; // 未使用的参数

    int rv = neu_persister_delete_template(tmpl_name);
    if (0 != rv) {
        nlog_error("failed to delete template info");
    }
}

/**
 * @brief 清除所有模板信息
 *
 * 从持久化存储中删除所有模板信息
 *
 * @param manager 管理器对象指针(未使用)
 */
void manager_storage_clear_templates(neu_manager_t *manager)
{
    (void) manager; // 未使用的参数

    int rv = neu_persister_clear_templates();
    if (0 != rv) {
        nlog_error("failed to clear templates info");
    }
}

/**
 * @brief 添加模板组信息到持久化存储
 *
 * @param tmpl 模板名称
 * @param group 组名称
 * @param interval 采集间隔时间
 */
void manager_storage_add_template_group(const char *tmpl, const char *group,
                                        uint32_t interval)
{
    // 构建组信息结构
    neu_persist_group_info_t info = {
        .name     = (char *) group,
        .interval = interval,
    };

    // 存储模板组信息
    int rv = neu_persister_store_template_group(tmpl, &info);
    if (0 != rv) {
        nlog_error("store template:%s grp:%s, interval:%" PRIu32 " fail", tmpl,
                   group, interval);
    }
}

/**
 * @brief 更新模板组信息
 *
 * 更新模板组的名称和采集间隔
 *
 * @param tmpl 模板名称
 * @param group 原组名称
 * @param new_name 新组名称
 * @param interval 更新后的采集间隔时间
 */
void manager_storage_update_template_group(const char *tmpl, const char *group,
                                           const char *new_name,
                                           uint32_t    interval)
{
    // 构建更新后的组信息结构
    neu_persist_group_info_t info = {
        .name     = (char *) new_name,
        .interval = interval,
    };

    // 更新模板组信息
    int rv = neu_persister_update_template_group(tmpl, group, &info);
    if (0 != rv) {
        nlog_error("update template:%s group:%s, new_name:%s interval:%" PRIu32
                   " fail",
                   tmpl, group, new_name, interval);
    }
}

/**
 * @brief 从持久化存储中删除模板组信息
 *
 * @param tmpl 模板名称
 * @param group 要删除的组名称
 */
void manager_storage_del_template_group(const char *tmpl, const char *group)
{
    int rv = neu_persister_delete_template_group(tmpl, group);
    if (0 != rv) {
        nlog_error("delete template:%s grp:%s fail", tmpl, group);
    }
}

/**
 * @brief 添加模板标签到持久化存储
 *
 * @param tmpl 模板名称
 * @param group 组名称
 * @param tag 标签数据指针
 */
void manager_storage_add_template_tag(const char *tmpl, const char *group,
                                      const neu_datatag_t *tag)
{
    int rv = neu_persister_store_template_tags(tmpl, group, tag, 1);
    if (0 != rv) {
        nlog_error("store tag:%s template:%s grp:%s fail", tag->name, tmpl,
                   group);
    }
}

/**
 * @brief 批量添加模板标签到持久化存储
 *
 * @param tmpl 模板名称
 * @param group 组名称
 * @param tags 标签数组指针
 * @param n 标签数量
 */
void manager_storage_add_template_tags(const char *tmpl, const char *group,
                                       const neu_datatag_t *tags, size_t n)
{
    // 处理边界情况
    if (0 == n) {
        return;
    }

    // 如果只有一个标签，调用单标签添加函数
    if (1 == n) {
        return manager_storage_add_template_tag(tmpl, group, &tags[0]);
    }

    // 批量存储标签
    int rv = neu_persister_store_template_tags(tmpl, group, tags, n);
    if (0 != rv) {
        nlog_error("store %zu tags:[%s ... %s] template:%s grp:%s fail", n,
                   tags->name, tags[n - 1].name, tmpl, group);
    }
}

/**
 * @brief 更新模板标签
 *
 * @param tmpl 模板名称
 * @param group 组名称
 * @param tag 更新后的标签数据指针
 */
void manager_storage_update_template_tag(const char *tmpl, const char *group,
                                         const neu_datatag_t *tag)
{
    int rv = neu_persister_update_template_tags(tmpl, group, tag, 1);
    if (0 != rv) {
        nlog_error("update tag:%s template:%s grp:%s fail", tag->name, tmpl,
                   group);
    }
}

/**
 * @brief 批量更新模板标签
 *
 * @param tmpl 模板名称
 * @param group 组名称
 * @param tags 标签数组指针
 * @param n 标签数量
 */
void manager_storage_update_template_tags(const char *tmpl, const char *group,
                                          const neu_datatag_t *tags, size_t n)
{
    // 处理边界情况
    if (0 == n) {
        return;
    }

    // 如果只有一个标签，调用单标签更新函数
    if (1 == n) {
        return manager_storage_update_template_tag(tmpl, group, &tags[0]);
    }

    // 批量更新标签
    int rv = neu_persister_update_template_tags(tmpl, group, tags, n);
    if (0 != rv) {
        nlog_error("update %zu tags:[%s ... %s] template:%s grp:%s fail", n,
                   tags->name, tags[n - 1].name, tmpl, group);
    }
}

/**
 * @brief 删除模板标签
 *
 * 从持久化存储中删除指定的模板标签
 *
 * @param tmpl 模板名称
 * @param group 组名称
 * @param tag 要删除的标签名称
 */
void manager_storage_del_template_tag(const char *tmpl, const char *group,
                                      const char *tag)
{
    int rv = neu_persister_delete_template_tags(tmpl, group, &tag, 1);
    if (0 != rv) {
        nlog_error("delete tag:%s template:%s grp:%s fail", tag, tmpl, group);
    }
}

/**
 * @brief 批量删除模板标签
 *
 * 从持久化存储中批量删除指定的模板标签
 *
 * @param tmpl 模板名称
 * @param group 组名称
 * @param tags 要删除的标签名称数组
 * @param n 标签数量
 */
void manager_storage_del_template_tags(const char *tmpl, const char *group,
                                       const char *const *tags, size_t n)
{
    // 处理边界情况
    if (0 == n) {
        return;
    }

    // 如果只有一个标签，调用单标签删除函数
    if (1 == n) {
        return manager_storage_del_template_tag(tmpl, group, tags[0]);
    }

    // 批量删除标签
    int rv = neu_persister_delete_template_tags(tmpl, group, tags, n);
    if (0 != rv) {
        nlog_error("del %zu tags:[%s ... %s] template:%s grp:%s fail", n,
                   tags[0], tags[n - 1], tmpl, group);
    }
}

/**
 * @brief 从持久化存储加载插件
 *
 * 从持久化存储中加载所有插件信息并添加到管理器中
 *
 * @param manager 管理器对象指针
 * @return 成功返回0，失败返回错误码
 */
int manager_load_plugin(neu_manager_t *manager)
{
    UT_array *plugin_infos = NULL;

    // 从持久化存储加载插件信息
    int rv = neu_persister_load_plugins(&plugin_infos);
    if (rv != 0) {
        return rv;
    }

    // 遍历插件信息，添加到管理器
    utarray_foreach(plugin_infos, char **, name)
    {
        rv                    = neu_manager_add_plugin(manager, *name);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        nlog_notice("load plugin %s, lib:%s", ok_or_err, *name);
    }

    // 释放资源
    utarray_foreach(plugin_infos, char **, name)
    {
        free(*name);
    }
    utarray_free(plugin_infos);

    return rv;
}

/**
 * @brief 从持久化存储加载节点
 *
 * 从持久化存储中加载所有节点信息并添加到管理器中
 *
 * @param manager 管理器对象指针
 * @return 成功返回0，失败返回错误码
 */
int manager_load_node(neu_manager_t *manager)
{
    UT_array *node_infos = NULL;
    int       rv         = 0;

    // 从持久化存储加载节点信息
    rv = neu_persister_load_nodes(&node_infos);
    if (0 != rv) {
        nlog_error("failed to load adapter infos");
        return -1;
    }

    // 遍历节点信息，添加到管理器
    utarray_foreach(node_infos, neu_persist_node_info_t *, node_info)
    {
        rv                    = neu_manager_add_node(manager, node_info->name,
                                                     node_info->plugin_name, node_info->state,
                                                     true);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        nlog_notice("load adapter %s type:%d, name:%s plugin:%s state:%d",
                    ok_or_err, node_info->type, node_info->name,
                    node_info->plugin_name, node_info->state);
    }

    // 释放资源
    utarray_free(node_infos);
    return rv;
}

/**
 * @brief 从持久化存储加载订阅关系
 *
 * 从持久化存储中加载所有应用节点的订阅关系并重新建立订阅
 *
 * @param manager 管理器对象指针
 * @return 始终返回0
 */
int manager_load_subscribe(neu_manager_t *manager)
{
    // 获取所有应用节点和北向驱动节点
    UT_array *nodes = neu_node_manager_get(
        manager->node_manager, NEU_NA_TYPE_APP | NEU_NA_TYPE_NDRIVER);

    // 遍历所有节点
    utarray_foreach(nodes, neu_resp_node_info_t *, node)
    {
        int       rv        = 0;
        UT_array *sub_infos = NULL;

        // 加载该节点的所有订阅信息
        rv = neu_persister_load_subscriptions(node->node, &sub_infos);
        if (0 != rv) {
            nlog_warn("load %s subscribetion infos error", node->node);
        } else {
            // 遍历所有订阅信息，重新建立订阅关系
            utarray_foreach(sub_infos, neu_persist_subscription_info_t *, info)
            {
                uint16_t app_port = 0;
                // 建立订阅关系
                rv = neu_manager_subscribe(manager, node->node,
                                           info->driver_name, info->group_name,
                                           info->params, &app_port);
                const char *ok_or_err = (0 == rv) ? "success" : "fail";
                nlog_notice("%s load subscription app:%s driver:%s grp:%s",
                            ok_or_err, node->node, info->driver_name,
                            info->group_name);

                // 如果订阅成功，发送订阅消息
                if (0 == rv) {
                    neu_manager_send_subscribe(
                        manager, node->node, info->driver_name,
                        info->group_name, app_port, info->params);
                }
            }

            // 释放订阅信息资源
            utarray_free(sub_infos);
        }
    }

    // 释放节点信息资源
    utarray_free(nodes);
    return 0;
}

/**
 * @brief 加载模板的组和标签信息
 *
 * 从持久化存储中加载指定模板的所有组和标签信息，并添加到管理器中
 *
 * @param manager 管理器对象指针
 * @param tmpl_name 模板名称
 * @return 成功返回0，失败返回错误码
 */
static int load_template_group_and_tags(neu_manager_t *manager,
                                        const char    *tmpl_name)
{
    int       rv          = 0;
    UT_array *group_infos = NULL;

    // 加载模板的所有组信息
    rv = neu_persister_load_template_groups(tmpl_name, &group_infos);
    if (0 != rv) {
        nlog_warn("load template %s group fail", tmpl_name);
        return rv;
    }

    // 遍历所有组信息
    utarray_foreach(group_infos, neu_persist_group_info_t *, p)
    {
        // 添加组到模板
        neu_manager_add_template_group(manager, tmpl_name, p->name,
                                       p->interval);

        // 加载该组的所有标签信息
        UT_array *tags = NULL;
        rv = neu_persister_load_template_tags(tmpl_name, p->name, &tags);
        if (0 != rv) {
            nlog_warn("load template:%s group:%s tags fail", tmpl_name,
                      p->name);
            continue;
        }

        // 添加标签到模板组
        neu_manager_add_template_tags(manager, tmpl_name, p->name,
                                      utarray_len(tags), utarray_front(tags),
                                      NULL);

        // 释放标签信息资源
        utarray_free(tags);
    }

    // 释放组信息资源
    utarray_free(group_infos);
    return rv;
}

/**
 * @brief 从持久化存储加载模板
 *
 * 从持久化存储中加载所有模板信息及其组和标签信息，并添加到管理器中
 *
 * @param manager 管理器对象指针
 * @return 成功返回0，失败返回错误码
 */
int manager_load_template(neu_manager_t *manager)
{
    int       rv         = 0;
    UT_array *tmpl_infos = NULL;

    // 加载所有模板信息
    rv = neu_persister_load_templates(&tmpl_infos);
    if (0 != rv) {
        nlog_error("failed to load template infos");
        return -1;
    }

    // 遍历所有模板信息
    utarray_foreach(tmpl_infos, neu_persist_template_info_t *, tmpl_info)
    {
        // 添加模板到管理器
        rv = neu_manager_add_template(manager, tmpl_info->name,
                                      tmpl_info->plugin_name, 0, NULL);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        nlog_info("load template name:%s plugin:%s %s", tmpl_info->name,
                  tmpl_info->plugin_name, ok_or_err);
        if (0 != rv) {
            continue; // 忽略错误，尽可能加载更多数据
        }

        // 加载该模板的所有组和标签信息
        load_template_group_and_tags(manager, tmpl_info->name);
    }

    // 释放模板信息资源
    utarray_free(tmpl_infos);
    return rv;
}
