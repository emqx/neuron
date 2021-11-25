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

#include "node.h"

char *command_node_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                       neu_json_get_nodes_req_t *req)
{
    log_info("Get node list uuid:%s, node type:%d", mqtt->uuid, req->node_type);
    neu_node_type_e       node_type = req->node_type;
    neu_json_error_resp_t error     = { 0 };
    if (NEU_NODE_TYPE_MAX <= req->node_type ||
        NEU_NODE_TYPE_UNKNOW >= req->node_type) {
        error.error = NEU_ERR_NODE_TYPE_INVALID;

        char *result = NULL;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    neu_json_get_nodes_resp_t res   = { 0 };
    int                       index = 0;
    vector_t                  nodes = neu_system_get_nodes(plugin, node_type);

    res.n_node = nodes.size;
    res.nodes  = malloc(res.n_node * sizeof(neu_json_get_nodes_resp_node_t));
    memset(res.nodes, 0, res.n_node * sizeof(neu_json_get_nodes_resp_node_t));

    VECTOR_FOR_EACH(&nodes, iter)
    {
        neu_node_info_t *info = (neu_node_info_t *) iterator_get(&iter);

        res.nodes[index].id   = info->node_id;
        res.nodes[index].name = info->node_name;
        index += 1;
    }

    vector_uninit(&nodes);

    char *result = NULL;
    neu_json_encode_with_mqtt(&res, neu_json_encode_get_nodes_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_node_add(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                       neu_json_add_node_req_t *req)
{
    log_info("Add node uuid:%s, node type:%d", mqtt->uuid, req->type);
    neu_json_error_resp_t error = { 0 };
    if (NEU_NODE_TYPE_MAX <= req->type || NEU_NODE_TYPE_UNKNOW >= req->type) {
        error.error = NEU_ERR_NODE_TYPE_INVALID;
    } else {
        error.error =
            neu_system_add_node(plugin, req->type, req->name, req->plugin_name);
    }

    char *result = NULL;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_node_update(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                          neu_json_update_node_req_t *req)
{
    log_info("Update node uuid:%s, node name:%s", mqtt->uuid, req->name);
    neu_json_error_resp_t error = { 0 };
    error.error = neu_system_update_node(plugin, req->id, req->name);

    char *result = NULL;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_node_delete(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                          neu_json_del_node_req_t *req)
{
    log_info("Delete node uuid:%s, node id:%d", mqtt->uuid, req->id);
    neu_json_error_resp_t error = { 0 };
    error.error                 = neu_system_del_node(plugin, req->id);
    char *result                = NULL;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_node_setting_set(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                               neu_json_node_setting_req_t *req,
                               const char *                 json_str)
{
    log_info("Set node setting uuid:%s, node id:%d", mqtt->uuid, req->node_id);
    neu_json_error_resp_t error  = { 0 };
    char *                result = NULL;
    error.error = neu_plugin_set_node_setting(plugin, req->node_id, json_str);
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_node_setting_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                               neu_json_node_setting_req_t *req)
{
    log_info("Get node setting uuid:%s, node id:%d", mqtt->uuid, req->node_id);
    neu_json_error_resp_t error  = { 0 };
    char *                result = NULL;
    error.error = neu_plugin_get_node_setting(plugin, req->node_id, &result);
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_node_state_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                             neu_json_node_setting_req_t *req)
{
    log_info("Get node state uuid:%s, node id:%d", mqtt->uuid, req->node_id);
    neu_plugin_state_t             state  = { 0 };
    neu_json_get_node_state_resp_t res    = { 0 };
    neu_json_error_resp_t          error  = { 0 };
    char *                         result = NULL;
    error.error = neu_plugin_get_node_state(plugin, req->node_id, &state);
    if (0 != error.error) {
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    res.running = state.running;
    res.link    = state.link;
    neu_json_encode_with_mqtt(&res, neu_json_encode_get_node_state_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_node_control(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                           neu_json_node_ctl_req_t *req)
{
    log_info("Node control uuid:%s, node id:%d", mqtt->uuid, req->id);
    char *                result = NULL;
    neu_json_error_resp_t error  = { 0 };
    error.error = neu_plugin_node_ctl(plugin, req->id, req->cmd);
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}
