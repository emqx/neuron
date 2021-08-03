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

#ifndef ADAPTER_INTERNAL_H
#define ADAPTER_INTERNAL_H

#include "core/databuf.h"
#include "core/neu_manager.h"
#include "core/plugin_info.h"

typedef enum adapter_type {
    ADAPTER_TYPE_UNKNOW,
    ADAPTER_TYPE_WEBSERVER,
    ADAPTER_TYPE_MQTT,
    ADAPTER_TYPE_STREAM_PROCESSOR,
    ADAPTER_TYPE_APP,
    ADAPTER_TYPE_DRIVER,
    ADAPTER_TYPE_MAX,
} adapter_type_e;

typedef uint32_t adapter_id_t;
typedef struct neu_adapter_info {
    adapter_id_t   id;
    adapter_type_e type;
    plugin_id_t    plugin_id;
    const char *   name;
    const char *   plugin_lib_name;
} neu_adapter_info_t;

neu_adapter_t *neu_adapter_create(neu_adapter_info_t *info);
void           neu_adapter_destroy(neu_adapter_t *adapter);
int         neu_adapter_start(neu_adapter_t *adapter, neu_manager_t *manager);
int         neu_adapter_stop(neu_adapter_t *adapter, neu_manager_t *manager);
const char *neu_adapter_get_name(neu_adapter_t *adapter);
neu_manager_t *neu_adapter_get_manager(neu_adapter_t *adapter);
nng_socket     neu_adapter_get_sock(neu_adapter_t *adapter);
adapter_id_t   neu_adapter_get_id(neu_adapter_t *adapter);

#endif
