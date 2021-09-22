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

#ifndef NEURON_PLUGIN_OPEN62541_HANDLE
#define NEURON_PLUGIN_OPEN62541_HANDLE

#ifdef __cplusplus
extern "C" {
#endif

#include <neuron.h>
#include <nng/nng.h>

typedef void (*periodic_response_callback)(neu_plugin_t *       plugin,
                                           neu_taggrp_config_t *config,
                                           neu_variable_t *     array);

typedef void (*subscribe_response_callback)(neu_plugin_t *       plugin,
                                            neu_taggrp_config_t *config,
                                            neu_variable_t *     array);

typedef struct {
    neu_plugin_t *       plugin;
    open62541_client_t * client;
    neu_datatag_table_t *table;
    neu_node_id_t        self_node_id;
    neu_list             subscribe_list;
} opc_handle_context_t;

typedef struct {
    neu_list_node               node;
    neu_plugin_t *              plugin;
    neu_node_id_t               node_id;
    neu_taggrp_config_t *       config;
    char *                      name;
    nng_aio *                   aio;
    periodic_response_callback  periodic_cb;
    subscribe_response_callback subscribe_cb;
    opc_handle_context_t *      context;
} opc_subscribe_tuple_t;

int plugin_handle_read_once(opc_handle_context_t *context,
                            neu_taggrp_config_t *config, neu_variable_t *array);
int plugin_handle_write_value(opc_handle_context_t *context,
                              neu_variable_t *array, vector_t *data);
int plugin_handle_subscribe(opc_handle_context_t *      context,
                            neu_taggrp_config_t *       config,
                            periodic_response_callback  periodic_cb,
                            subscribe_response_callback subscribe_cb);
int plugin_handle_unsubscribe(opc_handle_context_t *context,
                              neu_taggrp_config_t * config);

#ifdef __cplusplus
}
#endif
#endif
