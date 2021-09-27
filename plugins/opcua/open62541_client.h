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

#ifndef NEURON_PLUGIN_OPEN62541_CLIENT
#define NEURON_PLUGIN_OPEN62541_CLIENT

#ifdef __cplusplus
extern "C" {
#endif

#include <neuron.h>
#include <stdbool.h>
#include <stdint.h>

#include "option.h"

typedef struct open62541_client open62541_client_t;
typedef struct subscribe_tuple  subscribe_tuple_t;

union opcua_value {
    int8_t   value_int8;
    uint8_t  value_uint8;
    bool     value_boolean;
    int16_t  value_int16;
    uint16_t value_uint16;
    int32_t  value_int32;
    uint32_t value_uint32;
    int64_t  value_int64;
    uint64_t value_uint64;
    float    value_float;
    double   value_double;
    uint32_t value_datetime;
    char *   value_string;
};

typedef struct {
    char *   name;
    uint32_t id;
    int      identifier_type; /* 0-str, 1-uint32*/
    uint16_t namespace_index;
    char *   identifier_str;
    uint32_t identifier_int;
    int      error;
} opcua_node_t;

typedef struct {
    opcua_node_t      opcua_node;
    int               error;
    neu_dtype_e       type;
    union opcua_value value;
} opcua_data_t;

open62541_client_t *open62541_client_create(option_t *option, void *context);

int open62541_client_connect(open62541_client_t *client);
int open62541_client_open(option_t *option, void *context,
                          open62541_client_t **p_client);
int open62541_client_is_connected(open62541_client_t *client);

int open62541_client_read(open62541_client_t *client, vector_t *data);
int open62541_client_write(open62541_client_t *client, vector_t *data);

int open62541_client_subscribe(open62541_client_t *client, vector_t *node);
int open62541_client_unsubscribe(open62541_client_t *client, vector_t *node);

int open62541_client_disconnect(open62541_client_t *client);
int open62541_client_destroy(open62541_client_t *client);
int open62541_client_close(open62541_client_t *client);

#ifdef __cplusplus
}
#endif
#endif