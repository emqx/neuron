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

#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "handle.h"

static void ping(nng_aio *aio);
static void read(nng_aio *aio);
static void write(nng_aio *aio);

struct neu_rest_handler rest_handlers[] = {
    {
        .method     = NEU_REST_METHOD_GET,
        .type       = NEU_REST_HANDLER_DIRECTORY,
        .url        = "",
        .value.path = "./dist",
    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/ping",
        .value.handler = ping,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/read",
        .value.handler = read,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/write",
        .value.handler = write,
    }
};

void neu_rest_all_handler(const struct neu_rest_handler **handlers,
                          uint32_t *                      size)
{
    *handlers = rest_handlers;
    *size     = sizeof(rest_handlers) / sizeof(struct neu_rest_handler);
}

static void ping(nng_aio *aio)
{
    nng_http_res *res      = NULL;
    char *        res_data = "{\"status\":\"OK\"}";

    nng_http_res_alloc(&res);

    nng_http_res_copy_data(res, res_data, strlen(res_data));
    nng_http_res_set_status(res, NNG_HTTP_STATUS_OK);
    nng_http_res_set_header(res, "Content-Type", "application/json");

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
}

static void read(nng_aio *aio)
{
    (void) aio;
}

static void write(nng_aio *aio)
{
    (void) aio;
}
