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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "parser/neu_json_add_tag.h"
#include "parser/neu_json_delete_tag.h"
#include "parser/neu_json_get_tag.h"
#include "parser/neu_json_login.h"
#include "parser/neu_json_parser.h"
#include "parser/neu_json_updata_tag.h"

#include "handle.h"

static void bad_request(nng_aio *aio, char *error);
static void ping(nng_aio *aio);
static void read(nng_aio *aio);
static void write(nng_aio *aio);
static void login(nng_aio *aio);
static void logout(nng_aio *aio);
static void get_tags(nng_aio *aio);
static void add_tags(nng_aio *aio);
static void delete_tags(nng_aio *aio);

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
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/login",
        .value.handler = login,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/logout",
        .value.handler = logout,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = add_tags,

    },
    {
        .method        = NEU_REST_METHOD_GET,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = get_tags,
    },
    {
        .method        = NEU_REST_METHOD_DELETE,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/tags",
        .value.handler = delete_tags,
    }
};

void neu_rest_init_all_handler(const struct neu_rest_handler **handlers,
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

static void login(nng_aio *aio)
{
    int                         ret             = -1;
    nng_http_res *              res             = NULL;
    char *                      res_data        = NULL;
    char *                      req_data        = NULL;
    char                        req_cdata[1024] = { 0 };
    size_t                      req_data_size   = 0;
    nng_http_req *              req             = nng_aio_get_input(aio, 0);
    struct neu_parse_login_req *login_req       = NULL;
    struct neu_parse_login_res  login_res       = {
        .function = NEU_PARSE_OP_LOGIN,
        .error    = 1,
        .token    = "fake token",
    };

    nng_http_req_get_data(req, (void **) &req_data, &req_data_size);
    if (req_data_size <= 0) {
        neu_parse_encode(&login_res, &res_data);
        bad_request(aio, res_data);
        free(res_data);
        return;
    }

    memcpy(req_cdata, req_data, req_data_size);
    ret = neu_parse_decode(req_cdata, (void **) &login_req);
    if (ret != 0) {
        neu_parse_encode(&login_res, &res_data);
        bad_request(aio, res_data);
        free(res_data);
        return;
    }

    login_res.error = 0;
    login_res.uuid  = login_req->uuid;

    neu_parse_encode(&login_res, &res_data);
    nng_http_res_alloc(&res);

    nng_http_res_copy_data(res, res_data, strlen(res_data));
    nng_http_res_set_status(res, NNG_HTTP_STATUS_OK);
    nng_http_res_set_header(res, "Content-Type", "application/json");

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
    free(res_data);
    neu_parse_decode_free(login_req);
}

static void logout(nng_aio *aio)
{
    (void) aio;
}

static void add_tags(nng_aio *aio)
{
    int           ret             = -1;
    nng_http_res *res             = NULL;
    char *        res_data        = NULL;
    char *        req_data        = NULL;
    char          req_cdata[1024] = { 0 };
    size_t        req_data_size   = 0;

    nng_http_req *req = nng_aio_get_input(aio, 0);

    struct neu_parse_add_tags_req *add_tags_req = NULL;
    struct neu_parse_add_tags_res  add_tags_res = {
        .function = NEU_PARSE_OP_ADD_TAGS,
        .error    = 1,
    };

    nng_http_req_get_data(req, (void **) &req_data, &req_data_size);
    if (req_data_size <= 0) {
        neu_parse_encode(&add_tags_res, &res_data);
        bad_request(aio, res_data);
        free(res_data);
        return;
    }

    memcpy(req_cdata, req_data, req_data_size);
    ret = neu_parse_decode(req_cdata, (void **) &add_tags_req);
    if (ret != 0) {
        neu_parse_encode(&add_tags_res, &res_data);
        bad_request(aio, res_data);
        free(res_data);
        return;
    }

    add_tags_res.error = 0;
    add_tags_res.uuid  = add_tags_req->uuid;

    neu_parse_encode(&add_tags_res, &res_data);
    nng_http_res_alloc(&res);

    nng_http_res_copy_data(res, res_data, strlen(res_data));
    nng_http_res_set_status(res, NNG_HTTP_STATUS_OK);
    nng_http_res_set_header(res, "Content-Type", "application/json");

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
    free(res_data);
    neu_parse_decode_free(add_tags_req);
}

static void get_tags(nng_aio *aio)
{
    int           ret             = -1;
    nng_http_res *res             = NULL;
    char *        res_data        = NULL;
    char *        req_data        = NULL;
    char          req_cdata[1024] = { 0 };
    size_t        req_data_size   = 0;

    nng_http_req *req = nng_aio_get_input(aio, 0);

    struct neu_parse_get_tags_req *get_tags_req = NULL;
    struct neu_parse_get_tags_res  get_tags_res = {
        .function = NEU_PARSE_OP_GET_TAGS,
        .error    = 1,
    };

    nng_http_req_get_data(req, (void **) &req_data, &req_data_size);
    if (req_data_size <= 0) {
        neu_parse_encode(&get_tags_res, &res_data);
        bad_request(aio, res_data);
        free(res_data);
        return;
    }

    memcpy(req_cdata, req_data, req_data_size);
    ret = neu_parse_decode(req_cdata, (void **) &get_tags_req);
    if (ret != 0) {
        neu_parse_encode(&get_tags_res, &res_data);
        bad_request(aio, res_data);
        free(res_data);
        return;
    }

    get_tags_res.error = 0;
    get_tags_res.uuid  = get_tags_req->uuid;

    neu_parse_encode(&get_tags_res, &res_data);
    nng_http_res_alloc(&res);

    nng_http_res_copy_data(res, res_data, strlen(res_data));
    nng_http_res_set_status(res, NNG_HTTP_STATUS_OK);
    nng_http_res_set_header(res, "Content-Type", "application/json");

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
    free(res_data);
    neu_parse_decode_free(get_tags_req);
}

static void delete_tags(nng_aio *aio)
{
    int           ret             = -1;
    nng_http_res *res             = NULL;
    char *        res_data        = NULL;
    char *        req_data        = NULL;
    char          req_cdata[1024] = { 0 };
    size_t        req_data_size   = 0;

    nng_http_req *req = nng_aio_get_input(aio, 0);

    struct neu_parse_delete_tags_req *delete_tags_req = NULL;
    struct neu_parse_delete_tags_res  delete_tags_res = {
        .function = NEU_PARSE_OP_DELETE_TAGS,
        .error    = 1,
    };

    nng_http_req_get_data(req, (void **) &req_data, &req_data_size);
    if (req_data_size <= 0) {
        neu_parse_encode(&delete_tags_res, &res_data);
        bad_request(aio, res_data);
        free(res_data);
        return;
    }

    memcpy(req_cdata, req_data, req_data_size);
    ret = neu_parse_decode(req_cdata, (void **) &delete_tags_res);
    if (ret != 0) {
        neu_parse_encode(&delete_tags_res, &res_data);
        bad_request(aio, res_data);
        free(res_data);
        return;
    }

    delete_tags_res.error = 0;
    delete_tags_res.uuid  = delete_tags_req->uuid;

    neu_parse_encode(&delete_tags_res, &res_data);
    nng_http_res_alloc(&res);

    nng_http_res_copy_data(res, res_data, strlen(res_data));
    nng_http_res_set_status(res, NNG_HTTP_STATUS_OK);
    nng_http_res_set_header(res, "Content-Type", "application/json");

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
    free(res_data);
    neu_parse_decode_free(delete_tags_req);
}

static void bad_request(nng_aio *aio, char *error)
{

    nng_http_res *res = NULL;

    nng_http_res_alloc(&res);

    nng_http_res_copy_data(res, error, strlen(error));
    nng_http_res_set_status(res, NNG_HTTP_STATUS_BAD_REQUEST);
    nng_http_res_set_header(res, "Content-Type", "application/json");

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
}
