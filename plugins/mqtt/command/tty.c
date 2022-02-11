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
#include <stdlib.h>

#include "tty.h"

// Copy from normal_handle.c

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

char *command_tty_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt)
{
    log_info("Get tty uuid:%s", mqtt->uuid);

    UNUSED(plugin);
    neu_json_get_tty_resp_t ttys_res = { 0 };
    char *                  result   = NULL;
    ttys_res.n_tty                   = get_tty_file_list(&ttys_res.ttys);
    if (ttys_res.n_tty == -1) {
        neu_json_error_resp_t error = { 0 };
        error.error                 = NEU_ERR_FAILURE;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
    } else {
        neu_json_encode_with_mqtt(&ttys_res, neu_json_encode_get_tty_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);

        for (int i = 0; i < ttys_res.n_tty; i++) {
            free(ttys_res.ttys[i]);
        }

        free(result);
        free(ttys_res.ttys);
    }

    return result;
}
