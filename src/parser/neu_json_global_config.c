/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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

#include "neu_json_global_config.h"

int neu_json_decode_global_config_req(char *                         buf,
                                      neu_json_global_config_req_t **result)
{
    neu_json_global_config_req_t *req = calloc(1, sizeof(*req));
    if (NULL == req) {
        return -1;
    }

    void *json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    int ret = 0;
    if (0 == neu_json_decode_get_nodes_resp_json(json_obj, &req->nodes)) {
        *result = req;
    } else {
        ret = -1;
        neu_json_decode_global_config_req_free(req);
    }

    neu_json_decode_free(json_obj);
    return ret;
}

void neu_json_decode_global_config_req_free(neu_json_global_config_req_t *req)
{
    if (req) {
        neu_json_decode_get_nodes_resp_free(req->nodes);
        free(req);
    }
}
