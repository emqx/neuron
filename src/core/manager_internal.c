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
#include "adapter/adapter_internal.h"
#include "adapter/driver/driver_internal.h"

#include "manager_internal.h"

int neu_manager_add_plugin(neu_manager_t *manager, const char *library)
{
    return neu_plugin_manager_add(manager->plugin_manager, library);
}

int neu_manager_del_plugin(neu_manager_t *manager, const char *plugin)
{
    return neu_plugin_manager_del(manager->plugin_manager, plugin);
}

UT_array *neu_manager_get_plugins(neu_manager_t *manager)
{
    return neu_plugin_manager_get(manager->plugin_manager);
}

int neu_manager_add_node(neu_manager_t *manager, const char *node_name,
                         const char *plugin_name)
{
    neu_adapter_t *       adapter      = NULL;
    neu_plugin_instance_t instance     = { 0 };
    neu_adapter_info_t    adapter_info = {
        .name = node_name,
    };
    neu_resp_plugin_info_t info = { 0 };
    int                    ret =
        neu_plugin_manager_find(manager->plugin_manager, plugin_name, &info);

    if (ret != 0) {
        return NEU_ERR_LIBRARY_NOT_FOUND;
    }

    adapter = neu_node_manager_find(manager->node_manager, node_name);
    if (adapter != NULL) {
        return NEU_ERR_NODE_EXIST;
    }

    ret = neu_plugin_manager_create_instance(manager->plugin_manager,
                                             plugin_name, &instance);
    adapter_info.handle = instance.handle;
    adapter_info.module = instance.module;

    adapter     = neu_adapter_create(&adapter_info, manager);
    adapter->id = neu_node_manager_add(manager->node_manager, adapter);
    neu_adapter_init(adapter);

    return NEU_ERR_SUCCESS;
}

int neu_manager_del_node(neu_manager_t *manager, const char *node_name)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, node_name);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    neu_adapter_uninit(adapter);
    neu_adapter_destroy(adapter);

    neu_subscribe_manager_remove(manager->subscribe_manager, node_name, NULL);
    neu_node_manager_del(manager->node_manager, node_name);
    return NEU_ERR_SUCCESS;
}

UT_array *neu_manager_get_nodes(neu_manager_t *manager, neu_node_type_e type)
{
    return neu_node_manager_get(manager->node_manager, type);
}

int neu_manager_add_group(neu_manager_t *manager, const char *driver,
                          const char *group, uint32_t interval)
{
    if (interval < NEU_GROUP_INTERVAL_LIMIT) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    if (strlen(group) > NEU_GROUP_NAME_LEN) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);
    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_info.module->type != NEU_NA_TYPE_DRIVER) {
        return NEU_ERR_GROUP_NOT_ALLOW;
    }

    return neu_adapter_driver_add_group((neu_adapter_driver_t *) adapter, group,
                                        interval);
}

int neu_manager_del_group(neu_manager_t *manager, const char *driver,
                          const char *group)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);
    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_info.module->type != NEU_NA_TYPE_DRIVER) {
        return NEU_ERR_GROUP_NOT_ALLOW;
    }

    neu_subscribe_manager_remove(manager->subscribe_manager, driver, group);
    return neu_adapter_driver_del_group((neu_adapter_driver_t *) adapter,
                                        group);
}

int neu_manager_update_group(neu_manager_t *manager, const char *driver,
                             const char *group, uint32_t interval)
{
    if (interval < NEU_GROUP_INTERVAL_LIMIT) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }
    (void) manager;
    (void) driver;
    (void) group;
    (void) interval;

    return 0;
}

int neu_manager_get_group(neu_manager_t *manager, const char *driver,
                          UT_array **groups)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_info.module->type != NEU_NA_TYPE_DRIVER) {
        return NEU_ERR_GROUP_NOT_ALLOW;
    }

    *groups = neu_adapter_driver_get_group((neu_adapter_driver_t *) adapter);

    return NEU_ERR_SUCCESS;
}

int neu_manager_add_tag(neu_manager_t *manager, const char *driver,
                        const char *group, uint16_t n_tag, neu_datatag_t *tags,
                        uint16_t *index)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);
    *index = 0;

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_info.module->type != NEU_NA_TYPE_DRIVER) {
        return NEU_ERR_GROUP_NOT_ALLOW;
    }

    for (int i = 0; i < n_tag; i++) {
        int ret = neu_adapter_driver_add_tag((neu_adapter_driver_t *) adapter,
                                             group, &tags[i]);
        if (ret == NEU_ERR_SUCCESS) {
            *index += 1;
        } else {
            return ret;
        }
    }

    return NEU_ERR_SUCCESS;
}

int neu_manager_del_tag(neu_manager_t *manager, const char *driver,
                        const char *group, uint16_t n_tag, char **tags)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_info.module->type != NEU_NA_TYPE_DRIVER) {
        return NEU_ERR_GROUP_NOT_ALLOW;
    }

    for (int i = 0; i < n_tag; i++) {
        neu_adapter_driver_del_tag((neu_adapter_driver_t *) adapter, group,
                                   tags[i]);
    }

    return NEU_ERR_SUCCESS;
}

int neu_manager_update_tag(neu_manager_t *manager, const char *driver,
                           const char *group, uint16_t n_tag,
                           neu_datatag_t *tags, uint16_t *index)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);
    *index = 0;

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_info.module->type != NEU_NA_TYPE_DRIVER) {
        return NEU_ERR_GROUP_NOT_ALLOW;
    }

    for (int i = 0; i < n_tag; i++) {
        int ret = neu_adapter_driver_update_tag(
            (neu_adapter_driver_t *) adapter, group, &tags[i]);
        if (ret == NEU_ERR_SUCCESS) {
            *index += 1;
        } else {
            return ret;
        }
    }

    return NEU_ERR_SUCCESS;
}

int neu_manager_get_tag(neu_manager_t *manager, const char *driver,
                        const char *group, UT_array **tags)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_info.module->type != NEU_NA_TYPE_DRIVER) {
        return NEU_ERR_GROUP_NOT_ALLOW;
    }

    return neu_adapter_driver_get_tag((neu_adapter_driver_t *) adapter, group,
                                      tags);
}

int neu_manager_subscribe(neu_manager_t *manager, const char *app,
                          const char *driver, const char *group)
{
    int            ret  = NEU_ERR_SUCCESS;
    nng_pipe       pipe = { 0 };
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    ret =
        neu_adapter_driver_group_exist((neu_adapter_driver_t *) adapter, group);
    if (ret != NEU_ERR_SUCCESS) {
        return ret;
    }

    adapter = neu_node_manager_find(manager->node_manager, app);
    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    pipe = neu_node_manager_get_pipe(manager->node_manager, app);
    return neu_subscribe_manager_sub(manager->subscribe_manager, driver, app,
                                     group, pipe);
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

int neu_manager_get_node_state(neu_manager_t *manager, const char *node,
                               neu_plugin_state_t *state)
{
    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, node);
    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    *state = neu_adapter_get_state(adapter);

    return NEU_ERR_SUCCESS;
}

int neu_manager_node_ctl(neu_manager_t *manager, const char *node,
                         neu_adapter_ctl_e ctl)
{
    int            ret     = NEU_ERR_SUCCESS;
    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, node);
    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    switch (ctl) {
    case NEU_ADAPTER_CTL_START:
        ret = neu_adapter_start(adapter);
        break;
    case NEU_ADAPTER_CTL_STOP:
        ret = neu_adapter_stop(adapter);
        break;
    }

    return ret;
}

int neu_manager_node_setting(neu_manager_t *manager, const char *node,
                             const char *setting)
{
    neu_config_t   config  = { 0 };
    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, node);
    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    config.type    = NEU_CONFIG_SETTING;
    config.buf     = (char *) setting;
    config.buf_len = strlen(setting);

    return neu_adapter_set_setting(adapter, &config);
}

int neu_manager_node_get_setting(neu_manager_t *manager, const char *node,
                                 char **setting)
{
    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, node);
    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    return neu_adapter_get_setting(adapter, setting);
}

int neu_manager_get_adapter_info(neu_manager_t *manager, const char *name,
                                 neu_persist_adapter_info_t *info)
{
    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, name);

    if (adapter != NULL) {
        info->name        = strdup(name);
        info->type        = adapter->plugin_info.module->type;
        info->plugin_name = strdup(adapter->plugin_info.module->module_name);
        info->state       = adapter->state;
        return 0;
    }

    return -1;
}
