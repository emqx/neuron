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

#ifndef NEURON_PLUGIN_MQTT_CONFIG_H
#define NEURON_PLUGIN_MQTT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "connection/mqtt_client.h"
#include "plugin.h"

typedef enum {
    MQTT_UPLOAD_FORMAT_VALUES = 0,
    MQTT_UPLOAD_FORMAT_TAGS   = 1,
} mqtt_upload_format_e;

static inline const char *mqtt_upload_format_str(mqtt_upload_format_e f)
{
    switch (f) {
    case MQTT_UPLOAD_FORMAT_VALUES:
        return "format-values";
    case MQTT_UPLOAD_FORMAT_TAGS:
        return "format-tags";
    default:
        return NULL;
    }
}

typedef struct {
    neu_mqtt_version_e   version;             // mqtt version
    char *               client_id;           // client id
    neu_mqtt_qos_e       qos;                 // message QoS
    mqtt_upload_format_e format;              // upload format
    char *               write_req_topic;     // write request topic
    char *               write_resp_topic;    // write response topic
    size_t               cache;               // cache enable flag
    size_t               cache_mem_size;      // cache memory size in bytes
    size_t               cache_disk_size;     // cache disk size in bytes
    size_t               cache_sync_interval; // cache sync interval
    char *               host;                // broker host
    uint16_t             port;                // broker port
    char *               username;            // user name
    char *               password;            // user password
    bool                 ssl;                 // ssl flag
    char *               ca;                  // CA
    char *               cert;                // client cert
    char *               key;                 // client key
    char *               keypass;             // client key password
                                              // remove in 2.6, keep it here
                                              // for backward compatibility
} mqtt_config_t;

int decode_b64_param(neu_plugin_t *plugin, neu_json_elem_t *el);
int parse_b64_param(neu_plugin_t *plugin, const char *setting,
                    neu_json_elem_t *el);

int  mqtt_config_parse(neu_plugin_t *plugin, const char *setting,
                       mqtt_config_t *config);
void mqtt_config_fini(mqtt_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
