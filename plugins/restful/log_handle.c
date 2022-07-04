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
#include "http.h"
#include "utils/log.h"
#include "utils/neu_jwt.h"
#include "json/neu_json_fn.h"

#include "log_handle.h"
#include "utils/utarray.h"

UT_array *collect_log_files();
void      handle_get_log_files(nng_aio *aio);
int       read_log_file(const char *log_file, void **datap, size_t *lenp);

void handle_get_log(nng_aio *aio)
{
    int  rv                               = 0;
    char log_file[NEU_NODE_NAME_LEN + 32] = { 0 };

    VALIDATE_JWT(aio);

    // optional params
    rv = http_get_param_str(aio, "log_file", log_file, sizeof(log_file));
    if (-1 == rv) {
        nlog_error("parse query param `log_file` fail");
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, error_code.error, result_error);
        });
        return;
    } else if (-2 == rv) {
        // handle no param
        handle_get_log_files(aio);
        return;
    } else if ((size_t) rv >= sizeof(log_file)) {
        nlog_error("query param overflow, `log_file`:%s", log_file);
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            http_response(aio, error_code.error, result_error);
        });
        return;
    }

    void *        data = NULL;
    size_t        len  = 0;
    nng_http_res *res  = NULL;
    rv                 = read_log_file(log_file, &data, &len);
    if (0 != rv) {
        NEU_JSON_RESPONSE_ERROR(
            rv, { http_response(aio, error_code.error, result_error); });
        return;
    }

    if (((rv = nng_http_res_alloc(&res)) != 0) ||
        ((rv = nng_http_res_set_status(res, NNG_HTTP_STATUS_OK)) != 0) ||
        ((rv = nng_http_res_set_header(res, "Content-Type",
                                       "application/octet-stream")) != 0) ||
        ((rv = nng_http_res_set_header(res, "Access-Control-Allow-Origin",
                                       "*")) != 0) ||
        ((rv = nng_http_res_set_header(res, "Access-Control-Allow-Methods",
                                       "POST,GET,PUT,DELETE,OPTIONS")) != 0) ||
        ((rv = nng_http_res_set_header(res, "Access-Control-Allow-Headers",
                                       "*")) != 0) ||
        ((rv = nng_http_res_copy_data(res, data, len)) != 0)) {
        nng_http_res_free(res);
        free(data);
        nng_aio_finish(aio, rv);
        return;
    }

    free(data);
    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
}

void handle_get_log_files(nng_aio *aio)
{
    UT_array *log_files = collect_log_files();
    if (NULL == log_files) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            http_response(aio, error_code.error, result_error);
        });
        return;
    }

    size_t len = 1 + strlen("{log_files:[  ]}");
    utarray_foreach(log_files, char **, log_file)
    {
        // one byte for the comma,
        // the last one will be for the terminating NULL byte
        len += strlen(*log_file) + 1;
    }

    char *res = malloc(len);
    if (NULL == res) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            http_response(aio, error_code.error, result_error);
        });
        utarray_free(log_files);
        return;
    }

    // with the space char to simplify code
    int i = snprintf(res, len, "{log_files:[ ");
    utarray_foreach(log_files, char **, log_file)
    {
        i += snprintf(res + i, len - i, "%s,", *log_file);
    }
    // will write over the last comma/space
    snprintf(res + i - 1, len, "]}");
    http_ok(aio, res);
    free(res);
    utarray_free(log_files);
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

// reads the entire named file, allocating storage to receive the data and
// returning the data and the size in the reference arguments.
int read_log_file(const char *log_file, void **datap, size_t *lenp)
{
    int         rv = 0;
    FILE *      f;
    struct stat st;
    size_t      len;
    void *      data;
    char        name[128] = { 0 };

    if ((size_t) snprintf(name, sizeof(name), "./logs/%s", log_file) >=
        sizeof(name)) {
        nlog_error("log_file path overflow:%s", name);
        return NEU_ERR_EINTERNAL;
    }

    if (stat(name, &st) != 0) {
        return NEU_ERR_FILE_NOT_EXIST;
    }

    if ((f = fopen(name, "rb")) == NULL) {
        nlog_error("open fail: %s", name);
        return NEU_ERR_EINTERNAL;
    }

    len = st.st_size;
    if (len > 0) {
        if ((data = malloc(len)) == NULL) {
            rv = NEU_ERR_EINTERNAL;
            goto done;
        }
        if (fread(data, 1, len, f) != len) {
            rv = errno;
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
