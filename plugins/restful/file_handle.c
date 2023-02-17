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
#include "handle.h"
#include "utils/http.h"

#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "file_handle.h"
#include "log_handle.h"

void handle_download_file(nng_aio *aio)
{
    char file_name[NEU_FILE_PATH_LEN] = { 0 };
    char file_path[256]               = { 0 };
    char http_resp[256]               = { 0 };

    int    rv        = 0;
    void * file_data = NULL;
    size_t file_len  = 0;
    char * p         = NULL;

    NEU_VALIDATE_JWT(aio);

    if (neu_http_get_param_str(aio, "file_path", file_path,
                               sizeof(file_path)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        });

        return;
    }

    /*check whether the file exists*/
    rv = access(file_path, F_OK);
    if (0 != rv) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_NOT_EXIST, {
            neu_http_response(aio, error_code.error, result_error);
        });

        return;
    }

    /*read file content*/
    rv = read_file(file_path, &file_data, &file_len);
    if (0 != rv) {
        NEU_JSON_RESPONSE_ERROR(
            rv, { neu_http_response(aio, error_code.error, result_error); });

        return;
    }

    /*extract file name*/
    p = strchr(file_path, '/');
    while (strchr(p, '/') != NULL) {
        p = strchr(p, '/') + 1;
    }

    if (p != NULL) {
        strcpy(file_name, p);
    }

    /*handle http response*/
    sprintf(http_resp, "%s%s", "attachment; filename=", file_name);
    rv = neu_http_response_file(aio, file_data, file_len, http_resp);
    if (0 != rv) {
        return;
    }
}