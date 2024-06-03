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

#ifndef _NEU_RW_HANDLE_H_
#define _NEU_RW_HANDLE_H_

#include <nng/nng.h>

#include "adapter.h"

void handle_rw_init();
void handle_rw_uninit();
void handle_read(nng_aio *aio);
void handle_read_paginate(nng_aio *aio);
void handle_write(nng_aio *aio);
void handle_write_tags(nng_aio *aio);
void handle_write_gtags(nng_aio *aio);
void handle_read_resp(nng_aio *aio, neu_resp_read_group_t *resp);
void handle_read_paginate_resp(nng_aio *                       aio,
                               neu_resp_read_group_paginate_t *resp);

#endif