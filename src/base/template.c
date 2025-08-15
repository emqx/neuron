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

#include "errcodes.h"
#include "group.h"
#include "utils/utarray.h"
#include "utils/uthash.h"

#include "template.h"

/**
 * @brief 组条目结构体
 *
 * 用于在模板内部管理组
 */
typedef struct {
    neu_group_t   *group; /**< 指向组对象的指针 */
    UT_hash_handle hh;    /**< UT哈希处理结构，用于哈希表实现 */
} group_entry_t;

/**
 * @brief 模板结构体
 *
 * 模板是一个预定义的配置模式，可以包含多个组和标签
 */
struct neu_template_s {
    char                      *name;          /**< 模板名称 */
    char                      *plugin;        /**< 关联的插件名称 */
    neu_plugin_tag_validator_t tag_validator; /**< 标签验证器函数指针 */
    group_entry_t             *groups;        /**< 组条目哈希表 */
};

/**
 * @brief 释放组条目资源
 *
 * @param ent 要释放的组条目指针
 */
static inline void group_entry_free(group_entry_t *ent)
{
    neu_group_destroy(ent->group); /* 销毁组对象 */
    free(ent);                     /* 释放条目内存 */
}

/**
 * @brief 创建新的模板对象
 *
 * @param name 模板名称
 * @param plugin 关联的插件名称
 * @return 成功返回模板指针，失败返回NULL
 */
neu_template_t *neu_template_new(const char *name, const char *plugin)
{
    /* 分配模板结构内存 */
    neu_template_t *tmpl = calloc(1, sizeof(*tmpl));
    if (NULL == tmpl) {
        return NULL;
    }

    /* 复制模板名称 */
    tmpl->name = strdup(name);
    if (NULL == tmpl->name) {
        free(tmpl);
        return NULL;
    }

    /* 复制插件名称 */
    tmpl->plugin = strdup(plugin);
    if (NULL == tmpl->plugin) {
        free(tmpl->name);
        free(tmpl);
        return NULL;
    }

    return tmpl;
}

/**
 * @brief 释放模板对象资源
 *
 * @param tmpl 要释放的模板对象指针
 */
void neu_template_free(neu_template_t *tmpl)
{
    if (tmpl) {
        /* 释放所有组条目 */
        group_entry_t *ent = NULL, *tmp = NULL;
        HASH_ITER(hh, tmpl->groups, ent, tmp)
        {
            HASH_DEL(tmpl->groups, ent);
            group_entry_free(ent);
        }

        /* 释放模板基本属性和结构体 */
        free(tmpl->name);
        free(tmpl->plugin);
        free(tmpl);
    }
}

/**
 * @brief 设置模板的标签验证器函数
 *
 * @param tmpl 模板对象指针
 * @param validator 标签验证器函数指针
 */
void neu_template_set_tag_validator(neu_template_t            *tmpl,
                                    neu_plugin_tag_validator_t validator)
{
    tmpl->tag_validator = validator;
}

/**
 * @brief 获取模板名称
 *
 * @param tmpl 模板对象指针
 * @return 模板名称字符串
 */
const char *neu_template_name(const neu_template_t *tmpl)
{
    return tmpl->name;
}

/**
 * @brief 获取模板关联的插件名称
 *
 * @param tmpl 模板对象指针
 * @return 插件名称字符串
 */
const char *neu_template_plugin(const neu_template_t *tmpl)
{
    return tmpl->plugin;
}

/**
 * @brief 获取模板中指定名称的组
 *
 * @param tmpl 模板对象指针
 * @param group 组名称
 * @return 成功返回组指针，如果组不存在则返回NULL
 */
neu_group_t *neu_template_get_group(const neu_template_t *tmpl,
                                    const char           *group)
{
    /* 在哈希表中查找对应名称的组 */
    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    return ent ? ent->group : NULL;
}

/**
 * @brief 向模板中添加新组
 *
 * @param tmpl 模板对象指针
 * @param group 组名称
 * @param interval 组轮询间隔(毫秒)
 * @return 成功返回0，失败返回错误码
 *         - NEU_ERR_GROUP_EXIST: 组已存在
 *         - NEU_ERR_EINTERNAL: 内部错误(内存分配失败等)
 */
int neu_template_add_group(neu_template_t *tmpl, const char *group,
                           uint32_t interval)
{
    group_entry_t *ent = NULL;

    /* 检查组是否已存在 */
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (ent) {
        return NEU_ERR_GROUP_EXIST;
    }

    /* 分配组条目内存 */
    ent = calloc(1, sizeof(*ent));
    if (NULL == ent) {
        return NEU_ERR_EINTERNAL;
    }

    /* 创建新的组对象 */
    ent->group = neu_group_new(group, interval);
    if (NULL == ent->group) {
        free(ent);
        return NEU_ERR_EINTERNAL;
    }

    /* 将组条目添加到哈希表 */
    HASH_ADD_KEYPTR(hh, tmpl->groups, neu_group_get_name(ent->group),
                    strlen(group), ent);

    return 0;
}

/**
 * @brief 从模板中删除指定组
 *
 * @param tmpl 模板对象指针
 * @param group 要删除的组名称
 * @return 成功返回0，失败返回错误码
 *         - NEU_ERR_GROUP_NOT_EXIST: 组不存在
 */
int neu_template_del_group(neu_template_t *tmpl, const char *group)
{
    /* 在哈希表中查找指定组 */
    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (NULL == ent) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    /* 从哈希表中删除组条目并释放资源 */
    HASH_DEL(tmpl->groups, ent);
    group_entry_free(ent);
    return 0;
}

/**
 * @brief 更新模板中指定组的轮询间隔
 *
 * @param tmpl 模板对象指针
 * @param group 组名称
 * @param interval 新的轮询间隔(毫秒)
 * @return 成功返回0，失败返回错误码
 *         - NEU_ERR_GROUP_NOT_EXIST: 组不存在
 */
int neu_template_update_group_interval(neu_template_t *tmpl, const char *group,
                                       uint32_t interval)
{
    /* 查找指定的组 */
    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (NULL == ent) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    /* 更新组的轮询间隔 */
    return neu_group_update(ent->group, interval);
}

/**
 * @brief 更新模板中指定组的名称
 *
 * @param tmpl 模板对象指针
 * @param group 当前组名称
 * @param new_name 新的组名称
 * @return 成功返回0，失败返回错误码
 *         - NEU_ERR_GROUP_EXIST: 新名称的组已存在
 *         - NEU_ERR_GROUP_NOT_EXIST: 原组不存在
 */
int neu_template_update_group_name(neu_template_t *tmpl, const char *group,
                                   const char *new_name)
{
    /* 如果新旧名称相同，直接返回成功 */
    if (0 == strcmp(group, new_name)) {
        return 0;
    }

    /* 检查新名称是否已被占用 */
    if (NULL != neu_template_get_group(tmpl, new_name)) {
        return NEU_ERR_GROUP_EXIST;
    }

    /* 查找指定的组 */
    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (NULL == ent) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    /* 从哈希表中移除组条目 */
    HASH_DEL(tmpl->groups, ent);
    /* 设置组的新名称 */
    int rv = neu_group_set_name(ent->group, new_name);
    /* 获取实际设置的组名称(可能经过处理) */
    new_name = neu_group_get_name(ent->group);
    /* 使用新名称将组条目重新添加到哈希表 */
    HASH_ADD_KEYPTR(hh, tmpl->groups, new_name, strlen(new_name), ent);

    return rv;
}

/**
 * @brief 获取模板中的组数量
 *
 * @param tmpl 模板对象指针
 * @return 模板中的组数量
 */
size_t neu_template_group_num(const neu_template_t *tmpl)
{
    return HASH_COUNT(tmpl->groups);
}

/**
 * @brief 遍历模板中的所有组并对每个组执行回调函数
 *
 * @param tmpl 模板对象指针
 * @param cb 对每个组执行的回调函数，函数签名为 int (*cb)(neu_group_t *group,
 * void *data)
 * @param data 传递给回调函数的用户数据
 * @return 成功返回0，如果回调函数返回非0值则终止遍历并返回该值
 */
int neu_template_for_each_group(neu_template_t *tmpl,
                                int (*cb)(neu_group_t *group, void *data),
                                void *data)
{
    int            rv  = 0;
    group_entry_t *ent = NULL, *tmp = NULL;
    /* 遍历哈希表中的所有组条目 */
    HASH_ITER(hh, tmpl->groups, ent, tmp)
    {
        /* 对每个组执行回调函数，如果返回非0值则中断遍历 */
        if (0 != (rv = cb(ent->group, data))) {
            break;
        }
    }

    return rv;
}

/**
 * @brief 向模板的指定组中添加标签
 *
 * @param tmpl 模板对象指针
 * @param group 组名称
 * @param tag 要添加的标签对象指针
 * @return 成功返回0，失败返回错误码
 *         - NEU_ERR_GROUP_NOT_EXIST: 组不存在
 *         - NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE:
 * 插件不支持模板功能(无标签验证器)
 *         - 其他错误码: 由标签验证器或组添加标签函数返回
 */
int neu_template_add_tag(neu_template_t *tmpl, const char *group,
                         const neu_datatag_t *tag)
{
    int ret = 0;
    /* 查找指定的组 */
    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (NULL == ent) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    /* 检查是否设置了标签验证器 */
    if (NULL == tmpl->tag_validator) {
        return NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE;
    }

    /* 使用验证器验证标签 */
    if (0 != (ret = tmpl->tag_validator(tag))) {
        return ret;
    }

    /* 将标签添加到组中 */
    return neu_group_add_tag(ent->group, tag);
}

/**
 * @brief 更新模板指定组中的标签
 *
 * @param tmpl 模板对象指针
 * @param group 组名称
 * @param tag 更新后的标签对象指针
 * @return 成功返回0，失败返回错误码
 *         - NEU_ERR_GROUP_NOT_EXIST: 组不存在
 *         - NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE:
 * 插件不支持模板功能(无标签验证器)
 *         - 其他错误码: 由标签验证器或组更新标签函数返回
 */
int neu_template_update_tag(neu_template_t *tmpl, const char *group,
                            const neu_datatag_t *tag)
{
    int ret = 0;

    /* 查找指定的组 */
    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (NULL == ent) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    /* 检查是否设置了标签验证器 */
    if (NULL == tmpl->tag_validator) {
        return NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE;
    }

    /* 使用验证器验证标签 */
    if (0 != (ret = tmpl->tag_validator(tag))) {
        return ret;
    }

    /* 更新组中的标签 */
    return neu_group_update_tag(ent->group, tag);
}
