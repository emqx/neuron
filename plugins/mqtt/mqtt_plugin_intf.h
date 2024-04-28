/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef NEURON_PLUGIN_MQTT_INTF_H
#define NEURON_PLUGIN_MQTT_INTF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "neuron.h"

extern const neu_plugin_intf_funs_t mqtt_plugin_intf_funs;

neu_plugin_t *mqtt_plugin_open(void);
int           mqtt_plugin_close(neu_plugin_t *plugin);
int           mqtt_plugin_init(neu_plugin_t *plugin, bool load);
int           mqtt_plugin_uninit(neu_plugin_t *plugin);
int           mqtt_plugin_config(neu_plugin_t *plugin, const char *setting);
int           mqtt_plugin_start(neu_plugin_t *plugin);
int           mqtt_plugin_stop(neu_plugin_t *plugin);
int mqtt_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                        void *data);

#ifdef __cplusplus
}
#endif

#endif
