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

#ifndef NEURON_ADAPTER_H
#define NEURON_ADAPTER_H

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "neu_tag_group_config.h"
#include "neu_types.h"

typedef struct neu_adapter neu_adapter_t;

/**
 * definition enum and structure for neuron request and response
 */

typedef enum neu_reqresp_type {
    NEU_REQRESP_NOP,
    NEU_REQRESP_READ,
    NEU_REQRESP_WRITE,
    NEU_REQRESP_TRANS_DATA,
} neu_reqresp_type_e;

/* NEU_REQRESP_TRANS_DATA */
typedef struct neu_reqresp_data {
    neu_taggrp_config_t *grp_config;
    neu_variable_t *     data_var;
} neu_reqresp_data_t;

typedef struct neu_request {
    uint32_t           req_id;
    neu_reqresp_type_e req_type;
    uint32_t           buf_len;
    void *             buf;
} neu_request_t;

typedef struct neu_response {
    uint32_t           req_id;
    neu_reqresp_type_e resp_type;
    uint32_t           buf_len;
    void *             buf;
} neu_response_t;

/**
 * definition enum and structure for neuron event
 */
typedef enum neu_event_type {
    NEU_EVENT_NOP,
    NEU_EVENT_STATUS,
    NEU_EVENT_READ,
} neu_event_type_e;

/* NEU_EVENT_READ */
typedef struct neu_event_read {
    neu_taggrp_config_t *grp_config;
    // TODO: use neu_variable_t to hold address information
    uint32_t addr;
} neu_event_read_t;

typedef struct neu_event_notify {
    uint32_t         event_id;
    neu_event_type_e type;
    uint32_t         buf_len;
    void *           buf;
} neu_event_notify_t;

typedef struct neu_event_reply {
    uint32_t         event_id;
    neu_event_type_e type;
    uint32_t         buf_len;
    void *           buf;
} neu_event_reply_t;

/**
 * definition enum and structure for neuron config
 */
typedef enum neu_config_type {
    NEU_CONFIG_UNKNOW,
    NEU_CONFIG_ADDRESS,
} neu_config_type_e;

typedef struct neu_config {
    neu_config_type_e type;
    uint32_t          buf_len;
    void *            buf;
} neu_config_t;

typedef struct adapter_callbacks {
    int (*response)(neu_adapter_t *adapter, neu_response_t *resp);
    int (*event_notify)(neu_adapter_t *adapter, neu_event_notify_t *event);
} adapter_callbacks_t;

#endif
