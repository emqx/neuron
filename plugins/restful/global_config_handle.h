/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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
#ifndef _NEU_PLUGIN_REST_GLOBAL_CONFIG_HANDLE_H_
#define _NEU_PLUGIN_REST_GLOBAL_CONFIG_HANDLE_H_

#include <nng/nng.h>

#include "adapter.h"

void handle_get_global_config(nng_aio *aio);
void handle_put_global_config(nng_aio *aio);

void handle_get_drivers(nng_aio *aio);

void handle_get_apps(nng_aio *aio);
void handle_put_apps(nng_aio *aio);

void handle_global_config_resp(nng_aio *aio, neu_reqresp_type_e type,
                               void *data);

#endif
