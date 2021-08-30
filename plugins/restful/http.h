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

typedef struct {
    char *key;
    char *value;
} http_header_kv_t;

#define HTTP_HEADER_ADD_JSON(kv) \
    (kv).key   = "Content-Type"; \
    (kv).value = "application/json";

typedef struct {
    nng_aio *         aio;
    char *            content;
    http_header_kv_t *headers;
    int               header_size;
} http_response_t;

int http_get_body(nng_aio *aio, void **data, size_t *data_size);
int http_ok(http_response_t *response);
int http_created(http_response_t *response);
int http_bad_request(http_response_t *response);
int http_unauthorized(http_response_t *response);
int http_not_found(http_response_t *response);
int http_conflict(http_response_t *response);

#endif
