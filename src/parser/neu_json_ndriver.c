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

#include "neu_json_ndriver.h"

int neu_json_encode_ndriver_map(void *json_object, void *param)
{
    int                     ret = 0;
    neu_json_ndriver_map_t *req = param;

    neu_json_elem_t req_elems[] = {
        {
            .name      = "ndriver",
            .t         = NEU_JSON_STR,
            .v.val_str = req->ndriver,
        },
        {
            .name      = "driver",
            .t         = NEU_JSON_STR,
            .v.val_str = req->driver,
        },
        {
            .name      = "group",
            .t         = NEU_JSON_STR,
            .v.val_str = req->group,
        },
    };

    ret = neu_json_encode_field(json_object, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));

    return ret;
}

void neu_json_decode_ndriver_map_free(neu_json_ndriver_map_t *req);

int neu_json_decode_ndriver_map(char *buf, neu_json_ndriver_map_t **result)
{
    int                     ret      = 0;
    void *                  json_obj = NULL;
    neu_json_ndriver_map_t *req      = NULL;

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        return -1;
    }

    req = calloc(1, sizeof(*req));
    if (req == NULL) {
        goto error;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "ndriver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "driver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group",
            .t    = NEU_JSON_STR,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    req->ndriver = req_elems[0].v.val_str;
    req->driver  = req_elems[1].v.val_str;
    req->group   = req_elems[2].v.val_str;

    if (ret != 0) {
        goto error;
    }

    *result = req;
    neu_json_decode_free(json_obj);
    return 0;

error:
    neu_json_decode_ndriver_map_free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

void neu_json_decode_ndriver_map_free(neu_json_ndriver_map_t *req)
{
    if (req) {
        free(req->ndriver);
        free(req->driver);
        free(req->group);
        free(req);
    }
}
