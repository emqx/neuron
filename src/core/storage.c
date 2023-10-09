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
                               const char *params)
{
    (void) manager;
    int rv = neu_persister_store_subscription(app, driver, group, params);
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

void manager_storage_add_ndriver_map(neu_manager_t *manager,
                                     const char *ndriver, const char *driver,
                                     const char *group)
{
    return manager_storage_subscribe(manager, ndriver, driver, group, NULL);
}

void manager_storage_del_ndriver_map(neu_manager_t *manager,
                                     const char *ndriver, const char *driver,
                                     const char *group)
{
    return manager_storage_unsubscribe(manager, ndriver, driver, group);
}

static int save_node_group_and_tags(neu_group_t *grp, void *data)
{
    const char *node     = data;
    const char *name     = neu_group_get_name(grp);
    uint32_t    interval = neu_group_get_interval(grp);

    UT_array *tags = neu_group_get_tag(grp);
    if (NULL == tags) {
        return NEU_ERR_EINTERNAL;
    }

    adapter_storage_add_group(node, name, interval);
    adapter_storage_add_tags(node, name, utarray_front(tags),
                             utarray_len(tags));

    utarray_free(tags);
    return 0;
}

void manager_storage_inst_node(neu_manager_t *manager, const char *tmpl_name,
                               const char *node)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, tmpl_name);
    if (NULL == tmpl) {
        nlog_error("save instantiated node `%s`, fail, no template `%s`", node,
                   tmpl_name);
        return;
    }

    manager_storage_add_node(manager, node);

    int rv = neu_template_for_each_group(tmpl, save_node_group_and_tags,
                                         (void *) node);
    if (0 != rv) {
        nlog_error("save instantiated node `%s` groups and tags fail", node);
    }
}

static int save_tmpl_group_and_tags(neu_group_t *grp, void *data)
{
    const char *tmpl_name = data;
    const char *name      = neu_group_get_name(grp);
    uint32_t    interval  = neu_group_get_interval(grp);

    UT_array *tags = neu_group_get_tag(grp);
    if (NULL == tags) {
        return NEU_ERR_EINTERNAL;
    }

    manager_storage_add_template_group(tmpl_name, name, interval);
    manager_storage_add_template_tags(tmpl_name, name, utarray_front(tags),
                                      utarray_len(tags));

    utarray_free(tags);
    return 0;
}

void manager_storage_add_template(neu_manager_t *manager, const char *tmpl_name)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, tmpl_name);
    if (NULL == tmpl) {
        nlog_error("no template `%s` to fail", tmpl_name);
        return;
    }

    int rv = neu_persister_store_template(tmpl_name, neu_template_plugin(tmpl));
    if (0 != rv) {
        nlog_error("failed to store template info");
        return;
    }

    rv = neu_template_for_each_group(tmpl, save_tmpl_group_and_tags,
                                     (void *) tmpl_name);
    if (0 != rv) {
        nlog_error("save template `%s` groups and tags fail", tmpl_name);
    }
}

void manager_storage_del_template(neu_manager_t *manager, const char *tmpl_name)
{
    (void) manager;

    int rv = neu_persister_delete_template(tmpl_name);
    if (0 != rv) {
        nlog_error("failed to delete template info");
    }
}

void manager_storage_clear_templates(neu_manager_t *manager)
{
    (void) manager;

    int rv = neu_persister_clear_templates();
    if (0 != rv) {
        nlog_error("failed to clear templates info");
    }
}

void manager_storage_add_template_group(const char *tmpl, const char *group,
                                        uint32_t interval)
{
    neu_persist_group_info_t info = {
        .name     = (char *) group,
        .interval = interval,
    };

    int rv = neu_persister_store_template_group(tmpl, &info);
    if (0 != rv) {
        nlog_error("store template:%s grp:%s, interval:%" PRIu32 " fail", tmpl,
                   group, interval);
    }
}

void manager_storage_update_template_group(const char *tmpl, const char *group,
                                           const char *new_name,
                                           uint32_t    interval)
{
    neu_persist_group_info_t info = {
        .name     = (char *) new_name,
        .interval = interval,
    };

    int rv = neu_persister_update_template_group(tmpl, group, &info);
    if (0 != rv) {
        nlog_error("update template:%s group:%s, new_name:%s interval:%" PRIu32
                   " fail",
                   tmpl, group, new_name, interval);
    }
}

void manager_storage_del_template_group(const char *tmpl, const char *group)
{
    int rv = neu_persister_delete_template_group(tmpl, group);
    if (0 != rv) {
        nlog_error("delete template:%s grp:%s fail" PRIu32, tmpl, group);
    }
}

void manager_storage_add_template_tag(const char *tmpl, const char *group,
                                      const neu_datatag_t *tag)
{
    int rv = neu_persister_store_template_tags(tmpl, group, tag, 1);
    if (0 != rv) {
        nlog_error("store tag:%s template:%s grp:%s fail", tag->name, tmpl,
                   group);
    }
}

void manager_storage_add_template_tags(const char *tmpl, const char *group,
                                       const neu_datatag_t *tags, size_t n)
{
    if (0 == n) {
        return;
    }

    if (1 == n) {
        return manager_storage_add_template_tag(tmpl, group, &tags[0]);
    }

    int rv = neu_persister_store_template_tags(tmpl, group, tags, n);
    if (0 != rv) {
        nlog_error("store %zu tags:[%s ... %s] template:%s grp:%s fail", n,
                   tags->name, tags[n - 1].name, tmpl, group);
    }
}

void manager_storage_update_template_tag(const char *tmpl, const char *group,
                                         const neu_datatag_t *tag)
{
    int rv = neu_persister_update_template_tags(tmpl, group, tag, 1);
    if (0 != rv) {
        nlog_error("update tag:%s template:%s grp:%s fail", tag->name, tmpl,
                   group);
    }
}

void manager_storage_update_template_tags(const char *tmpl, const char *group,
                                          const neu_datatag_t *tags, size_t n)
{
    if (0 == n) {
        return;
    }

    if (1 == n) {
        return manager_storage_update_template_tag(tmpl, group, &tags[0]);
    }

    int rv = neu_persister_update_template_tags(tmpl, group, tags, n);
    if (0 != rv) {
        nlog_error("update %zu tags:[%s ... %s] template:%s grp:%s fail", n,
                   tags->name, tags[n - 1].name, tmpl, group);
    }
}

void manager_storage_del_template_tag(const char *tmpl, const char *group,
                                      const char *tag)
{
    int rv = neu_persister_delete_template_tags(tmpl, group, &tag, 1);
    if (0 != rv) {
        nlog_error("delete tag:%s template:%s grp:%s fail", tag, tmpl, group);
    }
}

void manager_storage_del_template_tags(const char *tmpl, const char *group,
                                       const char *const *tags, size_t n)
{
    if (0 == n) {
        return;
    }

    if (1 == n) {
        return manager_storage_del_template_tag(tmpl, group, tags[0]);
    }

    int rv = neu_persister_delete_template_tags(tmpl, group, tags, n);
    if (0 != rv) {
        nlog_error("del %zu tags:[%s ... %s] template:%s grp:%s fail", n,
                   tags[0], tags[n - 1], tmpl, group);
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
                                  node_info->plugin_name, node_info->state,
                                  true);
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
    UT_array *nodes = neu_node_manager_get(
        manager->node_manager, NEU_NA_TYPE_APP | NEU_NA_TYPE_NDRIVER);

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
                rv                = neu_manager_subscribe(manager, node->node,
                                           info->driver_name, info->group_name,
                                           info->params, &app_port);
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

static int load_template_group_and_tags(neu_manager_t *manager,
                                        const char *   tmpl_name)
{
    int       rv          = 0;
    UT_array *group_infos = NULL;

    rv = neu_persister_load_template_groups(tmpl_name, &group_infos);
    if (0 != rv) {
        nlog_warn("load template %s group fail", tmpl_name);
        return rv;
    }

    utarray_foreach(group_infos, neu_persist_group_info_t *, p)
    {
        neu_manager_add_template_group(manager, tmpl_name, p->name,
                                       p->interval);

        UT_array *tags = NULL;
        rv = neu_persister_load_template_tags(tmpl_name, p->name, &tags);
        if (0 != rv) {
            nlog_warn("load template:%s group:%s tags fail", tmpl_name,
                      p->name);
            continue;
        }

        neu_manager_add_template_tags(manager, tmpl_name, p->name,
                                      utarray_len(tags), utarray_front(tags),
                                      NULL);

        utarray_free(tags);
    }

    utarray_free(group_infos);
    return rv;
}

int manager_load_template(neu_manager_t *manager)
{
    int       rv         = 0;
    UT_array *tmpl_infos = NULL;

    rv = neu_persister_load_templates(&tmpl_infos);
    if (0 != rv) {
        nlog_error("failed to load template infos");
        return -1;
    }

    utarray_foreach(tmpl_infos, neu_persist_template_info_t *, tmpl_info)
    {
        rv = neu_manager_add_template(manager, tmpl_info->name,
                                      tmpl_info->plugin_name, 0, NULL);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        nlog_info("load template name:%s plugin:%s %s", tmpl_info->name,
                  tmpl_info->plugin_name, ok_or_err);
        if (0 != rv) {
            continue; // ignore error, load as much data as possible
        }

        load_template_group_and_tags(manager, tmpl_info->name);
    }

    utarray_free(tmpl_infos);
    return rv;
}
