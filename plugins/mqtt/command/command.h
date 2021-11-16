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

#ifndef NEURON_PLUGIN_MQTT_COMMAND
#define NEURON_PLUGIN_MQTT_COMMAND

#ifdef __cplusplus
extern "C" {
#endif

#include <neuron.h>

#include "datatag.h"
#include "group_config.h"
#include "node.h"
#include "read_write.h"

typedef void (*mqtt_send_callback)(neu_plugin_t *plugin, char *json_str);
typedef void (*context_add_callback)(neu_plugin_t *plugin, uint32_t req_id,
                                     neu_json_mqtt_t *parse_head);

typedef struct {
    const char *         topic_name;
    size_t               topic_len;
    void *               payload;
    size_t               len;
    void *               context;
    mqtt_send_callback   mqtt_send;
    context_add_callback context_add;
} mqtt_response_t;

void command_response_handle(mqtt_response_t *response);

#ifdef __cplusplus
}
#endif
#endif