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

#include "neu_json_read.h"

#define GROUP "group"
#define NAMES "names"
#define NAME "name"
#define TYPE "type"
#define TIMESTAMP "timestamp"
#define VALUE "value"
#define TAGS "tags"

int neu_parse_decode_read_req(char *buf, struct neu_parse_read_req **result)
{
    struct neu_parse_read_req *req =
        calloc(1, sizeof(struct neu_parse_read_req));
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
    req->function = NEU_PARSE_OP_READ;
    req->uuid     = elem[0].v.val_str;
    req->group    = elem[1].v.val_str;

    req->n_name = neu_json_decode_array_size(buf, NAMES);
    req->names  = calloc(req->n_name, sizeof(struct neu_parse_read_req_name));
    for (int i = 0; i < req->n_name; i++) {
        neu_json_elem_t name = {
            .name = NULL,
            .t    = NEU_JSON_STR,
        };
        neu_json_decode_array(buf, NAMES, i, 1, &name);
        req->names[i].name = name.v.val_str;
    }

    *result = req;

    return 0;
}
int neu_parse_encode_read_res(struct neu_parse_read_res *res, char **buf)
{
    void *array = NULL;
    for (int i = 0; i < res->n_tag; i++) {
        neu_json_elem_t tag[] = { {
                                      .name      = NAME,
                                      .t         = NEU_JSON_STR,
                                      .v.val_str = res->tags[i].name,
                                  },
                                  {
                                      .name      = TYPE,
                                      .t         = NEU_JSON_INT,
                                      .v.val_int = res->tags[i].type,

                                  },
                                  {
                                      .name      = TIMESTAMP,
                                      .t         = NEU_JSON_INT,
                                      .v.val_int = res->tags[i].timestamp,
                                  },
                                  {
                                      .name         = VALUE,
                                      .t            = NEU_JSON_DOUBLE,
                                      .v.val_double = res->tags[i].value,
                                  } };
        array                 = neu_json_encode_array(array, tag, 4);
    }
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_READ,
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
                                },
                                {
                                    .name     = TAGS,
                                    .t        = NEU_JSON_OBJECT,
                                    .v.object = array,
                                } };

    return neu_json_encode(elems, 4, buf);
}