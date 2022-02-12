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

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <jwt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "json/neu_json_fn.h"
#include "json/neu_json_login.h"
#include "json/neu_json_logout.h"
#include "json/neu_json_tty.h"

#include "config.h"
#include "handle.h"
#include "http.h"
#include "neu_file.h"
#include "utils/neu_jwt.h"

#include "normal_handle.h"

void handle_get_ttys(nng_aio *aio)
{
    neu_json_get_tty_resp_t ttys_res = { 0 };

    VALIDATE_JWT(aio);

    ttys_res.n_tty = tty_file_list_get(&ttys_res.ttys);
    if (ttys_res.n_tty == -1) {
        http_bad_request(aio, "{\"error\": 400}");
        return;
    } else {
        char *result = NULL;

        neu_json_encode_by_fn(&ttys_res, neu_json_encode_get_tty_resp, &result);
        http_ok(aio, result);

        for (int i = 0; i < ttys_res.n_tty; i++) {
            free(ttys_res.ttys[i]);
        }
        free(result);
        free(ttys_res.ttys);
    }
}

void handle_ping(nng_aio *aio)
{
    VALIDATE_JWT(aio);

    http_ok(aio, "{}");
}

void handle_login(nng_aio *aio)
{
    REST_PROCESS_HTTP_REQUEST(
        aio, neu_json_login_req_t, neu_json_decode_login_req, {
            neu_json_login_resp_t login_resp = { 0 };
            char *                name       = "admin";
            char *                password   = "0000";
            int                   ret        = 0;

            if (strcmp(req->name, name) == 0 &&
                strcmp(req->pass, password) == 0) {

                char *token  = NULL;
                char *result = NULL;

                ret = neu_jwt_new(&token);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_NEED_TOKEN, {
                        http_response(aio, error_code.error, result_error);
                        jwt_free_str(token);
                    });
                }

                login_resp.token = calloc(strlen(token), sizeof(char));
                login_resp.token = token;

                neu_json_encode_by_fn(&login_resp, neu_json_encode_login_resp,
                                      &result);
                http_ok(aio, result);
                free(login_resp.token);
            } else {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_USER_OR_PASSWORD, {
                    http_response(aio, error_code.error, result_error);
                });
            }
        })
}

void handle_logout(nng_aio *aio)
{
    char *jwt = (char *) http_get_header(aio, (char *) "Authorization");
    NEU_JSON_RESPONSE_ERROR(neu_jwt_validate(jwt), {
        if (error_code.error == NEU_ERR_SUCCESS) {
            neu_jwt_destroy();
        }
        http_response(aio, error_code.error, result_error);
    });
}

void handle_get_plugin_schema(nng_aio *aio)
{
    VALIDATE_JWT(aio);

    size_t len = 0;
    char * buf = NULL;
    buf        = file_string_read(&len, "./plugin_param_schema.json");
    if (NULL == buf) {
        log_info("open ./plugin_param_schema.json error: %d", errno);
        http_not_found(aio, "{\"status\": \"error\"}");
        return;
    }

    http_ok(aio, buf);
    free(buf);
}
