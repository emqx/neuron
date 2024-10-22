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

#include "utils/cid.h"
#include "utils/log.h"

#include "adapter_internal.h"
#include "driver/driver_internal.h"
#include "storage.h"

void adapter_storage_state(const char *node, neu_node_running_state_e state)
{
    neu_persister_update_node_state(node, state);
}

void adapter_storage_setting(const char *node, const char *setting)
{
    neu_persister_store_node_setting(node, setting);
}

void adapter_storage_add_group(const char *node, const char *group,
                               uint32_t interval, void *context)
{
    neu_persist_group_info_t info = {
        .name     = (char *) group,
        .interval = interval,
    };
    if (context != NULL) {
        char *ctx = neu_cid_info_to_string((cid_dataset_info_t *) context);
        neu_persister_store_group(node, &info, ctx);
        free(ctx);
    } else {
        neu_persister_store_group(node, &info, NULL);
    }
}

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

void adapter_storage_del_group(const char *node, const char *group)
{
    neu_persister_delete_group(node, group);
}

void adapter_storage_add_tag(const char *node, const char *group,
                             const neu_datatag_t *tag)
{
    int rv = neu_persister_store_tag(node, group, tag);
    if (0 != rv) {
        nlog_error("fail store tag:%s adapter:%s grp:%s", tag->name, node,
                   group);
    }
}

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

void adapter_storage_update_tag(const char *node, const char *group,
                                const neu_datatag_t *tag)
{
    int rv = neu_persister_update_tag(node, group, tag);
    if (0 != rv) {
        nlog_error("fail update tag:%s adapter:%s grp:%s", tag->name, node,
                   group);
    }
}

void adapter_storage_update_tag_value(const char *node, const char *group,
                                      const neu_datatag_t *tag)
{
    int rv = neu_persister_update_tag_value(node, group, tag);
    if (0 != rv) {
        nlog_error("fail update value tag:%s adapter:%s grp:%s", tag->name,
                   node, group);
    }
}

void adapter_storage_del_tag(const char *node, const char *group,
                             const char *name)
{
    int rv = neu_persister_delete_tag(node, group, name);
    if (0 != rv) {
        nlog_error("fail del tag:%s adapter:%s grp:%s", name, node, group);
    }
}

int adapter_load_setting(const char *node, char **setting)
{
    int rv = neu_persister_load_node_setting(node, (const char **) setting);
    if (0 != rv) {
        nlog_warn("load %s setting fail", node);
        return -1;
    }

    return 0;
}

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
        if (p->context == NULL) {
            neu_adapter_driver_add_group(driver, p->name, p->interval, NULL);
        } else {
            cid_dataset_info_t *info = neu_cid_info_from_string(p->context);
            neu_adapter_driver_add_group(driver, p->name, p->interval, info);
        }

        rv = neu_persister_load_tags(adapter->name, p->name, &tags);
        if (0 != rv) {
            nlog_warn("load %s:%s tags fail", adapter->name, p->name);
            continue;
        }

        utarray_foreach(tags, neu_datatag_t *, tag)
        {
            neu_adapter_driver_add_tag(driver, p->name, tag, -1);
            neu_adapter_driver_load_tag(driver, p->name, tag, 1);
        }

        utarray_free(tags);
    }

    utarray_free(group_infos);
    return rv;
}