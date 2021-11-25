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

#ifndef NEURON_PLUGIN_MQTT_COMMAND_NODE
#define NEURON_PLUGIN_MQTT_COMMAND_NODE

#ifdef __cplusplus
extern "C" {
#endif

#include <neuron.h>

#include "common.h"

char *command_node_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                       neu_json_get_nodes_req_t *req);
char *command_node_add(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                       neu_json_add_node_req_t *req);
char *command_node_update(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                          neu_json_update_node_req_t *req);
char *command_node_delete(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                          neu_json_del_node_req_t *req);
char *command_node_setting_set(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                               neu_json_node_setting_req_t *req,
                               const char *                 json_str);
char *command_node_setting_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                               neu_json_node_setting_req_t *req);
char *command_node_state_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                             neu_json_node_setting_req_t *req);
char *command_node_control(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                           neu_json_node_ctl_req_t *req);

#ifdef __cplusplus
}
#endif
#endif