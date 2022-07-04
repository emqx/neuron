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
    if (ret != 0) {
        return NEU_ERR_LIBRARY_FAILED_TO_OPEN;
    }
    adapter_info.handle = instance.handle;
    adapter_info.module = instance.module;

    adapter = neu_adapter_create(&adapter_info, true);
    neu_node_manager_add(manager->node_manager, adapter);
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

    neu_adapter_destroy(adapter);
    neu_subscribe_manager_remove(manager->subscribe_manager, node_name, NULL);
    neu_node_manager_del(manager->node_manager, node_name);
    return NEU_ERR_SUCCESS;
}

UT_array *neu_manager_get_nodes(neu_manager_t *manager, neu_node_type_e type)
{
    return neu_node_manager_get(manager->node_manager, type);
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

int neu_manager_get_adapter_info(neu_manager_t *manager, const char *name,
                                 neu_persist_adapter_info_t *info)
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

void neu_manager_notify_app_sub(neu_manager_t *manager, const char *app,
                                const char *driver, const char *group)
{
    nng_msg *                     msg;
    neu_reqresp_head_t            header = { 0 };
    neu_req_app_subscribe_group_t sub    = { 0 };
    nng_pipe pipe = neu_node_manager_get_pipe(manager->node_manager, driver);

    header.type = NEU_REQ_APP_SUBSCRIBE_GROUP;
    strcpy(header.sender, app);
    strcpy(header.receiver, driver);

    strcpy(sub.driver, driver);
    strcpy(sub.group, group);

    msg = neu_msg_gen(&header, (void *) &sub);
    nng_msg_set_pipe(msg, pipe);
    if (nng_sendmsg(manager->socket, msg, 0) != 0) {
        nng_msg_free(msg);
        nlog_warn("%s -> %s req app subscribe group send fail", app, driver);
    }
}

void neu_manager_notify_app_unsub(neu_manager_t *manager, const char *app,
                                  const char *driver, const char *group)
{
    nng_msg *                       msg;
    neu_reqresp_head_t              header = { 0 };
    neu_req_app_unsubscribe_group_t unsub  = { 0 };
    nng_pipe pipe = neu_node_manager_get_pipe(manager->node_manager, app);

    header.type = NEU_REQ_APP_UNSUBSCRIBE_GROUP;
    strcpy(header.sender, "manager");
    strcpy(header.receiver, app);

    strcpy(unsub.driver, driver);
    strcpy(unsub.group, group);

    msg = neu_msg_gen(&header, (void *) &unsub);
    nng_msg_set_pipe(msg, pipe);
    if (nng_sendmsg(manager->socket, msg, 0) != 0) {
        nng_msg_free(msg);
        nlog_warn("manager -> %s req app unsubscribe group send fail", app);
    }
}

void neu_manager_notify_app_unsub_update(neu_manager_t *manager,
                                         const char *driver, const char *group)
{
    UT_array *apps =
        neu_subscribe_manager_find(manager->subscribe_manager, driver, group);
    if (apps != NULL) {
        utarray_foreach(apps, neu_app_subscribe_t *, app)
        {
            neu_manager_notify_app_unsub(manager, app->app_name, driver, group);
        }
        utarray_free(apps);
    }
}

void neu_manager_notify_app_unsub_driver_update(neu_manager_t *manager,
                                                const char *   driver)
{
    UT_array *apps = neu_subscribe_manager_find_by_driver(
        manager->subscribe_manager, driver);
    if (apps != NULL) {
        utarray_foreach(apps, neu_app_subscribe_t *, app)
        {
            neu_manager_notify_app_unsub(manager, app->app_name, driver, "");
        }
        utarray_free(apps);
    }
}

void neu_manager_notify_app_sub_update(neu_manager_t *manager,
                                       const char *driver, const char *group)
{
    UT_array *apps =
        neu_subscribe_manager_find(manager->subscribe_manager, driver, group);
    if (apps != NULL) {
        utarray_foreach(apps, neu_app_subscribe_t *, app)
        {
            neu_manager_notify_app_sub(manager, app->app_name, driver, group);
        }
        utarray_free(apps);
    }
}