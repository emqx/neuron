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

#ifndef _NEU_HTTP_H_
#define _NEU_HTTP_H_

#include <nng/nng.h>

#include "adapter.h"
#include "errcodes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Decode url encoded string.
// Return -1 if decode fails, otherwise return the number of bytes written to
// `buf` excluding the terminating NULL byte. A return value of `size` indicates
// buffer overflow.
ssize_t neu_url_decode(const char *s, size_t len, char *buf, size_t size);

int neu_http_get_body(nng_aio *aio, void **data, size_t *data_size);

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
const char *neu_http_get_param(nng_aio *aio, const char *name, size_t *len);

// Get query parameter as url decoded string.
// Return -2 if param does not exist, -1 if url decode fail, otherwise return
// the number of bytes written to `buf` excluding the terminating NULL byte. A
// return value of `size` indicates buffer overflow.
ssize_t neu_http_get_param_str(nng_aio *aio, const char *name, char *buf,
                               size_t size);

// Returns 0 on success.
// On failure returns
//    NEU_ERR_ENOENT if no query parameter named `name`.
//    NEU_ERR_EINVAL if the query parameter value is not valid.
int neu_http_get_param_uintmax(nng_aio *aio, const char *name,
                               uintmax_t *param);
int neu_http_get_param_node_type(nng_aio *aio, const char *name,
                                 neu_node_type_e *param);

int         neu_http_response(nng_aio *aio, neu_err_code_e code, char *content);
const char *neu_http_get_header(nng_aio *aio, char *name);
int         neu_http_ok(nng_aio *aio, char *content);
int         neu_http_created(nng_aio *aio, char *content);
int         neu_http_partial(nng_aio *aio, char *content);
int         neu_http_bad_request(nng_aio *aio, char *content);
int         neu_http_unauthorized(nng_aio *aio, char *content);
int         neu_http_not_found(nng_aio *aio, char *content);
int         neu_http_conflict(nng_aio *aio, char *content);
int         neu_http_internal_error(nng_aio *aio, char *content);

int neu_http_response_file(nng_aio *aio, void *data, size_t len,
                           const char *disposition);

int neu_http_post_otel_trace(uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#endif
