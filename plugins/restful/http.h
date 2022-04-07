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

#include "adapter.h"
#include "errcodes.h"

#ifdef __cplusplus
extern "C" {
#endif

int http_get_body(nng_aio *aio, void **data, size_t *data_size);

// Find query parameter value of the given name.
//
// On failure, returns NULL.
// On success, returns an alias pointer to the value following name in the url,
// if `len_p` is not NULL, then stores the length of the value in `*len_p`.
//
// Example
// -------
// 1. http_get_param("/?key=val", "key", &len) will return a pointer to "val"
//    and store 3 in len.
// 2. http_get_param("/?key&foo", "key", &len) will return a pointer to "&foo"
//    and store 0 in len.
// 3. http_get_param("/?foo=bar", "key", &len) will return NULL and will not
//    touch len.
const char *http_get_param(nng_aio *aio, const char *name, size_t *len);

// Returns 0 on success.
// On failure returns
//    NEU_ERR_ENOENT if no query parameter named `name`.
//    NEU_ERR_EINVAL if the query parameter value is not valid.
int http_get_param_uintmax(nng_aio *aio, const char *name, uintmax_t *param);
int http_get_param_uint32(nng_aio *aio, const char *name, uint32_t *param);
int http_get_param_uint64(nng_aio *aio, const char *name, uint64_t *param);
int http_get_param_node_type(nng_aio *aio, const char *name,
                             neu_node_type_e *param);
int http_get_param_node_id(nng_aio *aio, const char *name,
                           neu_node_id_t *param);

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

#ifdef __cplusplus
}
#endif

#endif
