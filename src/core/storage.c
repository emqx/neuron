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
#include "errcodes.h"
#include "utils/log.h"

#include "adapter/storage.h"
#include "storage.h"

void manager_strorage_plugin(neu_manager_t *manager)
{
    UT_array *plugin_infos = NULL;

    plugin_infos = neu_manager_get_plugins(manager);

    int rv = neu_persister_store_plugins(plugin_infos);
    if (0 != rv) {
        nlog_error("failed to store plugins infos");
    }

    neu_persist_plugin_infos_free(plugin_infos);
    return;
}

void manager_storage_add_node(neu_manager_t *manager, const char *node)
{
    int                     rv        = 0;
    neu_persist_node_info_t node_info = {};

    rv = neu_manager_get_node_info(manager, node, &node_info);
    if (0 != rv) {
        nlog_error("unable to get adapter:%s info", node);
        return;
    }

    rv = neu_persister_store_node(&node_info);
    if (0 != rv) {
        nlog_error("failed to store adapter info");
    }

    neu_persist_node_info_fini(&node_info);
}

void manager_storage_update_node(neu_manager_t *manager, const char *node,
                                 const char *new_name)
{
    (void) manager;
    neu_persister_update_node(node, new_name);
}

void manager_storage_del_node(neu_manager_t *manager, const char *node)
{
    (void) manager;
    neu_persister_delete_node(node);
}

void manager_storage_subscribe(neu_manager_t *manager, const char *app,
                               const char *driver, const char *group,
                               const char *params, const char *static_tags)
{
    (void) manager;
    int rv = neu_persister_store_subscription(app, driver, group, params,
                                              static_tags);
    if (0 != rv) {
        nlog_error("fail store subscription app:%s driver:%s group:%s", app,
                   driver, group);
    }
}

void manager_storage_update_subscribe(neu_manager_t *manager, const char *app,
                                      const char *driver, const char *group,
                                      const char *params)
{
    (void) manager;
    int rv = neu_persister_update_subscription(app, driver, group, params);
    if (0 != rv) {
        nlog_error("fail update subscription app:%s driver:%s group:%s", app,
                   driver, group);
    }
}

void manager_storage_unsubscribe(neu_manager_t *manager, const char *app,
                                 const char *driver, const char *group)
{
    (void) manager;
    int rv = neu_persister_delete_subscription(app, driver, group);
    if (0 != rv) {
        nlog_error("fail delete subscription app:%s driver:%s group:%s", app,
                   driver, group);
    }
}

int manager_load_plugin(neu_manager_t *manager)
{
    UT_array *plugin_infos = NULL;

    int rv = neu_persister_load_plugins(&plugin_infos);
    if (rv != 0) {
        return rv;
    }

    utarray_foreach(plugin_infos, char **, name)
    {
        rv                    = neu_manager_add_plugin(manager, *name);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        nlog_notice("load plugin %s, lib:%s", ok_or_err, *name);
    }

    utarray_foreach(plugin_infos, char **, name) { free(*name); }
    utarray_free(plugin_infos);

    return rv;
}

int manager_load_node(neu_manager_t *manager)
{
    UT_array *node_infos = NULL;
    int       rv         = 0;

    rv = neu_persister_load_nodes(&node_infos);
    if (0 != rv) {
        nlog_error("failed to load adapter infos");
        return -1;
    }

    utarray_foreach(node_infos, neu_persist_node_info_t *, node_info)
    {
        rv                    = neu_manager_add_node(manager, node_info->name,
                                  node_info->plugin_name, NULL,
                                  node_info->state, true);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        nlog_notice("load adapter %s type:%d, name:%s plugin:%s state:%d",
                    ok_or_err, node_info->type, node_info->name,
                    node_info->plugin_name, node_info->state);
    }

    utarray_free(node_infos);
    return rv;
}

int manager_load_subscribe(neu_manager_t *manager)
{
    UT_array *nodes =
        neu_node_manager_get(manager->node_manager, NEU_NA_TYPE_APP);

    utarray_foreach(nodes, neu_resp_node_info_t *, node)
    {
        int       rv        = 0;
        UT_array *sub_infos = NULL;

        rv = neu_persister_load_subscriptions(node->node, &sub_infos);
        if (0 != rv) {
            nlog_warn("load %s subscribetion infos error", node->node);
        } else {
            utarray_foreach(sub_infos, neu_persist_subscription_info_t *, info)
            {
                uint16_t app_port = 0;
                rv                = neu_manager_subscribe(
                    manager, node->node, info->driver_name, info->group_name,
                    info->params, info->static_tags, &app_port);
                const char *ok_or_err = (0 == rv) ? "success" : "fail";
                nlog_notice("%s load subscription app:%s driver:%s grp:%s",
                            ok_or_err, node->node, info->driver_name,
                            info->group_name);
                if (0 == rv) {
                    neu_manager_send_subscribe(
                        manager, node->node, info->driver_name,
                        info->group_name, app_port, info->params);
                }
            }

            utarray_free(sub_infos);
        }
    }

    utarray_free(nodes);
    return 0;
}
