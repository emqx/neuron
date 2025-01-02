/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2025 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef NEURON_PLUGIN_MQTT_SCHEMA_H
#define NEURON_PLUGIN_MQTT_SCHEMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "json/json.h"

#include "plugin.h"

typedef enum {
    MQTT_SCHEMA_TIMESTAMP         = 0,
    MQTT_SCHEMA_NODE_NAME         = 1,
    MQTT_SCHEMA_GROUP_NAME        = 2,
    MQTT_SCHEMA_TAG_VALUES        = 3,
    MQTT_SCHEMA_STATIC_TAG_VALUES = 4,
    MQTT_SCHEMA_TAG_ERRORS        = 5,
    MQTT_SCHEMA_UD                = 6,
} mqtt_schema_vt_e;

typedef struct {
    char name[128];

    mqtt_schema_vt_e vt;
    char             ud[128];
} mqtt_schema_vt_t;

int mqtt_schema_validate(const char *schema, mqtt_schema_vt_t **vts,
                         size_t *vts_len);

int mqtt_schema_encode(char *driver, char *group, neu_json_read_resp_t *tags,
                       mqtt_schema_vt_t *vts, size_t n_vts, char **result_str);

typedef struct {
    char name[128];

    neu_json_type_e  jtype;
    neu_json_value_u jvalue;
} mqtt_static_vt_t;

int  mqtt_static_validate(const char *static_tags, mqtt_static_vt_t *vts,
                          size_t *vts_len);
void mqtt_static_free(mqtt_static_vt_t *vts, size_t vts_len);

#ifdef __cplusplus
}
#endif

#endif
