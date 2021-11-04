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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "json/neu_json_fn.h"
#include "json/neu_json_tty.h"

#include "handle.h"
#include "http.h"

#include "normal_handle.h"

#define DEV_TTY_LENTH 261

static int get_tty_file_list(char ***tty_file)
{
    DIR *          dir    = NULL;
    struct dirent *ptr    = NULL;
    int            n_file = 0;

    *tty_file = NULL;
    dir       = opendir("/dev");
    if (dir == NULL) {
        log_error("Open dir error: %s", strerror(errno));
        return -1;
    }
    ptr = readdir(dir);
    while (ptr != NULL) {
        if (ptr->d_type == 2) {
            char dev_tty[DEV_TTY_LENTH] = { 0 };
            if (strstr(ptr->d_name, "tty") != NULL) {
                n_file += 1;
                *tty_file = realloc(*tty_file, sizeof(char *) * n_file);
                snprintf(dev_tty, sizeof(dev_tty), "/dev/%s", ptr->d_name);
                (*tty_file)[n_file - 1] = strdup(dev_tty);
            }
        }
        ptr = readdir(dir);
    }

    closedir(dir);

    return n_file;
}

void handle_get_ttys(nng_aio *aio)
{
    neu_parse_get_tty_res_t ttys_res = { 0 };

    ttys_res.n_tty = get_tty_file_list(&ttys_res.ttys);
    if (ttys_res.n_tty == -1) {
        http_bad_request(aio, "{\"error\": 400}");
        return;
    } else {
        char *result = NULL;

        neu_json_encode_by_fn(&ttys_res, neu_parse_encode_get_tty, &result);
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
    http_ok(aio, "{\"status\": \"OK\"}");
}

void handle_login(nng_aio *aio)
{
    (void) aio;
}

void handle_logout(nng_aio *aio)
{
    (void) aio;
}

void handle_get_plugin_schema(nng_aio *aio)
{
    char  buf[4096] = { 0 };
    FILE *fp        = fopen("./plugin_param_schema.json", "r");

    if (fp == NULL) {
        log_info("open ./plugin_param_schema.json error: %d", errno);
        http_not_found(aio, "{\"status\": \"error\"}");
        return;
    }

    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    http_ok(aio, buf);
}