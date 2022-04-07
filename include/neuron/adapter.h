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

#ifndef _NEU_ADAPTER_H_
#define _NEU_ADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef uint32_t neu_node_id_t;

typedef enum neu_node_type {
    NEU_NODE_TYPE_UNKNOW = 0,
    NEU_NODE_TYPE_DRIVER,
    NEU_NODE_TYPE_WEBSERVER,
    NEU_NODE_TYPE_MQTT,
    NEU_NODE_TYPE_DRIVERX,
    NEU_NODE_TYPE_APP,
    NEU_NODE_TYPE_FUNCTIONAL,
    NEU_NODE_TYPE_MAX,
} neu_node_type_e;

#include "adapter_msg.h"

typedef struct neu_adapter        neu_adapter_t;
typedef struct neu_adapter_driver neu_adapter_driver_t;

typedef enum neu_config_type {
    NEU_CONFIG_UNKNOW,
    NEU_CONFIG_SETTING,
} neu_config_type_e;

typedef struct neu_config {
    neu_config_type_e type;
    uint32_t          buf_len;
    void *            buf;
} neu_config_t;

typedef struct adapter_callbacks {
    int (*command)(neu_adapter_t *adapter, neu_request_t *cmd,
                   neu_response_t **p_result);
    int (*response)(neu_adapter_t *adapter, neu_response_t *resp);
    int (*event_notify)(neu_adapter_t *adapter, neu_event_notify_t *event);
} adapter_callbacks_t;

#ifdef __cplusplus
}
#endif

#endif