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

#ifndef NEURON_PLUGIN_MQTT_V2_CONFIG_H
#define NEURON_PLUGIN_MQTT_V2_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "connection/mqtt_client.h"
#include "plugin.h"

#include "mqtt_v2_template.h"

#define MQTT_V2_DEFAULT_TOPIC_PREFIX "neuron/${app}"

#define MQTT_V2_DEFAULT_DATA_TEMPLATE                               \
    "{\"timestamp\": \"${timestamp}\", \"device\": \"${device}\", " \
    "\"group\": \"${group}\", \"tags\": \"${tags}\"}"

#define MQTT_V2_DEFAULT_TAG_TEMPLATE                                         \
    "{\"name\": \"${tag.name}\", \"value\": \"${tag.value}\", "              \
    "\"timestamp\": \"${tag.timestamp}\", \"quality\": \"${tag.quality}\", " \
    "\"description\": \"${tag.description}\", \"unit\": \"${tag.unit}\", "   \
    "\"error\": \"${tag.error}\"}"

#define MQTT_V2_TEMPLATE_MAX_LEN 16384

typedef struct {
    neu_mqtt_version_e version;
    char *             client_id;
    neu_mqtt_qos_e     qos;
    char *             topic_prefix; // may contain `${app}`
    mqtt_v2_tmpl_t *   data_tmpl;
    mqtt_v2_tmpl_t *   tag_tmpl;
    bool               upload_err;
    bool               cache;
    size_t             cache_mem_size;  // in bytes
    size_t             cache_disk_size; // in bytes
    size_t             cache_sync_interval;
    char *             host;
    uint16_t           port;
    char *             username;
    char *             password;
    bool               ssl;
    char *             ca;
    char *             cert;
    char *             key;
} mqtt_v2_config_t;

int  mqtt_v2_config_parse(neu_plugin_t *plugin, const char *setting,
                          mqtt_v2_config_t *config);
void mqtt_v2_config_fini(mqtt_v2_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
