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

UT_array *collect_log_files();

void handle_logs_files(nng_aio *aio)
{
    char   path[128] = { 0 };
    char   disp[128] = { 0 };
    void * data      = NULL;
    size_t len       = 0;
    int    rv        = 0;

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

    /* tar the neuron directory */
    rv = system("tar -zcvf neuron_debug.tar.gz ../neuron");
    if (0 != rv) {
        nlog_error("failed to create neuron_debug.tar.gz");
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_COMMAND_EXECUTION_FAILED, {
            neu_http_response(aio, error_code.error, result_error);
        });
        return;
    }

    strcpy(path, "neuron_debug.tar.gz");
    rv = read_file(path, &data, &len);
    if (0 != rv) {
        NEU_JSON_RESPONSE_ERROR(
            rv, { neu_http_response(aio, error_code.error, result_error); });
        return;
    }

    /* handle http response */
    strcpy(disp, "attachment; filename=neuron_debug.tar.gz");
    rv = neu_http_response_file(aio, data, len, disp);
    if (0 != rv) {
        return;
    }
}

void handle_log_level(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_log_level_req_t,
        neu_json_decode_update_log_level_req, {
            char node_name[NEU_NODE_NAME_LEN] = { 0 };

            UT_array *log_files = collect_log_files();

            utarray_foreach(log_files, char **, log_file)
            {
                if (strncmp(*log_file, req->node_name, strlen(*log_file) - 4) ==
                    0) {
                    strcpy(node_name, req->node_name);
                }
            }

            if (strlen(node_name) == 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_NODE_NOT_EXIST, {
                    neu_http_response(aio, error_code.error, result_error);
                });
            } else {
                int                        ret    = 0;
                neu_reqresp_head_t         header = { 0 };
                neu_req_update_log_level_t cmd    = { 0 };
                zlog_category_t *          ct = zlog_get_category(node_name);
                zlog_category_t *          neuron = zlog_get_category("neuron");

                ret = zlog_level_switch(ct, ZLOG_LEVEL_DEBUG);
                if (ret != 0) {
                    nlog_error(
                        "Modify node log level fail, node_name:%s, ret: %d,",
                        node_name, ret);
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
            }

            utarray_free(log_files);
        })
}

static inline bool ends_with(const char *str, const char *suffix)
{
    size_t m = strlen(str);
    size_t n = strlen(suffix);
    return m >= n && !strcmp(str + m - n, suffix);
}

UT_array *collect_log_files()
{
    DIR *dp = opendir("./logs");
    if (NULL == dp) {
        nlog_error("open logs directory fail");
        return NULL;
    }

    UT_array *log_files = NULL;
    utarray_new(log_files, &ut_str_icd);

    struct dirent *de = NULL;
    while (NULL != (de = readdir(dp))) {
        char *name = de->d_name;
        if (ends_with(de->d_name, ".log")) {
            utarray_push_back(log_files, &name);
        }
    }

    closedir(dp);

    return log_files;
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
