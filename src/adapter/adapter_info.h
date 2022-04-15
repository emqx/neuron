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

#ifndef ADAPTER_INFO_H
#define ADAPTER_INFO_H

#include <stdint.h>

#include "adapter.h"
#include "plugin_info.h"

typedef enum adapter_type {
    ADAPTER_TYPE_UNKNOW = 0,
    ADAPTER_TYPE_DRIVER,
    ADAPTER_TYPE_WEBSERVER,
    ADAPTER_TYPE_MQTT,
    ADAPTER_TYPE_DRIVERX,
    ADAPTER_TYPE_APP,
    ADAPTER_TYPE_FUNCTIONAL,

    ADAPTER_TYPE_MAX,
} adapter_type_e;

typedef struct neu_adapter_info {
    neu_adapter_id_t id;
    adapter_type_e   type;
    plugin_id_t      plugin_id;
    const char *     name;
    plugin_kind_e    plugin_kind;
    const char *     plugin_lib_name;
} neu_adapter_info_t;

#endif
