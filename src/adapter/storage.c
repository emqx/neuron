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

void adapter_storage_setting(neu_persister_t *persister, const char *node,
                             const char *setting)
{
    neu_persister_store_node_setting(persister, node, setting);
}

void adapter_storage_add_group(neu_persister_t *persister, const char *node,
                               const char *group, uint32_t interval)
{
    neu_persist_group_config_info_t info = {
        .group_config_name = (char *) group,
        .read_interval     = interval,
        .adapter_name      = (char *) node,
    };

    neu_persister_store_group_config(persister, node, &info);
}

void adapter_storage_del_group(neu_persister_t *persister, const char *node,
                               const char *group)
{
    neu_persister_delete_group_config(persister, node, group);
}

void adapter_storage_tag(neu_persister_t *persister, neu_adapter_t *adapter,
                         const char *node, const char *group)
{
    UT_array *tags = NULL;

    int rv = neu_adapter_driver_get_tag((neu_adapter_driver_t *) adapter, group,
                                        &tags);
    if (rv != 0) {
        nlog_warn("fail get tag infos");
        return;
    }

    rv = neu_persister_store_datatags(persister, node, group, tags);
    if (0 != rv) {
        nlog_error("fail store adapter:%s grp:%s datatag infos", node, group);
    }

    neu_persist_datatag_infos_free(tags);
}

int adapter_load_setting(neu_persister_t *persister, const char *node,
                         char **setting)
{
    int rv = neu_persister_load_node_setting(persister, node,
                                             (const char **) setting);
    if (0 != rv) {
        nlog_info("load %s setting fail", node);
        return -1;
    }

    return 0;
}

int adapter_load_group_and_tag(neu_persister_t *     persister,
                               neu_adapter_driver_t *driver)
{
    UT_array *     group_infos = NULL;
    neu_adapter_t *adapter     = (neu_adapter_t *) driver;

    int rv = neu_persister_load_group_configs(persister, adapter->name,
                                              &group_infos);
    if (0 != rv) {
        nlog_warn("load %s group fail", adapter->name);
        return rv;
    }

    utarray_foreach(group_infos, neu_persist_group_config_info_t *, p)
    {
        UT_array *tags = NULL;
        neu_adapter_driver_add_group(driver, p->group_config_name,
                                     p->read_interval);

        rv = neu_persister_load_datatags(persister, adapter->name,
                                         p->group_config_name, &tags);
        if (0 != rv) {
            nlog_warn("load %s:%s tags fail", adapter->name,
                      p->group_config_name);
            continue;
        }

        utarray_foreach(tags, neu_datatag_t *, tag)
        {
            neu_adapter_driver_add_tag(driver, p->group_config_name, tag);
        }

        neu_persist_datatag_infos_free(tags);
    }

    neu_persist_group_config_infos_free(group_infos);
    return rv;
}