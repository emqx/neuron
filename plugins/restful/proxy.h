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

#ifndef NEU_PLUGIN_REST_PROXY_H
#define NEU_PLUGIN_REST_PROXY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "handle.h"

int  rest_proxy_handler(const struct neu_rest_handler *rest_handler,
                        nng_http_handler **            handler);
void handle_proxy(nng_aio *aio);

#ifdef __cplusplus
}
#endif

#endif
