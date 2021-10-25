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

#include <stdlib.h>

#include "command_node.h"

char *command_get_nodes(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                        neu_parse_get_nodes_req_t *req)
{
    log_info("Get node list uuid:%s, node type:%d", mqtt->uuid, req->node_type);

    neu_node_type_e           node_type = req->node_type;
    neu_parse_get_nodes_res_t res       = { 0 };
    int                       index     = 0;
    vector_t                  nodes = neu_system_get_nodes(plugin, node_type);

    res.n_node = nodes.size;
    res.nodes  = malloc(res.n_node * sizeof(neu_parse_get_nodes_res_node_t));
    memset(res.nodes, 0, res.n_node * sizeof(neu_parse_get_nodes_res_node_t));

    VECTOR_FOR_EACH(&nodes, iter)
    {
        neu_node_info_t *info = (neu_node_info_t *) iterator_get(&iter);

        res.nodes[index].id   = info->node_id;
        res.nodes[index].name = info->node_name;
        index += 1;
    }

    char *json_str = NULL;
    int   rc = neu_json_encode_with_mqtt(&res, neu_parse_encode_get_tags, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
    if (0 == rc) {
        return json_str;
    }

    return NULL;
}

char *command_add_node(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                       neu_parse_add_node_req_t *req)
{
    log_info("Add node uuid:%s, node type:%d", mqtt->uuid, req->node_type);
    intptr_t rc = neu_system_add_node(plugin, req->node_type, req->node_name,
                                      req->plugin_name);
    char *   json_str = NULL;
    if (rc != 0) {
        json_str = strdup("{\"error\": 1}");
    } else {
        json_str = strdup("{\"error\": 0}");
    }

    return json_str;
}

char *command_update_node(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                          neu_parse_update_node_req_t *req)
{
    log_info("Update node uuid:%s, node type:%d", mqtt->uuid, req->node_type);
    intptr_t rc = neu_system_update_node(plugin, req->node_type, req->node_name,
                                         req->plugin_name);
    char *   json_str = NULL;
    if (rc != 0) {
        json_str = strdup("{\"error\": 1}");
    } else {
        json_str = strdup("{\"error\": 0}");
    }

    return json_str;
}

char *command_delete_node(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                          neu_parse_del_node_req_t *req)
{
    log_info("Delete node uuid:%s, node id:%d", mqtt->uuid, req->node_id);
    intptr_t rc       = neu_system_del_node(plugin, req->node_id);
    char *   json_str = NULL;
    if (rc != 0) {
        json_str = strdup("{\"error\": 1}");
    } else {
        json_str = strdup("{\"error\": 0}");
    }

    return json_str;
}