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
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "utils/http_handler.h"
#include "utils/log.h"

/**
 * @brief 向HTTP服务器添加处理器
 * @param server HTTP服务器实例
 * @param http_handler HTTP处理器配置
 * @return 成功返回0，失败返回-1
 */
int neu_http_add_handler(nng_http_server *              server,
                         const struct neu_http_handler *http_handler)
{
    nng_http_handler *handler;
    int               ret        = -1;
    char              method[16] = { 0 };

    switch (http_handler->type) {
    case NEU_HTTP_HANDLER_FUNCTION:
        ret = nng_http_handler_alloc(&handler, http_handler->url,
                                     http_handler->value.handler);
        break;
    case NEU_HTTP_HANDLER_DIRECTORY:
        ret = nng_http_handler_alloc_directory(&handler, http_handler->url,
                                               http_handler->value.path);
        break;
    case NEU_HTTP_HANDLER_PROXY:
        ret = neu_http_proxy_handler(http_handler, &handler);
        break;
    case NEU_HTTP_HANDLER_REDIRECT:
        ret = nng_http_handler_alloc_redirect(&handler, http_handler->url, 0,
                                              http_handler->value.path);
        break;
    }

    if (ret != 0) {
        return -1;
    }

    switch (http_handler->method) {
    case NEU_HTTP_METHOD_GET:
        ret = nng_http_handler_set_method(handler, "GET");
        strncpy(method, "GET", sizeof(method));
        break;
    case NEU_HTTP_METHOD_POST:
        ret = nng_http_handler_set_method(handler, "POST");
        strncpy(method, "POST", sizeof(method));
        break;
    case NEU_HTTP_METHOD_PUT:
        ret = nng_http_handler_set_method(handler, "PUT");
        strncpy(method, "PUT", sizeof(method));
        break;
    case NEU_HTTP_METHOD_DELETE:
        ret = nng_http_handler_set_method(handler, "DELETE");
        strncpy(method, "DELETE", sizeof(method));
        break;
    case NEU_HTTP_METHOD_OPTIONS:
        ret = nng_http_handler_set_method(handler, "OPTIONS");
        strncpy(method, "OPTIONS", sizeof(method));
        break;
    default:
        break;
    }
    assert(ret == 0);

    ret = nng_http_server_add_handler(server, handler);
    nlog_info("http add handler, method: %s, url: %s, ret: %d", method,
              http_handler->url, ret);

    return ret;
}

/**
 * @brief 处理CORS（跨域资源共享）请求
 * @param aio NNG异步I/O对象
 */
void neu_http_handle_cors(nng_aio *aio)
{
    nng_http_res *res = NULL;

    nng_http_res_alloc(&res);

    nng_http_res_set_header(res, "Access-Control-Allow-Origin", "*");
    nng_http_res_set_header(res, "Access-Control-Allow-Methods",
                            "POST,GET,PUT,DELETE,OPTIONS");
    nng_http_res_set_header(res, "Access-Control-Allow-Headers", "*");

    nng_http_res_copy_data(res, " ", strlen(" "));
    nng_http_res_set_status(res, NNG_HTTP_STATUS_OK);

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
}
