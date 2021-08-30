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

#include "neu_json_get_tag.h"
#include "utils/json.h"

#define NODE_ID "node_id"

#define TAGS "tags"
#define NAME "name"
#define TYPE "type"
#define DECIMAL "decimal"
#define ADDRESS "address"
#define FLAG "flag"

int neu_parse_decode_get_tags_req(char *                          buf,
                                  struct neu_parse_get_tags_req **result)
{
    struct neu_parse_get_tags_req *req =
        calloc(1, sizeof(struct neu_parse_get_tags_req));
    neu_json_elem_t elem[] = {
        {
            .name = NEU_PARSE_UUID,
            .t    = NEU_JSON_STR,

        },
        {
            .name = NODE_ID,
            .t    = NEU_JSON_INT,
        }
        // {
        //     .name = GROUP,
        //     .t    = NEU_JSON_STR,
        // },
        // {
        //     .name = CONFIG,
        //     .t    = NEU_JSON_STR,
        // }

    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function = NEU_PARSE_OP_GET_TAGS;
    req->uuid     = elem[0].v.val_str;
    req->node_id  = elem[1].v.val_int;
    // req->group    = elem[1].v.val_str;
    // req->config   = elem[2].v.val_str;

    *result = req;

    return 0;
}

int neu_parse_encode_get_tags_res(struct neu_parse_get_tags_res *res,
                                  char **                        buf)
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
                                      .name      = DECIMAL,
                                      .t         = NEU_JSON_INT,
                                      .v.val_int = res->tags[i].decimal,
                                  },
                                  { .name      = ADDRESS,
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->tags[i].address },
                                  {
                                      .name      = FLAG,
                                      .t         = NEU_JSON_INT,
                                      .v.val_int = res->tags[i].flag,
                                  } };
        array                 = neu_json_encode_array(array, tag, 5);
    }
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_GET_TAGS,
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
