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

#ifndef NEURON_PLUGIN_MQTT_UTIL
#define NEURON_PLUGIN_MQTT_UTIL

#ifdef __cplusplus
extern "C" {
#endif

#include "connection/mqtt_client_intf.h"
#include <neuron.h>

int mqtt_option_init(neu_plugin_t *plugin, char *config,
                     neu_mqtt_option_t *option);

const char *mqtt_option_error(int error);

int    mqtt_option_validate(neu_plugin_t *plugin, const char *config);
size_t mqtt_option_item_cache(neu_plugin_t *plugin, const char *config);

void mqtt_option_uninit(neu_plugin_t *plugin, neu_mqtt_option_t *option);

#ifdef __cplusplus
}
#endif
#endif