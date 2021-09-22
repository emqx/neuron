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

#ifndef _NEU_JSON_MQTT_H
#define _NEU_JSON_MQTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum neu_mqtt_function {
    NEU_MQTT_OP_LOGIN = 1,
    NEU_MQTT_OP_LOGOUT,

    NEU_MQTT_OP_READ,
    NEU_MQTT_OP_WRITE,

    /*group config*/
    NEU_MQTT_OP_ADD_GROUP_CONFIG,
    NEU_MQTT_OP_UPDATE_GROUP_CONFIG,
    NEU_MQTT_OP_DELETE_GROUP_CONFIG,
    NEU_MQTT_OP_GET_GROUP_CONFIG,

    /*tag */
    NEU_MQTT_OP_ADD_TAGS,
    NEU_MQTT_OP_UPDATE_TAGS,
    NEU_MQTT_OP_DELETE_TAGS,
    NEU_MQTT_OP_GET_TAGS,

    /*nodes*/
    NEU_MQTT_OP_ADD_NODES,
    NEU_MQTT_OP_UPDATE_NODES,
    NEU_MQTT_OP_DELETE_NODES,
    NEU_MQTT_OP_GET_NODES,

    /*plugin*/
    NEU_MQTT_OP_ADD_PLUGIN,
    NEU_MQTT_OP_UPDATE_PLUGIN,
    NEU_MQTT_OP_DELETE_PLUGIN,
    NEU_MQTT_OP_GET_PLUGIN,

    NEU_MQTT_OP_ADD_DATATAG_IDS_CONFIG,
    NEU_MQTT_OP_DELETE_DATATAG_IDS_CONFIG,
} neu_mqtt_function_e;

typedef struct neu_parse_mqtt {
    neu_mqtt_function_e function;
    char *              uuid;
} neu_parse_mqtt_t;

int  neu_parse_decode_mqtt_param(char *buf, neu_parse_mqtt_t **req);
void neu_parse_decode_mqtt_param_free(neu_parse_mqtt_t *req);
int  neu_parse_encode_mqtt_param(void *json_object, void *param);

#ifdef __cplusplus
}
#endif

#endif