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

#ifndef NEURON_PLUGIN_MQTT_COMMAND_RW
#define NEURON_PLUGIN_MQTT_COMMAND_RW

#ifdef __cplusplus
extern "C" {
#endif

#include <neuron.h>

#include "common.h"

int   command_read_once_request(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                                neu_parse_read_req_t *req);
char *command_read_once_response(neu_plugin_t *    plugin,
                                 neu_parse_mqtt_t *parse_header,
                                 neu_data_val_t *  resp_val);
char *command_read_cycle_response(neu_plugin_t *  plugin,
                                  neu_data_val_t *resp_val);
int   command_write_request(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                            neu_parse_write_req_t *write_req);
char *command_write_response(neu_plugin_t *plugin, const char *uuid,
                             neu_data_val_t *resp_val);

#ifdef __cplusplus
}
#endif
#endif