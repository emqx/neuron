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

char *command_tty_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt)
{
    log_info("Get tty uuid:%s", mqtt->uuid);

    UNUSED(plugin);
    neu_json_get_tty_resp_t ttys_res = { 0 };
    char *                  result   = NULL;
    ttys_res.n_tty                   = tty_file_list_get(&ttys_res.ttys);
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
