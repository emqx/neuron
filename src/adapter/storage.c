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

#include "utils/log.h"

#include "adapter_internal.h"
#include "driver/driver_internal.h"
#include "storage.h"

/**
 * 保存节点运行状态到持久化存储
 *
 * @param node  节点名称
 * @param state 节点运行状态
 */
void adapter_storage_state(const char *node, neu_node_running_state_e state)
{
    neu_persister_update_node_state(node, state);
}

/**
 * 保存节点配置到持久化存储
 *
 * @param node    节点名称
 * @param setting 节点配置内容（JSON字符串）
 */
void adapter_storage_setting(const char *node, const char *setting)
{
    neu_persister_store_node_setting(node, setting);
}

/**
 * 添加数据组信息到持久化存储
 *
 * @param node     节点名称
 * @param group    数据组名称
 * @param interval 数据组采集间隔
 */
void adapter_storage_add_group(const char *node, const char *group,
                               uint32_t interval)
{
    neu_persist_group_info_t info = {
        .name     = (char *) group,
        .interval = interval,
    };

    neu_persister_store_group(node, &info);
}

/**
 * 更新数据组信息到持久化存储
 *
 * @param node     节点名称
 * @param group    原数据组名称
 * @param new_name 新数据组名称
 * @param interval 数据组采集间隔
 */
void adapter_storage_update_group(const char *node, const char *group,
                                  const char *new_name, uint32_t interval)
{
    neu_persist_group_info_t info = {
        .name     = (char *) new_name,
        .interval = interval,
    };

    int rv = neu_persister_update_group(node, group, &info);
    if (0 != rv) {
        nlog_error("fail update adapter:%s group:%s, new_name:%s interval: %u",
                   node, group, new_name, interval);
    }
}

/**
 * 从持久化存储中删除数据组
 *
 * @param node  节点名称
 * @param group 数据组名称
 */
void adapter_storage_del_group(const char *node, const char *group)
{
    neu_persister_delete_group(node, group);
}

/**
 * 添加单个数据标签到持久化存储
 *
 * @param node  节点名称
 * @param group 数据组名称
 * @param tag   数据标签
 */
void adapter_storage_add_tag(const char *node, const char *group,
                             const neu_datatag_t *tag)
{
    int rv = neu_persister_store_tag(node, group, tag);
    if (0 != rv) {
        nlog_error("fail store tag:%s adapter:%s grp:%s", tag->name, node,
                   group);
    }
}

/**
 * 批量添加数据标签到持久化存储
 *
 * @param node  节点名称
 * @param group 数据组名称
 * @param tags  数据标签数组
 * @param n     数据标签数量
 */
void adapter_storage_add_tags(const char *node, const char *group,
                              const neu_datatag_t *tags, size_t n)
{
    if (0 == n) {
        return;
    }

    if (1 == n) {
        return adapter_storage_add_tag(node, group, &tags[0]);
    }

    int rv = neu_persister_store_tags(node, group, tags, n);
    if (0 != rv) {
        nlog_error("fail store %zu tags:[%s ... %s] adapter:%s grp:%s", n,
                   tags->name, tags[n - 1].name, node, group);
    }
}

/**
 * 更新数据标签到持久化存储
 *
 * @param node  节点名称
 * @param group 数据组名称
 * @param tag   数据标签
 */
void adapter_storage_update_tag(const char *node, const char *group,
                                const neu_datatag_t *tag)
{
    int rv = neu_persister_update_tag(node, group, tag);
    if (0 != rv) {
        nlog_error("fail update tag:%s adapter:%s grp:%s", tag->name, node,
                   group);
    }
}

/**
 * 更新数据标签值到持久化存储
 *
 * @param node  节点名称
 * @param group 数据组名称
 * @param tag   数据标签
 */
void adapter_storage_update_tag_value(const char *node, const char *group,
                                      const neu_datatag_t *tag)
{
    int rv = neu_persister_update_tag_value(node, group, tag);
    if (0 != rv) {
        nlog_error("fail update value tag:%s adapter:%s grp:%s", tag->name,
                   node, group);
    }
}

/**
 * 从持久化存储中删除数据标签
 *
 * @param node  节点名称
 * @param group 数据组名称
 * @param name  数据标签名称
 */
void adapter_storage_del_tag(const char *node, const char *group,
                             const char *name)
{
    int rv = neu_persister_delete_tag(node, group, name);
    if (0 != rv) {
        nlog_error("fail del tag:%s adapter:%s grp:%s", name, node, group);
    }
}

/**
 * 从持久化存储加载节点配置
 *
 * @param node    节点名称
 * @param setting 输出参数，用于接收节点配置
 * @return 成功返回0，失败返回-1
 */
int adapter_load_setting(const char *node, char **setting)
{
    int rv = neu_persister_load_node_setting(node, (const char **) setting);
    if (0 != rv) {
        nlog_warn("load %s setting fail", node);
        return -1;
    }

    return 0;
}

/**
 * 从持久化存储加载数据组和标签
 *
 * @param driver 驱动适配器
 * @return 成功返回0，失败返回错误码
 */
int adapter_load_group_and_tag(neu_adapter_driver_t *driver)
{
    UT_array *     group_infos = NULL;
    neu_adapter_t *adapter     = (neu_adapter_t *) driver;

    int rv = neu_persister_load_groups(adapter->name, &group_infos);
    if (0 != rv) {
        nlog_warn("load %s group fail", adapter->name);
        return rv;
    }

    utarray_foreach(group_infos, neu_persist_group_info_t *, p)
    {
        UT_array *tags = NULL;
        neu_adapter_driver_add_group(driver, p->name, p->interval);

        rv = neu_persister_load_tags(adapter->name, p->name, &tags);
        if (0 != rv) {
            nlog_warn("load %s:%s tags fail", adapter->name, p->name);
            continue;
        }

        utarray_foreach(tags, neu_datatag_t *, tag)
        {
            neu_adapter_driver_add_tag(driver, p->name, tag);
            neu_adapter_driver_load_tag(driver, p->name, tag, 1);
        }

        utarray_free(tags);
    }

    utarray_free(group_infos);
    return rv;
}