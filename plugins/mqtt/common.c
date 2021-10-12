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

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int common_node_id_exist(vector_t *v, const uint32_t node_id)
{
    neu_node_info_t *node_info;
    VECTOR_FOR_EACH(v, iter)
    {
        node_info = (neu_node_info_t *) iterator_get(&iter);
        log_info("node info id:%ld, name:%s", node_info->node_id,
                 node_info->node_name);
        if (node_id == node_info->node_id) {
            return 0;
        }
    }
    return -1;
}

int common_config_exist(vector_t *v, const char *config_name)
{
    neu_taggrp_config_t *config;

    VECTOR_FOR_EACH(v, iter)
    {
        config = *(neu_taggrp_config_t **) iterator_get(&iter);
        if (NULL == config) {
            continue;
        }

        if (0 == strcmp(config_name, neu_taggrp_cfg_get_name(config))) {
            return 0;
        }
    }
    return -1;
}

int common_has_node(neu_plugin_t *plugin, const uint32_t node_id)
{
    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = common_node_id_exist(&nodes, node_id);
    vector_uninit(&nodes);
    return rc;
}

int common_has_group_config(neu_plugin_t *plugin, const uint32_t node_id,
                            const char *group_config_name)
{
    vector_t configs = neu_system_get_group_configs(plugin, node_id);
    int      rc      = common_config_exist(&configs, group_config_name);
    GROUP_CONFIGS_UNINIT(configs);
    return rc;
}

neu_taggrp_config_t *common_get_group_config(neu_plugin_t * plugin,
                                             const uint32_t node_id,
                                             const char *   group_config_name)
{
    vector_t configs            = neu_system_get_group_configs(plugin, node_id);
    neu_taggrp_config_t *config = NULL;
    neu_taggrp_config_t *ret_config = NULL;
    VECTOR_FOR_EACH(&configs, iter)
    {
        config = *(neu_taggrp_config_t **) iterator_get(&iter);
        if (NULL == config) {
            continue;
        }

        if (0 == strcmp(group_config_name, neu_taggrp_cfg_get_name(config))) {
            ret_config = config;
        }
    }
    GROUP_CONFIGS_UNINIT(configs);

    return ret_config;
}

void common_group_configs_freach(neu_plugin_t *plugin, const uint32_t node_id,
                                 void *context,
                                 void visit_func(neu_taggrp_config_t *, void *))
{
    vector_t configs = neu_system_get_group_configs(plugin, node_id);
    neu_taggrp_config_t *config;
    VECTOR_FOR_EACH(&configs, iter)
    {
        config = *(neu_taggrp_config_t **) iterator_get(&iter);
        visit_func(config, context);
    }
    GROUP_CONFIGS_UNINIT(configs);
}

neu_datatag_table_t *common_get_datatags_table(neu_plugin_t * plugin,
                                               const uint32_t node_id)
{
    neu_datatag_table_t *table = neu_system_get_datatags_table(plugin, node_id);
    return table;
}