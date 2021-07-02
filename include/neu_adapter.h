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

#include <string.h>
#include <unistd.h>

typedef struct neu_adapter neu_adapter_t;

/**
 * definition enum and structure for neuron request and response
 */

typedef enum neu_reqresp_type {
	NEU_REQRESP_NOP,
	NEU_REQRESP_READ,
	NEU_REQRESP_WRITE,
} neu_reqresp_type_e;

typedef struct neu_request {
   uint32_t       req_id;
   neu_reqresp_type_e  req_type;
   uint32_t       buf_len;
   char*          buf;
} neu_request_t;

typedef struct neu_response {
    uint32_t   req_id;
    neu_reqresp_type_e  resp_type;
    uint32_t   buf_len;
    char*      buf;
} neu_response_t;

/**
 * definition enum and structure for neuron event
 */
typedef enum neu_event_type {
	NEU_EVENT_NOP,
	NEU_EVENT_STATUS,
} neu_event_type_e;

typedef struct neu_event_notify {
	uint32_t              event_id;
	neu_event_type_e      type;
    uint32_t              buf_len;
    char*                 buf;
} neu_event_notify_t;

typedef struct neu_event_reply {
    uint32_t              event_id;
	neu_event_type_e      type;
    uint32_t              buf_len;
    char*                 buf;
} neu_event_reply_t;

/**
 * definition enum and structure for neuron config
 */
typedef enum neu_config_type {
	NEU_CONFIG_UNKNOW,
	NEU_CONFIG_ADDRESS,
} neu_config_type_e;

typedef struct neu_config {
	neu_config_type_e     type;
    uint32_t              buf_len;
    char*                 buf;
}  neu_config_t;

typedef struct adapter_callbacks {
	int (*response)(neu_adapter_t* adapter,
					neu_response_t* resp);
	int (*event_notify)(neu_adapter_t* adapter,
						neu_event_notify_t* event);
} adapter_callbacks_t;

#endif
