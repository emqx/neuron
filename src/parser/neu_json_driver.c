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
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "json/json.h"

#include "json/neu_json_driver.h"

int neu_json_decode_driver_cmd_req_json(void *                 json_obj,
                                        neu_json_driver_cmd_t *req)
{
    int ret = 0;

    neu_json_elem_t req_elems[] = {
        {
            .name = "driver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "cmd",
            .t    = NEU_JSON_STR,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        return -1;
    }

    req->driver = req_elems[0].v.val_str;
    req->cmd    = req_elems[1].v.val_str;

    return 0;
}

int neu_json_decode_driver_cmd_req(char *buf, neu_json_driver_cmd_t **result)
{
    int                    ret      = 0;
    void *                 json_obj = NULL;
    neu_json_driver_cmd_t *req      = calloc(1, sizeof(neu_json_driver_cmd_t));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    ret = neu_json_decode_driver_cmd_req_json(json_obj, req);
    if (0 == ret) {
        *result = req;
    } else {
        free(req);
    }

    neu_json_decode_free(json_obj);
    return ret;

    return 0;
}

void neu_json_decode_driver_cmd_req_free(neu_json_driver_cmd_t *req)
{
    if (req) {
        free(req->driver);
        free(req->cmd);
        free(req);
    }
}