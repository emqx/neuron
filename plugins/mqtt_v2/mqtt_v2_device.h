/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef NEURON_PLUGIN_MQTT_V2_DEVICE_H
#define NEURON_PLUGIN_MQTT_V2_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mqtt_v2_plugin.h"

// topics stay NULL until the topic prefix is configured
mqtt_v2_device_t *mqtt_v2_device_new(const char *name, const char *app,
                                     const char *prefix);
void              mqtt_v2_device_free(mqtt_v2_device_t *device);
int mqtt_v2_device_set_topics(mqtt_v2_device_t *device, const char *app,
                              const char *prefix);

char *mqtt_v2_device_state_payload(const mqtt_v2_device_t *device,
                                   int64_t                 timestamp);

mqtt_v2_group_t *mqtt_v2_device_find_group(mqtt_v2_device_t *device,
                                           const char *      name);
mqtt_v2_group_t *mqtt_v2_device_add_group(mqtt_v2_device_t *device,
                                          const char *      name);
void mqtt_v2_device_del_group(mqtt_v2_device_t *device, mqtt_v2_group_t *group);
int  mqtt_v2_device_rename_group(mqtt_v2_device_t *device,
                                 mqtt_v2_group_t *group, const char *new_name,
                                 const char *app);

// takes ownership of `topic_tmpl`
int mqtt_v2_group_set_topic(mqtt_v2_group_t *group, char *topic_tmpl,
                            const char *app, const char *device);
// `tags` is an array of neu_datatag_t
void           mqtt_v2_group_set_tags(mqtt_v2_group_t *group, UT_array *tags);
mqtt_v2_tag_t *mqtt_v2_group_find_tag(mqtt_v2_group_t *group, const char *name);

#ifdef __cplusplus
}
#endif

#endif
