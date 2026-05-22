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

#include "core/manager.h"
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

void handle_global_log_level(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_global_log_level_req_t,
        neu_json_decode_update_global_log_level_req, {
            neu_reqresp_head_t                header    = { 0 };
            neu_req_update_global_log_level_t cmd       = { 0 };
            int                               log_level = -1;

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
                nlog_error("Failed to set global log_level, invalid level: %s",
                           req->log_level);
            }

            if (log_level == -1) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
                    neu_http_response(aio, error_code.error, result_error);
                });
            } else {
                header.ctx             = aio;
                header.type            = NEU_REQ_UPDATE_GLOBAL_LOG_LEVEL;
                header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;
                cmd.log_level          = log_level;

                nlog_info(
                    "Setting global log level to %s for core and all drivers",
                    req->log_level);

                int ret = neu_plugin_op(plugin, header, &cmd);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                        neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
            }
        })
}

void handle_get_global_log_level(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);

    int         global_log_level = neu_manager_get_global_log_level();
    const char *log_level_str    = "unknown";

    switch (global_log_level) {
    case ZLOG_LEVEL_DEBUG:
        log_level_str = NEU_LOG_LEVEL_DEBUG;
        break;
    case ZLOG_LEVEL_INFO:
        log_level_str = NEU_LOG_LEVEL_INFO;
        break;
    case ZLOG_LEVEL_NOTICE:
        log_level_str = NEU_LOG_LEVEL_NOTICE;
        break;
    case ZLOG_LEVEL_WARN:
        log_level_str = NEU_LOG_LEVEL_WARN;
        break;
    case ZLOG_LEVEL_ERROR:
        log_level_str = NEU_LOG_LEVEL_ERROR;
        break;
    case ZLOG_LEVEL_FATAL:
        log_level_str = NEU_LOG_LEVEL_FATAL;
        break;
    default:
        log_level_str = "unknown";
        break;
    }

    char json_response[256];
    snprintf(json_response, sizeof(json_response), "{\"level\":\"%s\"}",
             log_level_str);

    neu_http_ok(aio, json_response);
}
