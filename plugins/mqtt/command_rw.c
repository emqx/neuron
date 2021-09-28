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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "command_rw.h"

void command_read_request(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                          neu_parse_read_req_t *req)
{
    log_info("READ uuid:%s, node id:%u", mqtt->uuid, req->node_id);

    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = common_node_id_exist(&nodes, req->node_id);
    vector_uninit(&nodes);
    if (0 != rc) {
        return;
    }

    vector_t configs = neu_system_get_group_configs(plugin, req->node_id);
    neu_taggrp_config_t *config;
    neu_taggrp_config_t *c;
    config          = *(neu_taggrp_config_t **) vector_get(&configs, 0);
    c               = (neu_taggrp_config_t *) neu_taggrp_cfg_ref(config);
    uint32_t req_id = neu_plugin_send_read_cmd(plugin, req->node_id, c);

    GROUP_CONFIGS_UINIT(configs);
    UNUSED(req_id);
}

void command_read_response(neu_plugin_t *plugin, neu_taggrp_config_t *config,
                           neu_data_val_t *array)
{
    UNUSED(plugin);
    UNUSED(config);
    UNUSED(array);
}

void command_write_request(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                           neu_parse_write_req_t *write_req)
{
    UNUSED(plugin);
    UNUSED(mqtt);
    UNUSED(write_req);
}

void command_write_response(neu_plugin_t *plugin, neu_data_val_t *array)
{
    UNUSED(plugin);
    UNUSED(array);
}