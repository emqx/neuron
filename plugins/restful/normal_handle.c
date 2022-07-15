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

#include "utils/log.h"

#include "parser/neu_json_login.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "http.h"
#include "utils/neu_jwt.h"

#include "normal_handle.h"

void handle_ping(nng_aio *aio)
{
    http_ok(aio, "{}");
}

void handle_login(nng_aio *aio)
{
    REST_PROCESS_HTTP_REQUEST(
        aio, neu_json_login_req_t, neu_json_decode_login_req, {
            neu_json_login_resp_t login_resp = { 0 };
            char *                name       = "admin";
            char *                password   = "0000";

            if (strcmp(req->name, name) == 0 &&
                strcmp(req->pass, password) == 0) {

                char *token  = NULL;
                char *result = NULL;

                int ret = neu_jwt_new(&token);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_NEED_TOKEN, {
                        http_response(aio, error_code.error, result_error);
                        jwt_free_str(token);
                    });
                }

                login_resp.token = token;

                neu_json_encode_by_fn(&login_resp, neu_json_encode_login_resp,
                                      &result);
                http_ok(aio, result);
                jwt_free_str(token);
                free(result);
            } else {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_USER_OR_PASSWORD, {
                    http_response(aio, error_code.error, result_error);
                });
            }
        })
}

static char *file_string_read(size_t *length, const char *const path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        errno   = 0;
        *length = 0;
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    *length = (size_t) ftell(fp);
    if (0 == *length) {
        fclose(fp);
        return NULL;
    }

    char *data = NULL;
    data       = (char *) malloc((*length + 1) * sizeof(char));
    if (NULL != data) {
        data[*length] = '\0';
        fseek(fp, 0, SEEK_SET);
        size_t read = fread(data, sizeof(char), *length, fp);
        if (read != *length) {
            *length = 0;
            free(data);
            data = NULL;
        }
    } else {
        *length = 0;
    }

    fclose(fp);
    return data;
}

void handle_get_plugin_schema(nng_aio *aio)
{
    size_t len              = 0;
    char   schema_path[256] = { 0 };

    VALIDATE_JWT(aio);

    const char *plugin_name = http_get_param(aio, "plugin_name", &len);
    if (plugin_name == NULL || len == 0) {
        http_bad_request(aio, "{\"error\": 1002}");
        return;
    }

    snprintf(schema_path, sizeof(schema_path) - 1, "./plugins/schema/%s.json",
             plugin_name);

    char *buf = NULL;
    buf       = file_string_read(&len, schema_path);
    if (NULL == buf) {
        nlog_info("open %s error: %d", schema_path, errno);
        http_not_found(aio, "{\"status\": \"error\"}");
        return;
    }

    http_ok(aio, buf);
    free(buf);
}
