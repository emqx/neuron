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

#include "proxy.h"
#include "utils/log.h"

int rest_proxy_handler(const struct neu_rest_handler *rest_handler,
                       nng_http_handler **            handler)
{
    int              ret    = 0;
    const char *     dst    = rest_handler->value.dst_url;
    nng_url *        url    = NULL;
    nng_http_client *client = NULL;

    ret = nng_url_parse(&url, rest_handler->value.dst_url);
    if (0 != ret) {
        nlog_error("parse url: `%s` fail", dst);
        return ret;
    }

    ret = nng_http_client_alloc(&client, url);
    if (0 != ret) {
        nlog_error("alloc http client for `%s` fail", dst);
        goto error;
    }

    ret = nng_http_handler_alloc(handler, rest_handler->url, handle_proxy);
    if (0 != ret) {
        nlog_error("alloc proxy handler for `%s` fail", dst);
        goto error;
    }

    ret = nng_http_handler_set_tree(*handler);
    if (0 != ret) {
        nlog_error("proxy handler for `%s` set tree fail", dst);
        goto error;
    }

    ret = nng_http_handler_set_method(*handler, NULL);
    if (0 != ret) {
        nlog_error("proxy handler for `%s` set all method fail", dst);
        goto error;
    }

    ret = nng_http_handler_set_data(*handler, client, NULL);
    if (0 != ret) {
        nlog_error("proxy handler for `%s` set data fail", dst);
        goto error;
    }

    return ret;

error:
    if (NULL != url) {
        nng_url_free(url);
    }
    if (NULL != *handler) {
        nng_http_handler_free(*handler);
    }
    if (NULL != client) {
        nng_http_client_free(client);
    }
    return ret;
}

void handle_proxy(nng_aio *aio)
{
    nng_http_res *    res     = NULL;
    nng_http_req *    req     = nng_aio_get_input(aio, 0);
    nng_http_handler *handler = nng_aio_get_input(aio, 1);
    nng_http_client * client  = nng_http_handler_get_data(handler);

    if (0 != nng_http_res_alloc(&res)) {
        nlog_error("alloc http resp fail");
        return;
    }

    nng_http_client_transact(client, req, res, aio);
}
