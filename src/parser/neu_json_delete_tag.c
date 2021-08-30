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

#include "neu_json_delete_tag.h"
#include "utils/json.h"

#define NODE_ID "node_id"
#define TAG_ID "tag_id"

int neu_parse_decode_delete_tags_req(char *                             buf,
                                     struct neu_parse_delete_tags_req **result)
{
    struct neu_parse_delete_tags_req *req =
        calloc(1, sizeof(struct neu_parse_delete_tags_req));
    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = NODE_ID,
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = TAG_ID,
                                   .t    = NEU_JSON_INT,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function = NEU_PARSE_OP_DELETE_TAGS;
    req->uuid     = elem[0].v.val_str;
    req->node_id  = elem[1].v.val_int;
    req->tag_id   = elem[2].v.val_int;

    *result = req;

    return 0;
}

int neu_parse_encode_delete_tags_res(struct neu_parse_delete_tags_res *res,
                                     char **                           buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_DELETE_TAGS,
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