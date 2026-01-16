/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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
#ifndef _NEU_PLUGIN_REST_ADAPTER_HANDLE_H_
#define _NEU_PLUGIN_REST_ADAPTER_HANDLE_H_

#include <nng/nng.h>

#include "adapter.h"

void handle_add_adapter(nng_aio *aio);
void handle_update_adapter(nng_aio *aio);
void handle_del_adapter(nng_aio *aio);
void handle_get_adapter(nng_aio *aio);
void handle_get_adapter_resp(nng_aio *aio, neu_resp_get_node_t *nodes);

void handle_set_node_setting(nng_aio *aio);
void handle_get_node_setting(nng_aio *aio);
void handle_get_node_setting_resp(nng_aio *                    aio,
                                  neu_resp_get_node_setting_t *setting);

void handle_node_ctl(nng_aio *aio);
void handle_get_node_state(nng_aio *aio);
void handle_get_node_state_resp(nng_aio *aio, neu_resp_get_node_state_t *state);
void handle_get_nodes_state_resp(nng_aio *                   aio,
                                 neu_resp_get_nodes_state_t *states);

void handle_put_drivers(nng_aio *aio);
void handle_put_node_tag(nng_aio *aio);

#endif