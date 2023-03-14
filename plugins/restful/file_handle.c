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
#include <time.h>
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
#include "parser/neu_json_file.h"

void handle_file_info(nng_aio *aio)
{
    char dir_path[128] = { 0 };

    int            n_file      = 0;
    struct dirent *files       = { 0 };
    char *         f_name[256] = { 0 };
    char           c_time[32]  = { 0 };
    char           m_time[32]  = { 0 };

    neu_json_get_file_list_resp_t file_resp = { 0 };
    char *                        result    = NULL;

    NEU_VALIDATE_JWT(aio);

    if (neu_http_get_param_str(aio, "dir_path", dir_path, sizeof(dir_path)) <=
        0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        });

        return;
    }

    /*check whether the directory exists*/
    if (0 != access(dir_path, F_OK)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_NOT_EXIST, {
            neu_http_response(aio, error_code.error, result_error);
        });

        return;
    }

    /*open directory*/
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        nlog_error("open a directory failed");
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_OPEN_FAILURE, {
            neu_http_response(aio, error_code.error, result_error);
        });

        return;
    }

    /*read directory*/
    while ((files = readdir(dir)) != NULL) {
        if (strcmp(files->d_name, ".") == 0 ||
            strcmp(files->d_name, "..") == 0) {
            continue;
        }

        f_name[n_file] = files->d_name;
        ++n_file;
    }

    /*handle response*/
    file_resp.n_file = n_file;
    file_resp.file = calloc(file_resp.n_file, sizeof(neu_json_get_file_resp_t));

    for (int i = 0; i < file_resp.n_file; i++) {
        char        file_path[512] = { 0 };
        struct stat statbuf;

        sprintf(file_path, "%s/%s", dir_path, f_name[i]);
        stat(file_path, &statbuf);

        strncpy(c_time, ctime(&statbuf.st_ctim.tv_sec),
                strlen(ctime(&statbuf.st_ctim.tv_sec)) - 1);
        strncpy(m_time, ctime(&statbuf.st_mtim.tv_sec),
                strlen(ctime(&statbuf.st_mtim.tv_sec)) - 1);

        file_resp.file[i].name  = f_name[i];
        file_resp.file[i].size  = statbuf.st_size;
        file_resp.file[i].ctime = c_time;
        file_resp.file[i].mtime = m_time;
    }

    neu_json_encode_by_fn(&file_resp, neu_json_encode_get_file_list_resp,
                          &result);

    closedir(dir);
    neu_http_ok(aio, result);
    free(result);
    free(file_resp.file);

    return;
}

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
    free(file_data);
    nlog_notice("download file %s, rv: %d", file_name, rv);
}