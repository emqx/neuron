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
#include <stdio.h>
#include <stdlib.h>

#include "json/json.h"

#include "json/neu_json_mqtt.h"

int neu_parse_decode_mqtt_param(char *buf, neu_parse_mqtt_t **req)
{
    neu_parse_mqtt_t *result = calloc(1, sizeof(neu_parse_mqtt_t));

    neu_json_elem_t elem[] = { {
                                   .name = "function",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "uuid",
                                   .t    = NEU_JSON_STR,
                               } };
    int             ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(result);
        return -1;
    }

    result->function = elem[0].v.val_int;
    result->uuid     = elem[1].v.val_str;

    *req = result;
    return 0;
}

void neu_parse_decode_mqtt_param_free(neu_parse_mqtt_t *req)
{
    free(req->uuid);
    free(req);
}

int neu_parse_encode_mqtt_param(void *json_object, void *param)
{
    neu_parse_mqtt_t *res = (neu_parse_mqtt_t *) param;

    neu_json_elem_t elems[] = {
        {
            .name      = "function",
            .t         = NEU_JSON_INT,
            .v.val_int = res->function,
        },
        {
            .name      = "uuid",
            .t         = NEU_JSON_STR,
            .v.val_str = res->uuid,
        },
    };

    return neu_json_encode_field(json_object, elems, 2);
}