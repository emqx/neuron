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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "define.h"
#include "errcodes.h"
#include "handle.h"
#include "parser/neu_json_log.h"
#include "utils/http.h"
#include "utils/log.h"
#include "utils/neu_jwt.h"
#include "json/neu_json_fn.h"

#include "log_handle.h"
#include "utils/utarray.h"

#define LOG_DIR "logs"

void handle_log_level(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_log_level_req_t,
        neu_json_decode_update_log_level_req, {
            if (req->node_name != NULL &&
                strlen(req->node_name) > NEU_NODE_NAME_LEN) {
                CHECK_NODE_NAME_LENGTH_ERR;
            } else {
                neu_reqresp_head_t         header    = { 0 };
                neu_req_update_log_level_t cmd       = { 0 };
                int                        log_level = -1;

                if (0 == strcmp(req->log_level, NEU_LOG_LEVEL_DEBUG)) {
                    log_level = ZLOG_LEVEL_DEBUG;
                } else if (0 == strcmp(req->log_level, NEU_LOG_LEVEL_INFO)) {
                    log_level = ZLOG_LEVEL_INFO;
                } else if (0 == strcmp(req->log_level, NEU_LOG_LEVEL_NOTICE)) {
                    log_level = ZLOG_LEVEL_NOTICE;
                } else if (0 == strcmp(req->log_level, NEU_LOG_LEVEL_WARN)) {
                    log_level = ZLOG_LEVEL_WARN;
                } else if (0 == strcmp(req->log_level, NEU_LOG_LEVEL_ERROR)) {
                    log_level = ZLOG_LEVEL_ERROR;
                } else if (0 == strcmp(req->log_level, NEU_LOG_LEVEL_FATAL)) {
                    log_level = ZLOG_LEVEL_FATAL;
                } else {
                    nlog_error("Failed to modify log_level of the node, "
                               "node_name:%s, log_level: %s",
                               req->node_name, req->log_level);
                }

                if (log_level == -1) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
                        neu_http_response(aio, error_code.error, result_error);
                    });
                } else {
                    header.ctx             = aio;
                    header.type            = NEU_REQ_UPDATE_LOG_LEVEL;
                    header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;
                    cmd.core               = req->core;
                    cmd.log_level          = log_level;
                    if (req->node_name != NULL) {
                        strcpy(cmd.node, req->node_name);
                    }

                    int ret = neu_plugin_op(plugin, header, &cmd);
                    if (ret != 0) {
                        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                            neu_http_response(aio, NEU_ERR_IS_BUSY,
                                              result_error);
                        });
                    }
                }
            }
        })
}

void handle_log_list(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);

    char log_dir[PATH_MAX] = { 0 };
    getcwd(log_dir, PATH_MAX - 1);
    strncat(log_dir, "/", sizeof(log_dir) - strlen(log_dir) - 1);
    strncat(log_dir, LOG_DIR, sizeof(log_dir) - strlen(log_dir) - 1);
    DIR *dir = opendir(log_dir);
    if (NULL == dir) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_NOT_EXIST, {
            neu_http_response(aio, NEU_ERR_FILE_NOT_EXIST, result_error);
        });
    }

    UT_icd    icd       = { sizeof(neu_resp_log_file_t), NULL, NULL, NULL };
    UT_array *log_files = NULL;
    utarray_new(log_files, &icd);

    struct dirent *entry = NULL;
    while (NULL != (entry = readdir(dir))) {
        if (NULL == strstr(entry->d_name, ".log")) {
            continue;
        }

        char full_path[PATH_MAX] = { 0 };
        strncpy(full_path, log_dir, sizeof(full_path) - 1);
        strncat(full_path, "/", sizeof(full_path) - strlen(full_path) - 1);
        strncat(full_path, entry->d_name,
                sizeof(full_path) - strlen(full_path) - 1);
        struct stat st = { 0 };
        if (0 != stat(full_path, &st)) {
            continue;
        }

        neu_resp_log_file_t log_file = { 0 };
        strncpy(log_file.file, entry->d_name, sizeof(log_file.file) - 1);
        log_file.size = st.st_size;
        utarray_push_back(log_files, &log_file);
    }
    closedir(dir);

    char *result = NULL;
    neu_json_encode_by_fn(log_files, neu_json_encode_log_list_resp, &result);
    neu_http_ok(aio, result);
    free(result);
    utarray_free(log_files);
}

static int response(nng_aio *aio, char *content, enum nng_http_status status)
{
    nng_http_res *res = NULL;

    nng_http_res_alloc(&res);

    nng_http_res_set_header(res, "Content-Type", "text/plain");
    nng_http_res_set_header(res, "Access-Control-Allow-Origin", "*");
    nng_http_res_set_header(res, "Access-Control-Allow-Methods",
                            "POST,GET,PUT,DELETE,OPTIONS");
    nng_http_res_set_header(res, "Access-Control-Allow-Headers", "*");

    if (content != NULL && strlen(content) > 0) {
        nng_http_res_copy_data(res, content, strlen(content));
    }

    nng_http_res_set_status(res, status);

    nng_http_req *nng_req = nng_aio_get_input(aio, 0);
    nlog_notice("<%p> %s %s [%d]", aio, nng_http_req_get_method(nng_req),
                nng_http_req_get_uri(nng_req), status);

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);

    return 0;
}

void handle_log_file(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);

    char file_name[NEU_NODE_NAME_LEN] = { 0 };
    if (neu_http_get_param_str(aio, "name", file_name, sizeof(file_name)) <=
        0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        })
        return;
    }

    char path[PATH_MAX] = { 0 };
    getcwd(path, PATH_MAX - 1);
    strncat(path, "/", sizeof(path) - strlen(path) - 1);
    strncat(path, LOG_DIR, sizeof(path) - strlen(path) - 1);

    strncat(path, "/", sizeof(path) - strlen(path) - 1);
    strncat(path, file_name, sizeof(path) - strlen(path) - 1);

    FILE *file = fopen(path, "r");
    if (NULL == file) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_NOT_EXIST, {
            neu_http_response(aio, NEU_ERR_FILE_NOT_EXIST, result_error);
        });
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *file_content = (char *) malloc(file_size + 1);
    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);

    response(aio, file_content, NNG_HTTP_STATUS_OK);
    free(file_content);
}
