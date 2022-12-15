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

#include "adapter.h"
#include "errcodes.h"

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
                         const char *plugin_name, bool start)
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

    if (info.single) {
        return NEU_ERR_LIBRARY_NOT_ALLOW_CREATE_INSTANCE;
    }

    adapter = neu_node_manager_find(manager->node_manager, node_name);
    if (adapter != NULL) {
        return NEU_ERR_NODE_EXIST;
    }

    ret = neu_plugin_manager_create_instance(manager->plugin_manager,
                                             plugin_name, &instance);
    if (ret != 0) {
        return NEU_ERR_LIBRARY_FAILED_TO_OPEN;
    }
    adapter_info.handle = instance.handle;
    adapter_info.module = instance.module;

    adapter = neu_adapter_create(&adapter_info);
    neu_node_manager_add(manager->node_manager, adapter);
    neu_adapter_init(adapter, start);

    if (instance.module->type == NEU_NA_TYPE_APP) {
        for (int i = 0; i < NEU_APP_SUBSCRIBE_MSG_SIZE; i++) {
            if (instance.module->sub_msg[i] != 0) {
                neu_sub_msg_manager_add(manager->sub_msg_manager,
                                        instance.module->sub_msg[i],
                                        adapter->name);
            }
        }
    }

    return NEU_ERR_SUCCESS;
}

int neu_manager_del_node(neu_manager_t *manager, const char *node_name)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, node_name);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->module->type == NEU_NA_TYPE_APP) {
        for (int i = 0; i < NEU_APP_SUBSCRIBE_MSG_SIZE; i++) {
            if (adapter->module->sub_msg[i] != 0) {
                neu_sub_msg_manager_del(manager->sub_msg_manager,
                                        adapter->module->sub_msg[i],
                                        adapter->name);
            }
        }
    }

    neu_adapter_destroy(adapter);
    neu_subscribe_manager_remove(manager->subscribe_manager, node_name, NULL);
    neu_node_manager_del(manager->node_manager, node_name);
    return NEU_ERR_SUCCESS;
}

UT_array *neu_manager_get_nodes(neu_manager_t *manager, neu_node_type_e type,
                                const char *plugin, const char *node)
{
    return neu_node_manager_filter(manager->node_manager, type, plugin, node);
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

void neu_manager_count_tag(neu_manager_t *manager)
{
    UT_array *drivers =
        neu_node_manager_get_adapter(manager->node_manager, NEU_NA_TYPE_DRIVER);
    uint32_t all_size = 0;

    utarray_foreach(drivers, neu_adapter_t **, p_driver)
    {
        uint16_t size =
            neu_adapter_driver_tag_size((neu_adapter_driver_t *) (*p_driver));
        all_size += (uint32_t) size;
    }

    utarray_foreach(drivers, neu_adapter_t **, p_driver)
    {
        neu_adapter_driver_set_all_tag_size(
            (neu_adapter_driver_t *) (*p_driver), all_size);
    }

    utarray_free(drivers);
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

    if (0 == strcmp(app, "monitor")) {
        // filter out monitor node
        return NEU_ERR_NODE_NOT_ALLOW_SUBSCRIBE;
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
