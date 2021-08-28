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
#include <stdlib.h>

#include "utils/json.h"

#include "neu_json_write.h"

#define GROUP "group"
#define TAGS "tags"
#define NAME "name"
#define VALUE "value"

int neu_parse_decode_write_req(char *buf, struct neu_parse_write_req **result)
{
    struct neu_parse_write_req *req =
        calloc(1, sizeof(struct neu_parse_write_req));
    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,

                               },
                               {
                                   .name = GROUP,
                                   .t    = NEU_JSON_STR,

                               }

    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function = NEU_PARSE_OP_WRITE;
    req->uuid     = elem[0].v.val_str;
    req->group    = elem[1].v.val_str;

    req->n_tag = neu_json_decode_array_size(buf, TAGS);
    req->tags  = calloc(req->n_tag, sizeof(struct neu_parse_write_req_tag));
    for (int i = 0; i < req->n_tag; i++) {
        neu_json_elem_t celem[] = { {
                                        .name = NAME,
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = VALUE,
                                        .t    = NEU_JSON_UNDEFINE,
                                    }

        };

        neu_json_decode_array(buf, TAGS, i, NEU_JSON_ELEM_SIZE(celem), celem);
        req->tags[i].name = celem[0].v.val_str;
        switch (celem[1].t) {
        case NEU_JSON_INT:
            req->tags[i].value.val_int = celem[1].v.val_int;
            break;
        case NEU_JSON_BOOL:
            req->tags[i].value.val_bool = celem[1].v.val_bool;
            break;
        case NEU_JSON_DOUBLE:
            req->tags[i].value.val_double = celem[1].v.val_double;
            break;
        case NEU_JSON_STR:
            req->tags[i].value.val_str = celem[1].v.val_str;
            break;
        default:
            break;
        }
        req->tags[i].t = celem[1].t;
    }

    *result = req;

    return 0;
}
int neu_parse_encode_write_res(struct neu_parse_write_res *res, char **buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_WRITE,
                                },
                                {
                                    .name      = NEU_PARSE_UUID,
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->uuid,
                                },
                                {
                                    .name      = NEU_PARSE_ERROR,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = res->error,
                                } };

    return neu_json_encode(elems, 3, buf);
}