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

#ifndef _NEU_JSON_API_DRIVER_CMD_H_
#define _NEU_JSON_API_DRIVER_CMD_H_

#include "define.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *driver;
    char *action;
} neu_json_driver_action_t;

int  neu_json_decode_driver_action_req(char *                     buf,
                                       neu_json_driver_action_t **result);
void neu_json_decode_driver_action_req_free(neu_json_driver_action_t *req);

#ifdef __cplusplus
}
#endif

#endif