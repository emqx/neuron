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

#ifndef _NEU_HTTP_HANDLER_H_
#define _NEU_HTTP_HANDLER_H_

#include <stdlib.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "utils/neu_jwt.h"
#include "json/neu_json_error.h"

#define NEU_PROCESS_HTTP_REQUEST(aio, req_type, decode_fun, func)            \
    {                                                                        \
        char *    req_data      = NULL;                                      \
        size_t    req_data_size = 0;                                         \
        req_type *req           = NULL;                                      \
                                                                             \
        if (neu_http_get_body((aio), (void **) &req_data, &req_data_size) == \
                0 &&                                                         \
            decode_fun(req_data, &req) == 0) {                               \
            { func };                                                        \
            decode_fun##_free(req);                                          \
        } else {                                                             \
            neu_http_bad_request(aio, "{\"error\": 1002}");                  \
        }                                                                    \
        free(req_data);                                                      \
    }

#define NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(aio, req_type, decode_fun, func) \
    {                                                                          \
        if (!disable_jwt) {                                                    \
            char *jwt =                                                        \
                (char *) neu_http_get_header(aio, (char *) "Authorization");   \
                                                                               \
            NEU_JSON_RESPONSE_ERROR(neu_jwt_validate(jwt), {                   \
                if (error_code.error != NEU_ERR_SUCCESS) {                     \
                    neu_http_response(aio, error_code.error, result_error);    \
                    free(result_error);                                        \
                    return;                                                    \
                }                                                              \
            })                                                                 \
        }                                                                      \
        NEU_PROCESS_HTTP_REQUEST(aio, req_type, decode_fun, func);             \
    }

#define NEU_VALIDATE_JWT(aio)                                                \
    {                                                                        \
        if (!disable_jwt) {                                                  \
            char *jwt =                                                      \
                (char *) neu_http_get_header(aio, (char *) "Authorization"); \
                                                                             \
            NEU_JSON_RESPONSE_ERROR(neu_jwt_validate(jwt), {                 \
                if (error_code.error != NEU_ERR_SUCCESS) {                   \
                    neu_http_response(aio, error_code.error, result_error);  \
                    free(result_error);                                      \
                    return;                                                  \
                }                                                            \
            });                                                              \
        }                                                                    \
    }

#define NEU_VALIDATE_JWT_WITH_USER(aio, user)                                \
    {                                                                        \
        if (!disable_jwt) {                                                  \
            char *jwt =                                                      \
                (char *) neu_http_get_header(aio, (char *) "Authorization"); \
                                                                             \
            NEU_JSON_RESPONSE_ERROR(neu_jwt_validate(jwt), {                 \
                if (error_code.error != NEU_ERR_SUCCESS) {                   \
                    neu_http_response(aio, error_code.error, result_error);  \
                    free(result_error);                                      \
                    return;                                                  \
                } else {                                                     \
                    neu_jwt_decode_user(jwt, user);                          \
                }                                                            \
            });                                                              \
        }                                                                    \
    }

enum neu_http_method {
    NEU_HTTP_METHOD_UNDEFINE = 0x0,
    NEU_HTTP_METHOD_GET,
    NEU_HTTP_METHOD_POST,
    NEU_HTTP_METHOD_PUT,
    NEU_HTTP_METHOD_DELETE,
    NEU_HTTP_METHOD_OPTIONS
};
enum neu_http_handler_type {
    NEU_HTTP_HANDLER_FUNCTION = 0x0,
    NEU_HTTP_HANDLER_DIRECTORY,
    NEU_HTTP_HANDLER_REDIRECT,
};

struct neu_http_handler {
    enum neu_http_method       method;
    char *                     url;
    enum neu_http_handler_type type;
    union neu_http_handler_value {
        void *handler;
        char *path;
        char *dst_url;
    } value;
};

int  neu_http_add_handler(nng_http_server *              server,
                          const struct neu_http_handler *http_handler);
void neu_http_handle_cors(nng_aio *aio);

#endif
