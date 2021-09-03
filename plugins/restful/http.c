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

#include "http.h"

static int response(nng_aio *aio, char *content, enum nng_http_status status)
{
    nng_http_res *res = NULL;

    nng_http_res_alloc(&res);

    nng_http_res_set_header(res, "Content-Type", "application/json");

    if (content != NULL && strlen(content) > 0) {
        nng_http_res_copy_data(res, content, strlen(content));
    }

    nng_http_res_set_status(res, status);

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);

    nng_http_res_free(res);
    return 0;
}

int http_get_body(nng_aio *aio, void **data, size_t *data_size)
{
    nng_http_req *req = nng_aio_get_input(aio, 0);

    nng_http_req_get_data(req, data, data_size);
    if (*data_size <= 0) {
        return -1;
    } else {
        return 0;
    }
}

int http_ok(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_OK);
}

int http_created(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_CREATED);
}

int http_bad_request(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_BAD_GATEWAY);
}

int http_unauthorized(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_UNAUTHORIZED);
}

int http_not_found(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_NOT_FOUND);
}

int http_conflict(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_CONFLICT);
}
