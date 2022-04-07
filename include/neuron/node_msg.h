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

#ifndef _NEU_NODE_MSG_H_
#define _NEU_NODE_MSG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "node.h"

typedef enum neu_node_msg_type {
    /* NODE */
    NEU_NODE_MSG_START = 1,
    NEU_NODE_CONFIG    = NEU_NODE_MSG_START,
    NEU_NODE_GET_CONFIG,
    NEU_NODE_CTL,

    /* DRIVER */
    NEU_NODE_DRIVER_MSG_START = 101,
    NEU_NODE_DRIVER_ADD_GROUP = NEU_NODE_DRIVER_MSG_START,
    NEU_NODE_DRIVER_DEL_GROUP,
    NEU_NODE_DRIVER_GET_GROUP,
    NEU_NODE_DRIVER_ADD_TAG,
    NEU_NODE_DRIVER_DEL_TAG,
    NEU_NODE_DRIVER_UPDATE_TAG,
    NEU_NODE_DRIVER_READ_TAG,
    NEU_NODE_DRIVER_WRITE_TAG,

    /* CONFIG APP */
    NEU_NODE_CONFIG_APP_MSG_START = 201,
    NEU_NODE_CONFIG_APP_ADD_NODE  = NEU_NODE_CONFIG_APP_MSG_START,
    NEU_NODE_CONFIG_APP_DEL_NODE,
    NEU_NODE_CONFIG_APP_GET_NODE,
    NEU_NODE_CONFIG_APP_ADD_PLUGIN,
    NEU_NODE_CONFIG_APP_DEL_PLUGIN,
    NEU_NODE_CONFIG_APP_GET_PLUGIN,

    /* DATA APP */
    NEU_NODE_DATA_APP_MSG_START = 301,
    NEU_NODE_DATA_APP_SUBSCRIBE = NEU_NODE_DATA_APP_MSG_START,
    NEU_NODE_DATA_APP_UNSUBSCRIBE,

} neu_node_msg_type_e;

typedef struct neu_msg_request {
    uint32_t            req_id;
    neu_node_msg_type_e msg_type;

    void *ctx;

    char *sender_name;
    char *recver_name;

    uint32_t buf_len;
    void *   buf;
} neu_msg_request_t, neu_msg_response_t;

#ifdef __cplusplus
}
#endif

#endif
