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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "handle.h"
#include "neu_log.h"
#include "rest.h"

struct neu_rest_context {
    nng_http_server *server;
};

static int rest_add_handler(nng_http_server *              server,
                            const struct neu_rest_handler *rest_handler)
{
    nng_http_handler *handler;
    int               ret        = -1;
    char              method[16] = { 0 };

    switch (rest_handler->type) {
    case NEU_REST_HANDLER_FUNCTION:
        ret = nng_http_handler_alloc(&handler, rest_handler->url,
                                     rest_handler->value.handler);
        break;
    case NEU_REST_HANDLER_DIRECTORY:
        ret = nng_http_handler_alloc_directory(&handler, rest_handler->url,
                                               rest_handler->value.path);
        break;
    }

    if (ret != 0) {
        return -1;
    }

    switch (rest_handler->method) {
    case NEU_REST_METHOD_GET:
        ret = nng_http_handler_set_method(handler, "GET");
        strncpy(method, "GET", sizeof(method));
        break;
    case NEU_REST_METHOD_POST:
        ret = nng_http_handler_set_method(handler, "POST");
        strncpy(method, "POST", sizeof(method));
        break;
    case NEU_REST_METHOD_PUT:
        ret = nng_http_handler_set_method(handler, "PUT");
        strncpy(method, "PUT", sizeof(method));
        break;
    case NEU_REST_METHOD_DELETE:
        ret = nng_http_handler_set_method(handler, "DELETE");
        strncpy(method, "DELETE", sizeof(method));
        break;
    case NEU_REST_METHOD_OPTIONS:
        ret = nng_http_handler_set_method(handler, "OPTIONS");
        strncpy(method, "OPTIONS", sizeof(method));
        break;
    }

    if (ret != 0) {
        return -1;
    }

    ret = nng_http_server_add_handler(server, handler);
    log_info("rest add handler, method: %s, url: %s", method,
             rest_handler->url);

    return ret;
}

void neu_rest_stop(neu_rest_context_t *ctx)
{
    nng_http_server_stop(ctx->server);
    nng_http_server_release(ctx->server);
    free(ctx);
}

neu_rest_context_t *neu_rest_start(void)
{
    nng_http_server *              server;
    nng_url *                      url;
    uint32_t                       n_handler     = 0;
    const struct neu_rest_handler *rest_handlers = NULL;
    neu_rest_context_t *           context       = NULL;

    int ret = nng_url_parse(&url, "http://0.0.0.0:7000");
    if (ret != 0) {
        return NULL;
    }

    ret = nng_http_server_hold(&server, url);
    if (ret != 0) {
        return NULL;
    }

    neu_rest_all_handler(&rest_handlers, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        rest_add_handler(server, &rest_handlers[i]);
    }

    ret = nng_http_server_start(server);
    if (ret != 0) {
        return NULL;
    }

    nng_url_free(url);

    context         = calloc(1, sizeof(neu_rest_context_t));
    context->server = server;

    return context;
}
