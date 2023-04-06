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

void handle_logs_files(nng_aio *aio)
{
    void * data = NULL;
    size_t len  = 0;
    int    rv   = 0;

    NEU_VALIDATE_JWT(aio);

    /* check whether the neuron directory exists */
    rv = access("../neuron", F_OK);
    if (0 != rv) {
        nlog_error("The neuron directory does not exists");
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_NOT_EXIST, {
            neu_http_response(aio, error_code.error, result_error);
        });
        return;
    }

    rv = system("rm -rf /tmp/neuron_debug.tar.gz");
    nlog_warn("remove old neuron_debug.tar.gz, rv: %d", rv);
    /* tar the neuron directory */
    rv = system("tar -zcvf /tmp/neuron_debug.tar.gz ../neuron");
    if (rv == -1) {
        nlog_error("failed to create neuron_debug.tar.gz, rv: %d", rv);
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_COMMAND_EXECUTION_FAILED, {
            neu_http_response(aio, error_code.error, result_error);
        });
        return;
    } else {
        if (WIFEXITED(rv)) {
            if (WEXITSTATUS(rv) < 0) {
                nlog_error("failed to create neuron_debug.tar.gz, rv: %d",
                           WEXITSTATUS(rv));
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_COMMAND_EXECUTION_FAILED, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }
        } else {
            nlog_error("failed to create neuron_debug.tar.gz, rv: %d",
                       WEXITSTATUS(rv));
            NEU_JSON_RESPONSE_ERROR(NEU_ERR_COMMAND_EXECUTION_FAILED, {
                neu_http_response(aio, error_code.error, result_error);
            });
            return;
        }
    }

    rv = read_file("/tmp/neuron_debug.tar.gz", &data, &len);
    if (0 != rv) {
        NEU_JSON_RESPONSE_ERROR(
            rv, { neu_http_response(aio, error_code.error, result_error); });
        return;
    }

    /* handle http response */
    rv = neu_http_response_file(aio, data, len,
                                "attachment; filename=neuron_debug.tar.gz");

    free(data);
    nlog_notice("download neuron_debug.tar.gz, ret: %d", rv);
}

void handle_log_level(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_log_level_req_t,
        neu_json_decode_update_log_level_req, {
            int                        ret    = 0;
            neu_reqresp_head_t         header = { 0 };
            neu_req_update_log_level_t cmd    = { 0 };
            zlog_category_t *          ct = zlog_get_category(req->node_name);
            zlog_category_t *          neuron = zlog_get_category("neuron");

            ret = zlog_level_switch(ct, ZLOG_LEVEL_DEBUG);
            if (ret != 0) {
                nlog_error("Modify node log level fail, node_name:%s, "
                           "ret: %d,",
                           req->node_name, ret);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
            }

            ret = zlog_level_switch(neuron, ZLOG_LEVEL_DEBUG);
            if (ret != 0) {
                nlog_error("Modify neuron log level fail, ret: %d", ret);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
            }

            header.ctx  = aio;
            header.type = NEU_REQ_UPDATE_LOG_LEVEL;
            strcpy(cmd.node, req->node_name);

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

int read_file(const char *file_name, void **datap, size_t *lenp)
{
    int         rv = 0;
    FILE *      f;
    struct stat st;
    size_t      len;
    void *      data;

    if (stat(file_name, &st) != 0) {
        return NEU_ERR_FILE_NOT_EXIST;
    }

    if ((f = fopen(file_name, "rb")) == NULL) {
        nlog_error("open fail: %s", file_name);
        return NEU_ERR_FILE_OPEN_FAILURE;
    }

    len = st.st_size;
    if (len > 0) {
        if ((data = malloc(len)) == NULL) {
            rv = NEU_ERR_EINTERNAL;
            goto done;
        }
        if (fread(data, 1, len, f) != len) {
            nlog_error("file read failued, errno = %d", errno);
            rv = NEU_ERR_FILE_READ_FAILURE;
            free(data);
            goto done;
        }
    } else {
        data = NULL;
    }

    *datap = data;
    *lenp  = len;
done:
    fclose(f);
    return (rv);
}
