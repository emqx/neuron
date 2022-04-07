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

#ifndef _NEU_NODE_H_
#define _NEU_NODE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"

typedef enum {
    NEU_NODE_DRIVER     = 10,
    NEU_NODE_CONFIG_APP = 11,
    NEU_NODE_DATA_APP   = 12,
} neu_nodep_type_e;

typedef struct neu_node            neu_node_t;
typedef struct neu_node_driver     neu_node_driver_t;
typedef struct neu_node_config_app neu_node_config_app_t;
typedef struct neu_node_data_app   neu_node_data_app_t;

typedef enum {
    NEU_NODE_LINK_STATUS_DISCONNECTED = 0,
    NEU_NODE_LINK_STATUS_CONNECTING   = 1,
    NEU_NODE_LINK_STATUS_CONNECTED    = 2,
} neu_node_link_status_e;

typedef enum {
    NEU_NODE_RUNNING_STATUS_IDLE    = 0,
    NEU_NODE_RUNNING_STATUS_INIT    = 1,
    NEU_NODE_RUNNING_STATUS_READY   = 2,
    NEU_NODE_RUNNING_STATUS_RUNNING = 3,
    NEU_NODE_RUNNING_STATUS_STOPPED = 4,
} neu_node_running_status_e;

typedef struct {
    neu_node_running_status_e running;
    neu_node_link_status_e    link;
} neu_node_status_t;

typedef struct neu_node_callbacks {
    void (*set_link_status)(neu_node_t *           node,
                            neu_node_link_status_e link_status);
    int64_t (*send)(neu_node_t *node, uint16_t n_byte, uint8_t *bytes,
                    int flag);
    int64_t (*syn_recv)(neu_node_t *node, uint16_t max_byte, uint8_t *bytes,
                        int flag);
    union {
        struct {
            int (*update)(neu_node_driver_t *node, const char *tag_name,
                          neu_value_u value);
        } driver;

        struct {
            int (*config)(neu_node_config_app_t *node);
            int (*read)(neu_node_config_app_t *node, const char *node_name,
                        const char *tag_name, neu_value_u *value);
            int (*write)(neu_node_config_app_t *node, const char *node_name,
                         const char *tag_name, neu_value_u value);
        } config_app;

        struct {
            int (*read)(neu_node_data_app_t *node, const char *node_name,
                        const char *tag_name, neu_value_u *value);
            int (*write)(neu_node_data_app_t *node, const char *node_name,
                         const char *tag_name, neu_value_u value);
        } data_app;
    };
} neu_node_callbacks_t;

#ifdef __cplusplus
}
#endif

#endif
