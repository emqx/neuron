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
#include <dlfcn.h>

#include "utils/log.h"
#include "json/neu_json_param.h"

#include "adapter.h"
#include "errcodes.h"

#include "adapter/adapter_internal.h"
#include "adapter/driver/driver_internal.h"
#include "base/msg_internal.h"

#include "manager_internal.h"
#include "template_manager.h"

/**
 * @brief 添加插件库
 *
 * 向管理器中添加一个新的插件库
 *
 * @param manager 管理器对象指针
 * @param library 插件库文件路径
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_add_plugin(neu_manager_t *manager, const char *library)
{
    return neu_plugin_manager_add(manager->plugin_manager, library);
}

/**
 * @brief 删除插件
 *
 * 从管理器中删除指定的插件
 *
 * @param manager 管理器对象指针
 * @param plugin 插件名称
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_del_plugin(neu_manager_t *manager, const char *plugin)
{
    return neu_plugin_manager_del(manager->plugin_manager, plugin);
}

/**
 * @brief 获取所有插件列表
 *
 * @param manager 管理器对象指针
 * @return 插件列表数组
 */
UT_array *neu_manager_get_plugins(neu_manager_t *manager)
{
    return neu_plugin_manager_get(manager->plugin_manager);
}

/**
 * @brief 添加节点
 *
 * 基于指定的插件创建并添加一个新节点到管理器
 *
 * @param manager 管理器对象指针
 * @param node_name 节点名称
 * @param plugin_name 插件名称
 * @param state 节点初始运行状态
 * @param load 是否从持久化存储加载配置
 * @return 成功返回NEU_ERR_SUCCESS，失败返回错误码
 */
int neu_manager_add_node(neu_manager_t *manager, const char *node_name,
                         const char              *plugin_name,
                         neu_node_running_state_e state, bool load)
{
    neu_adapter_t        *adapter      = NULL;
    neu_plugin_instance_t instance     = { 0 };
    neu_adapter_info_t    adapter_info = {
           .name = node_name,
    };
    neu_resp_plugin_info_t info = { 0 };
    /* 查找指定的插件 */
    int ret =
        neu_plugin_manager_find(manager->plugin_manager, plugin_name, &info);

    if (ret != 0) {
        return NEU_ERR_LIBRARY_NOT_FOUND;
    }

    /* 检查插件是否为单例类型 */
    if (info.single) {
        return NEU_ERR_LIBRARY_NOT_ALLOW_CREATE_INSTANCE;
    }

    /* 检查节点名称是否已存在 */
    adapter = neu_node_manager_find(manager->node_manager, node_name);
    if (adapter != NULL) {
        return NEU_ERR_NODE_EXIST;
    }

    /* 创建插件实例 */
    ret = neu_plugin_manager_create_instance(manager->plugin_manager, info.name,
                                             &instance);
    if (ret != 0) {
        return NEU_ERR_LIBRARY_FAILED_TO_OPEN;
    }
    adapter_info.handle = instance.handle;
    adapter_info.module = instance.module;

    /* 创建适配器 */
    adapter = neu_adapter_create(&adapter_info, load);
    if (adapter == NULL) {
        return neu_adapter_error();
    }
    /* 添加节点到管理器 */
    neu_node_manager_add(manager->node_manager, adapter);
    /* 初始化适配器 */
    neu_adapter_init(adapter, state);

    return NEU_ERR_SUCCESS;
}

/**
 * @brief 删除节点
 *
 * 删除管理器中指定的节点
 *
 * @param manager 管理器对象指针
 * @param node_name 要删除的节点名称
 * @return 成功返回NEU_ERR_SUCCESS，失败返回错误码
 */
int neu_manager_del_node(neu_manager_t *manager, const char *node_name)
{
    /* 查找节点 */
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, node_name);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    /* 销毁适配器 */
    neu_adapter_destroy(adapter);
    /* 移除相关的订阅 */
    neu_subscribe_manager_remove(manager->subscribe_manager, node_name, NULL);
    /* 从节点管理器中删除 */
    neu_node_manager_del(manager->node_manager, node_name);
    return NEU_ERR_SUCCESS;
}

/**
 * @brief 获取节点列表
 *
 * 根据过滤条件获取符合条件的节点列表
 *
 * @param manager 管理器对象指针
 * @param type 节点类型过滤条件
 * @param plugin 插件名称过滤条件(可为NULL)
 * @param node 节点名称过滤条件(可为NULL)
 * @return 符合条件的节点列表数组
 */
UT_array *neu_manager_get_nodes(neu_manager_t *manager, int type,
                                const char *plugin, const char *node)
{
    return neu_node_manager_filter(manager->node_manager, type, plugin, node);
}

/**
 * @brief 更新节点名称
 *
 * 修改指定节点的名称，同时更新所有相关的订阅
 *
 * @param manager 管理器对象指针
 * @param node 当前节点名称
 * @param new_name 新的节点名称
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_update_node_name(neu_manager_t *manager, const char *node,
                                 const char *new_name)
{
    int ret = 0;
    /* 根据节点类型(驱动或应用)更新订阅管理器中的名称 */
    if (neu_node_manager_is_driver(manager->node_manager, node)) {
        ret = neu_subscribe_manager_update_driver_name(
            manager->subscribe_manager, node, new_name);
    } else {
        ret = neu_subscribe_manager_update_app_name(manager->subscribe_manager,
                                                    node, new_name);
    }
    /* 更新节点管理器中的名称 */
    if (0 == ret) {
        ret =
            neu_node_manager_update_name(manager->node_manager, node, new_name);
    }
    return ret;
}

/**
 * @brief 更新组名称
 *
 * 修改驱动节点中指定组的名称
 *
 * @param manager 管理器对象指针
 * @param driver 驱动节点名称
 * @param group 当前组名称
 * @param new_name 新的组名称
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_update_group_name(neu_manager_t *manager, const char *driver,
                                  const char *group, const char *new_name)
{
    return neu_subscribe_manager_update_group_name(manager->subscribe_manager,
                                                   driver, group, new_name);
}

/**
 * @brief 创建新的插件实例
 *
 * 分配内存并创建指定插件的实例
 *
 * @param plugin_manager 插件管理器指针
 * @param plugin 插件名称
 * @return 成功返回插件实例指针，失败返回NULL
 */
static inline neu_plugin_instance_t *
new_plugin_instance(neu_plugin_manager_t *plugin_manager, const char *plugin)
{
    /* 分配插件实例内存 */
    neu_plugin_instance_t *inst = calloc(1, sizeof(*inst));
    if (NULL == inst) {
        return NULL;
    }

    /* 创建插件实例 */
    if (0 != neu_plugin_manager_create_instance(plugin_manager, plugin, inst)) {
        free(inst);
        return NULL;
    }

    return inst;
}

/**
 * @brief 释放插件实例资源
 *
 * @param inst 要释放的插件实例指针
 */
static inline void free_plugin_instance(neu_plugin_instance_t *inst)
{
    if (inst) {
        dlclose(inst->handle); /* 关闭动态库句柄 */
        free(inst);
    }
}

/**
 * @brief 添加模板
 *
 * 创建并添加一个新的设备模板
 *
 * @param manager 管理器对象指针
 * @param name 模板名称
 * @param plugin 使用的插件名称
 * @param n_group 组数量
 * @param groups 组信息数组
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_add_template(neu_manager_t *manager, const char *name,
                             const char *plugin, uint16_t n_group,
                             neu_reqresp_template_group_t *groups)
{
    int rv = 0;

    /* 检查插件是否存在 */
    if (!neu_plugin_manager_exists(manager->plugin_manager, plugin)) {
        return NEU_ERR_PLUGIN_NOT_FOUND;
    }

    /* 检查插件是否为单例类型 */
    if (neu_plugin_manager_is_single(manager->plugin_manager, plugin)) {
        return NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE;
    }

    /* 创建插件实例以获取其功能 */
    neu_plugin_instance_t *plug_inst =
        new_plugin_instance(manager->plugin_manager, plugin);
    if (NULL == plug_inst) {
        return NEU_ERR_EINTERNAL;
    }

    /* 检查插件是否为驱动类型 */
    if (NEU_NA_TYPE_DRIVER != plug_inst->module->type) {
        free_plugin_instance(plug_inst);
        return NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE;
    }

    neu_template_t *tmpl = neu_template_new(name, plugin);
    if (NULL == tmpl) {
        free_plugin_instance(plug_inst);
        return NEU_ERR_EINTERNAL;
    }

    neu_template_set_tag_validator(
        tmpl, plug_inst->module->intf_funs->driver.tag_validator);

    for (int i = 0; i < n_group; ++i) {
        neu_reqresp_template_group_t *grp = &groups[i];
        if (0 !=
            (rv = neu_template_add_group(tmpl, grp->name, grp->interval))) {
            goto error;
        }

        for (int j = 0; j < grp->n_tag; ++j) {
            if (0 !=
                (rv = neu_template_add_tag(tmpl, grp->name, &grp->tags[j]))) {
                goto error;
            }
        }
    }

    rv = neu_template_manager_add(manager->template_manager, tmpl, plug_inst);
    return rv;

error:
    neu_template_free(tmpl);
    free_plugin_instance(plug_inst);
    return rv;
}

/**
 * @brief 删除模板
 *
 * 根据名称删除已存在的设备模板
 *
 * @param manager 管理器对象指针
 * @param name 要删除的模板名称
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_del_template(neu_manager_t *manager, const char *name)
{
    return neu_template_manager_del(manager->template_manager, name);
}

/**
 * @brief 清除所有模板
 *
 * 删除管理器中的所有模板
 *
 * @param manager 管理器对象指针
 */
void neu_manager_clear_template(neu_manager_t *manager)
{
    return neu_template_manager_clear(manager->template_manager);
}

/**
 * @brief 累积模板组信息
 *
 * 收集模板组信息并填充到响应结构中，作为回调函数使用
 *
 * @param grp 组对象指针
 * @param data 用户数据指针，指向响应结构
 * @return 始终返回0表示成功
 */
static int accumulate_template_group(neu_group_t *grp, void *data)
{
    neu_reqresp_template_t *resp = data;

    // 增加组计数
    neu_reqresp_template_group_t *resp_grp = &resp->groups[resp->n_group++];

    // 复制组名称和间隔
    strcpy(resp_grp->name, neu_group_get_name(grp));
    resp_grp->interval = neu_group_get_interval(grp);

    // 复制标签
    UT_array *tags  = neu_group_get_tag(grp);
    resp_grp->n_tag = utarray_len(tags);
    resp_grp->tags  = utarray_steal(tags);
    utarray_free(tags);

    return 0;
}

/**
 * @brief 获取模板信息
 *
 * 根据模板名称获取完整的模板详细信息，包括所有组和标签
 *
 * @param manager 管理器对象指针
 * @param name 模板名称
 * @param resp 用于返回模板信息的响应结构指针
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_get_template(neu_manager_t *manager, const char *name,
                             neu_resp_get_template_t *resp)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, name);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    neu_resp_get_template_t data  = { 0 };
    size_t                  count = neu_template_group_num(tmpl);
    if (0 == count) {
        goto end;
    }

    data.groups = calloc(count, sizeof(*data.groups));
    if (NULL == data.groups) {
        return -1;
    }

    int rv =
        neu_template_for_each_group(tmpl, accumulate_template_group, &data);
    if (0 != rv) {
        neu_reqresp_template_fini(&data);
        return rv;
    }

end:
    strncpy(data.name, neu_template_name(tmpl), sizeof(data.name));
    strncpy(data.plugin, neu_template_plugin(tmpl), sizeof(data.plugin));
    *resp = data;

    return 0;
}

/**
 * @brief 累积模板基本信息
 *
 * 收集模板基本信息并填充到响应结构中，作为回调函数使用
 *
 * @param tmpl 模板对象指针
 * @param data 用户数据指针，指向响应结构
 * @return 始终返回0表示成功
 */
static int accumulate_template(neu_template_t *tmpl, void *data)
{
    neu_resp_get_templates_t *resp = data;
    neu_resp_template_info_t *info = &resp->templates[resp->n_templates++];
    strncpy(info->name, neu_template_name(tmpl), sizeof(info->name));
    strncpy(info->plugin, neu_template_plugin(tmpl), sizeof(info->plugin));
    return 0;
}

/**
 * @brief 获取所有模板列表
 *
 * 获取系统中所有模板的基本信息列表
 *
 * @param manager 管理器对象指针
 * @param resp 用于返回模板列表的响应结构指针
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_get_templates(neu_manager_t            *manager,
                              neu_resp_get_templates_t *resp)
{
    int count = neu_template_manager_count(manager->template_manager);
    if (0 == count) {
        resp->n_templates = 0;
        return 0;
    }

    resp->templates = calloc(count, sizeof(*resp->templates));
    if (NULL == resp->templates) {
        return NEU_ERR_EINTERNAL;
    }

    // 保证成功执行
    neu_template_manager_for_each(manager->template_manager,
                                  accumulate_template, resp);
    return 0;
}

/**
 * @brief 添加模板组
 *
 * 向指定模板添加新的组
 *
 * @param manager 管理器对象指针
 * @param tmpl_name 模板名称
 * @param group 要添加的组名称
 * @param interval 组的采集间隔
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_add_template_group(neu_manager_t *manager,
                                   const char *tmpl_name, const char *group,
                                   uint32_t interval)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, tmpl_name);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    return neu_template_add_group(tmpl, group, interval);
}

/**
 * @brief 更新模板组信息
 *
 * 更新模板中指定组的名称和采集间隔
 *
 * @param manager 管理器对象指针
 * @param req 包含更新信息的请求结构指针
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_update_template_group(neu_manager_t                   *manager,
                                      neu_req_update_template_group_t *req)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, req->tmpl);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    int rv = 0;

    // 如果提供了新名称且不同于当前名称，则更新组名称
    if (strlen(req->new_name) > 0 && 0 != strcmp(req->group, req->new_name)) {
        rv = neu_template_update_group_name(tmpl, req->group, req->new_name);
        if (0 != rv) {
            return rv;
        }
    }

    // 如果间隔值有效，则更新组的采集间隔
    if (req->interval >= NEU_GROUP_INTERVAL_LIMIT) {
        rv = neu_template_update_group_interval(tmpl, req->new_name,
                                                req->interval);
    }

    return rv;
}

/**
 * @brief 删除模板组
 *
 * 从指定模板中删除一个组
 *
 * @param manager 管理器对象指针
 * @param req 包含删除信息的请求结构指针
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_del_template_group(neu_manager_t                *manager,
                                   neu_req_del_template_group_t *req)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, req->tmpl);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    return neu_template_del_group(tmpl, req->group);
}

/**
 * @brief 累积组信息
 *
 * 收集组信息并填充到数组中，作为回调函数使用
 *
 * @param grp 组对象指针
 * @param data 用户数据指针，指向UT_array指针
 * @return 始终返回0表示成功
 */
static int accumulate_group_info(neu_group_t *grp, void *data)
{
    UT_array            **vec  = data;
    neu_resp_group_info_t info = { 0 };

    // 收集组信息
    info.interval  = neu_group_get_interval(grp);
    info.tag_count = neu_group_tag_size(grp);
    strncpy(info.name, neu_group_get_name(grp), sizeof(info.name));

    utarray_push_back(*vec, &info);
    return 0;
}

/**
 * @brief 获取模板组信息
 *
 * 获取指定模板的所有组信息
 *
 * @param manager 管理器对象指针
 * @param req 包含请求信息的结构指针
 * @param group_info_p 用于返回组信息数组的指针
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_get_template_group(neu_manager_t                *manager,
                                   neu_req_get_template_group_t *req,
                                   UT_array                    **group_info_p)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, req->tmpl);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    UT_array *vec = NULL;
    UT_icd    icd = { sizeof(neu_resp_group_info_t), NULL, NULL, NULL };

    utarray_new(vec, &icd);
    if (NULL == vec) {
        return NEU_ERR_EINTERNAL;
    }

    int ret = neu_template_for_each_group(tmpl, accumulate_group_info, &vec);
    if (0 == ret) {
        *group_info_p = vec;
    } else {
        utarray_free(vec);
    }

    return ret;
}

/**
 * @brief 添加模板标签
 *
 * 向指定模板的组中添加多个标签
 *
 * @param manager 管理器对象指针
 * @param tmpl_name 模板名称
 * @param group 组名称
 * @param n_tag 标签数量
 * @param tags 标签数组
 * @param index_p 用于返回成功添加的标签数量的指针
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_add_template_tags(neu_manager_t *manager, const char *tmpl_name,
                                  const char *group, uint16_t n_tag,
                                  neu_datatag_t *tags, uint16_t *index_p)
{
    int ret = 0;

    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, tmpl_name);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    int i = 0;
    for (; i < n_tag; ++i) {
        if (0 != (ret = neu_template_add_tag(tmpl, group, &tags[i]))) {
            break;
        }
    }

    // 如果提供了index_p指针，返回成功添加的标签数量
    if (index_p) {
        *index_p = i;
    }

    return ret;
}

/**
 * @brief 更新模板标签
 *
 * 更新指定模板组中的多个标签
 *
 * @param manager 管理器对象指针
 * @param req 包含更新信息的请求结构指针
 * @param index_p 用于返回成功更新的标签数量的指针
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_update_template_tags(neu_manager_t                 *manager,
                                     neu_req_update_template_tag_t *req,
                                     uint16_t                      *index_p)
{
    int ret = 0;

    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, req->tmpl);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    int i = 0;
    for (; i < req->n_tag; ++i) {
        if (0 !=
            (ret = neu_template_update_tag(tmpl, req->group, &req->tags[i]))) {
            break;
        }
    }

    // 如果提供了index_p指针，返回成功更新的标签数量
    if (index_p) {
        *index_p = i;
    }

    return ret;
}

/**
 * @brief 删除模板标签
 *
 * 从指定模板组中删除多个标签
 *
 * @param manager 管理器对象指针
 * @param req 包含删除信息的请求结构指针
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_del_template_tags(neu_manager_t              *manager,
                                  neu_req_del_template_tag_t *req)
{
    int          ret = 0;
    neu_group_t *grp = NULL;

    ret = neu_template_manager_find_group(manager->template_manager, req->tmpl,
                                          req->group, &grp);
    if (0 != ret) {
        return ret;
    }

    int i = 0;
    for (; i < req->n_tag; ++i) {
        // 唯一可能的错误是标签不存在
        // 此时我们忽略错误并继续处理
        neu_group_del_tag(grp, req->tags[i]);
    }

    return 0;
}

/**
 * @brief 获取模板标签
 *
 * 获取指定模板组中的标签，可以根据名称过滤
 *
 * @param manager 管理器对象指针
 * @param req 包含请求信息的结构指针
 * @param tags_p 用于返回标签数组的指针
 * @return 成功返回0，失败返回错误码
 */
int neu_manager_get_template_tags(neu_manager_t              *manager,
                                  neu_req_get_template_tag_t *req,
                                  UT_array                  **tags_p)
{
    int          ret = 0;
    neu_group_t *grp = NULL;

    ret = neu_template_manager_find_group(manager->template_manager, req->tmpl,
                                          req->group, &grp);
    if (0 != ret) {
        return ret;
    }

    // 如果提供了标签名称，则根据名称查询特定标签
    // 否则获取组中的所有标签
    if (strlen(req->name) > 0) {
        *tags_p = neu_group_query_tag(grp, req->name);
    } else {
        *tags_p = neu_group_get_tag(grp);
    }

    return 0;
}

static int add_template_group(neu_group_t *grp, void *data)
{
    neu_adapter_driver_t *driver   = data;
    const char           *name     = neu_group_get_name(grp);
    uint32_t              interval = neu_group_get_interval(grp);

    if (interval < NEU_GROUP_INTERVAL_LIMIT) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    int ret = neu_adapter_driver_add_group(driver, name, interval);
    if (0 != ret) {
        return ret;
    }

    UT_array *tags = neu_group_get_tag(grp);
    if (NULL == tags) {
        return NEU_ERR_EINTERNAL;
    }

    ret = neu_adapter_driver_try_add_tag(driver, name, utarray_eltptr(tags, 0),
                                         utarray_len(tags));
    if (0 != ret) {
        utarray_free(tags);
        return ret;
    }

    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        // this invocation is not necessary since we validate when adding tags
        // we keep it here as an act of defensive programming
        ret = neu_adapter_driver_validate_tag(driver, name, tag);
        if (0 != ret) {
            break;
        }

        ret = neu_adapter_driver_add_tag(driver, name, tag);
        if (0 != ret) {
            break;
        }
    }

    // we do not call neu_adapter_driver_try_del_tag here,
    // relying on neu_adapter_driver_uninit to delete the tags

    utarray_free(tags);
    return ret;
}

int neu_manager_instantiate_template(neu_manager_t *manager,
                                     const char    *tmpl_name,
                                     const char    *node_name)
{
    int ret = 0;

    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, tmpl_name);
    if (NULL == tmpl) {
        ret = NEU_ERR_TEMPLATE_NOT_FOUND;
        goto end;
    }

    ret = neu_manager_add_node(manager, node_name, neu_template_plugin(tmpl),
                               false, false);
    if (0 != ret) {
        goto end;
    }

    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, node_name);
    neu_adapter_driver_t *driver = (neu_adapter_driver_t *) adapter;

    if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
        ret = NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE;
        neu_adapter_uninit(adapter);
        neu_manager_del_node(manager, node_name);
        goto end;
    }

    ret = neu_template_for_each_group(tmpl, add_template_group, driver);
    if (0 != ret) {
        // this will call neu_adapter_driver_try_del_tag to cleanup
        neu_adapter_uninit(adapter);
        neu_manager_del_node(manager, node_name);
    }

end:
    if (0 == ret) {
        nlog_notice("instantiate tmpl:%s node:%s success", tmpl_name,
                    node_name);
    } else {
        nlog_error("instantiate tmpl:%s node:%s ret:%d", tmpl_name, node_name,
                   ret);
    }
    return ret;
}

int neu_manager_instantiate_templates(neu_manager_t            *manager,
                                      neu_req_inst_templates_t *req)
{
    int ret = 0;

    for (int i = 0; i < req->n_inst; ++i) {
        ret = neu_manager_instantiate_template(manager, req->insts[i].tmpl,
                                               req->insts[i].node);
        if (0 != ret) {
            // revert on error, destroy nodes instantiated thus far
            while (--i >= 0) {
                neu_adapter_t *adapter = neu_node_manager_find(
                    manager->node_manager, req->insts[i].node);
                // this will call neu_adapter_driver_try_del_tag to cleanup
                neu_adapter_uninit(adapter);
                neu_manager_del_node(manager, req->insts[i].node);
            }
            break;
        }
    }

    return ret;
}

UT_array *neu_manager_get_driver_group(neu_manager_t *manager)
{
    UT_array *drivers =
        neu_node_manager_get(manager->node_manager, NEU_NA_TYPE_DRIVER);
    UT_array *driver_groups = NULL;
    UT_icd    icd = { sizeof(neu_resp_driver_group_info_t), NULL, NULL, NULL };

    utarray_new(driver_groups, &icd);

    utarray_foreach(drivers, neu_resp_node_info_t *, driver)
    {
        neu_adapter_t *adapter =
            neu_node_manager_find(manager->node_manager, driver->node);
        UT_array *groups =
            neu_adapter_driver_get_group((neu_adapter_driver_t *) adapter);

        utarray_foreach(groups, neu_resp_group_info_t *, g)
        {
            neu_resp_driver_group_info_t dg = { 0 };

            strcpy(dg.driver, driver->node);
            strcpy(dg.group, g->name);
            dg.interval  = g->interval;
            dg.tag_count = g->tag_count;

            utarray_push_back(driver_groups, &dg);
        }

        utarray_free(groups);
    }

    utarray_free(drivers);

    return driver_groups;
}

static inline int manager_subscribe(neu_manager_t *manager, const char *app,
                                    const char *driver, const char *group,
                                    const char *params)
{
    int                ret  = NEU_ERR_SUCCESS;
    struct sockaddr_in addr = { 0 };
    neu_adapter_t     *adapter =
        neu_node_manager_find(manager->node_manager, driver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    ret =
        neu_adapter_driver_group_exist((neu_adapter_driver_t *) adapter, group);
    if (ret != NEU_ERR_SUCCESS) {
        return ret;
    }

    addr = neu_node_manager_get_addr(manager->node_manager, app);
    return neu_subscribe_manager_sub(manager->subscribe_manager, driver, app,
                                     group, params, addr);
}

int neu_manager_subscribe(neu_manager_t *manager, const char *app,
                          const char *driver, const char *group,
                          const char *params, uint16_t *app_port)
{
    if (neu_node_manager_is_monitor(manager->node_manager, app)) {
        // filter out monitor node
        return NEU_ERR_NODE_NOT_ALLOW_SUBSCRIBE;
    }

    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, app);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    *app_port = neu_adapter_trans_data_port(adapter);

    // guard against empty mqtt topic parameter
    // this is not an elegant solution due to the current architecture
    if (params && 0 == strcmp(adapter->module->module_name, "MQTT")) {
        neu_json_elem_t elem = { .name      = "topic",
                                 .t         = NEU_JSON_STR,
                                 .v.val_str = NULL };
        int             ret  = neu_parse_param(params, NULL, 1, &elem);
        if (ret != 0 || (elem.v.val_str && 0 == strlen(elem.v.val_str))) {
            free(elem.v.val_str);
            return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
        }
        free(elem.v.val_str);
    }

    if (NEU_NA_TYPE_APP != neu_adapter_get_type(adapter)) {
        return NEU_ERR_NODE_NOT_ALLOW_SUBSCRIBE;
    }

    return manager_subscribe(manager, app, driver, group, params);
}

int neu_manager_update_subscribe(neu_manager_t *manager, const char *app,
                                 const char *driver, const char *group,
                                 const char *params)
{
    return neu_subscribe_manager_update_params(manager->subscribe_manager, app,
                                               driver, group, params);
}

int neu_manager_send_subscribe(neu_manager_t *manager, const char *app,
                               const char *driver, const char *group,
                               uint16_t app_port, const char *params)
{
    neu_req_subscribe_t cmd = { 0 };
    strcpy(cmd.app, app);
    strcpy(cmd.driver, driver);
    strcpy(cmd.group, group);
    cmd.port = app_port;

    if (params && NULL == (cmd.params = strdup(params))) {
        return NEU_ERR_EINTERNAL;
    }

    neu_msg_t *msg = neu_msg_new(NEU_REQ_SUBSCRIBE_GROUP, NULL, &cmd);
    if (NULL == msg) {
        free(cmd.params);
        return NEU_ERR_EINTERNAL;
    }
    neu_reqresp_head_t *header = neu_msg_get_header(msg);
    strcpy(header->sender, "manager");
    strcpy(header->receiver, app);

    struct sockaddr_in addr =
        neu_node_manager_get_addr(manager->node_manager, app);

    int ret = neu_send_msg_to(manager->server_fd, &addr, msg);
    if (0 != ret) {
        nlog_warn("send %s to %s app failed",
                  neu_reqresp_type_string(NEU_REQ_SUBSCRIBE_GROUP), app);
        free(cmd.params);
        neu_msg_free(msg);
    } else {
        nlog_notice("send %s to %s app",
                    neu_reqresp_type_string(NEU_REQ_SUBSCRIBE_GROUP), app);
    }
    cmd.params = NULL;

    msg = neu_msg_new(NEU_REQ_SUBSCRIBE_GROUP, NULL, &cmd);
    if (NULL == msg) {
        return NEU_ERR_EINTERNAL;
    }
    header = neu_msg_get_header(msg);
    strcpy(header->sender, "manager");
    strcpy(header->receiver, driver);
    addr = neu_node_manager_get_addr(manager->node_manager, driver);

    ret = neu_send_msg_to(manager->server_fd, &addr, msg);
    if (0 != ret) {
        nlog_warn("send %s to %s driver failed",
                  neu_reqresp_type_string(NEU_REQ_SUBSCRIBE_GROUP), driver);
        neu_msg_free(msg);
    } else {
        nlog_notice("send %s to %s driver",
                    neu_reqresp_type_string(NEU_REQ_SUBSCRIBE_GROUP), driver);
    }

    return 0;
}

int neu_manager_unsubscribe(neu_manager_t *manager, const char *app,
                            const char *driver, const char *group)
{
    return neu_subscribe_manager_unsub(manager->subscribe_manager, driver, app,
                                       group);
}

UT_array *neu_manager_get_sub_group(neu_manager_t *manager, const char *app)
{
    return neu_subscribe_manager_get(manager->subscribe_manager, app);
}

UT_array *neu_manager_get_sub_group_deep_copy(neu_manager_t *manager,
                                              const char    *app)
{
    UT_array *subs = neu_subscribe_manager_get(manager->subscribe_manager, app);

    utarray_foreach(subs, neu_resp_subscribe_info_t *, sub)
    {
        if (sub->params) {
            sub->params = strdup(sub->params);
        }
    }

    // set vector element destructor
    subs->icd.dtor = (void (*)(void *)) neu_resp_subscribe_info_fini;

    return subs;
}

int neu_manager_add_ndriver_map(neu_manager_t *manager, const char *ndriver,
                                const char *driver, const char *group)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, ndriver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (NEU_NA_TYPE_NDRIVER != neu_adapter_get_type(adapter)) {
        return NEU_ERR_NODE_NOT_ALLOW_MAP;
    }

    return manager_subscribe(manager, ndriver, driver, group, NULL);
}

int neu_manager_del_ndriver_map(neu_manager_t *manager, const char *ndriver,
                                const char *driver, const char *group)
{
    return neu_manager_unsubscribe(manager, ndriver, driver, group);
}

int neu_manager_get_ndriver_maps(neu_manager_t *manager, const char *ndriver,
                                 UT_array **result)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, ndriver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    } else if (NEU_NA_TYPE_NDRIVER != neu_adapter_get_type(adapter)) {
        return NEU_ERR_NODE_NOT_ALLOW_MAP;
    }

    *result = neu_subscribe_manager_get_ndriver_maps(manager->subscribe_manager,
                                                     ndriver);
    return 0;
}

int neu_manager_get_node_info(neu_manager_t *manager, const char *name,
                              neu_persist_node_info_t *info)
{
    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, name);

    if (adapter != NULL) {
        info->name        = strdup(name);
        info->type        = adapter->module->type;
        info->plugin_name = strdup(adapter->module->module_name);
        info->state       = adapter->state;
        return 0;
    }

    return -1;
}
