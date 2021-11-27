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

#ifndef _NEU_PLUGIN_HTTP_H_
#define _NEU_PLUGIN_HTTP_H_

#include <nng/nng.h>

#include "neu_errcodes.h"

int   http_get_body(nng_aio *aio, void **data, size_t *data_size);
char *http_get_param(nng_aio *aio, const char *name);
int   http_get_param_int(nng_aio *aio, const char *name, int32_t *param);

int         http_response(nng_aio *aio, neu_err_code_e code, char *content);
const char *http_get_header(nng_aio *aio, char *name);
int         http_ok(nng_aio *aio, char *content);
int         http_created(nng_aio *aio, char *content);
int         http_partial(nng_aio *aio, char *content);
int         http_bad_request(nng_aio *aio, char *content);
int         http_unauthorized(nng_aio *aio, char *content);
int         http_not_found(nng_aio *aio, char *content);
int         http_conflict(nng_aio *aio, char *content);
int         http_internal_error(nng_aio *aio, char *content);

#endif
