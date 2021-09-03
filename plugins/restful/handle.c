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
#include "http.h"

#define PROCESS_HTTP_REQUEST(aio, req_type, func)                             \
    {                                                                         \
        char *    req_data      = NULL;                                       \
        size_t    req_data_size = 0;                                          \
        req_type *req           = NULL;                                       \
        if (http_get_body((aio), (void **) &req_data, &req_data_size) == 0 && \
            neu_parse_decode(req_data, (void **) &req)) {                     \
            { func };                                                         \
            neu_parse_decode_free(req);                                       \
        } else {                                                              \
            http_bad_request(aio, "{\"error\": \"request body is wrong\"}");  \
        }                                                                     \
        if (req_data != NULL) {                                               \
            free(req_data);                                                   \
        }                                                                     \
    }

static void ping(nng_aio *aio);
static void read_tag(nng_aio *aio);
static void write_tag(nng_aio *aio);
static void login(nng_aio *aio);
static void logout(nng_aio *aio);
static void get_tags(nng_aio *aio);
static void add_tags(nng_aio *aio);
static void delete_tags(nng_aio *aio);

static neu_plugin_t *api_plugin = NULL;

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
        .value.handler = read_tag,
    },
    {
        .method        = NEU_REST_METHOD_POST,
        .type          = NEU_REST_HANDLER_FUNCTION,
        .url           = "/api/v2/write",
        .value.handler = write_tag,
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
                               uint32_t *size, neu_plugin_t *plugin)
{
    *handlers = rest_handlers;
    *size     = sizeof(rest_handlers) / sizeof(struct neu_rest_handler);

    api_plugin = plugin;
}

static void ping(nng_aio *aio)
{
    http_ok(aio, "{\"status\": \"OK\"}");
}

static void read_tag(nng_aio *aio)
{
    (void) aio;
}

static void write_tag(nng_aio *aio)
{
    (void) aio;
}

static void login(nng_aio *aio)
{
    PROCESS_HTTP_REQUEST(aio, struct neu_parse_login_req,
                         { http_ok(aio, "{\"status\": \"ok\"}"); })
}

static void logout(nng_aio *aio)
{
    (void) aio;
}

static void add_tags(nng_aio *aio)
{
    (void) aio;
}

static void get_tags(nng_aio *aio)
{
    (void) aio;
}

static void delete_tags(nng_aio *aio)
{
    (void) aio;
}
