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

typedef struct {
    neu_plugin_t *       plugin;
    open62541_client_t * client;
    neu_datatag_table_t *table;
    neu_node_id_t        self_node_id;
} opc_handle_context_t;

int plugin_handle_read_once(opc_handle_context_t *context,
                            neu_taggrp_config_t *config, neu_variable_t *array);
int plugin_handle_write_value(opc_handle_context_t *context,
                              neu_variable_t *array, vector_t *data);

#ifdef __cplusplus
}
#endif
#endif
