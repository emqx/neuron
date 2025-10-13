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

#ifndef _NEU_NORMAL_HANDLE_H_
#define _NEU_NORMAL_HANDLE_H_

#include <nng/nng.h>

#include "msg.h"

void handle_ping(nng_aio *aio);
void handle_login(nng_aio *aio);
void handle_password(nng_aio *aio);
void handle_get_plugin_schema(nng_aio *aio);
void handle_get_plugin_schema_resp(nng_aio *aio, neu_resp_check_schema_t *resp);
void handle_status(nng_aio *aio);

void handle_get_user(nng_aio *aio);
void handle_add_user(nng_aio *aio);
void handle_update_user(nng_aio *aio);
void handle_delete_user(nng_aio *aio);

void handle_export_db(nng_aio *aio);

#endif
