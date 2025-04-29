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

#ifndef _NEU_PLUGIN_REST_DATALAYERS_HANDLE_H_
#define _NEU_PLUGIN_REST_DATALAYERS_HANDLE_H_

#include <nng/nng.h>

#include "adapter.h"

void handle_datalayers_get_groups(nng_aio *aio);
void handle_datalayers_get_groups_resp(nng_aio *                       aio,
                                       neu_resp_get_subscribe_group_t *groups);

void handle_datalayers_get_tags(nng_aio *aio);
void handle_datalayers_get_tags_resp(nng_aio *                       aio,
                                     neu_resp_get_sub_driver_tags_t *tags);

#endif