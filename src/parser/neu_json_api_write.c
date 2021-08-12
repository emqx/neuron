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

#include "neu_json_api_write.h"

int neu_json_decode_write_req(char *buf, struct neu_parse_write_req **result)
{
    struct neu_parse_write_req *req =
        calloc(1, sizeof(struct neu_parse_write_req));

    neu_json_elem_t uuid = {
        .name = "uuid",
        .t    = NEU_JSON_STR,
    };
    neu_json_elem_t group = {
        .name = "group",
        .t    = NEU_JSON_STR,
    };
    neu_json_elem_t tags = {
        .name = "tags",
        .t    = NEU_JSON_OBJECT,
    };

    int ret = neu_json_decode(buf, 3, &uuid, &group, &tags);
    if (ret != 0) {
        return -1;
    }
    req->op.function = NEU_PARSE_OP_WRITE;
    req->uuid        = uuid.v.val_str;
    req->group       = group.v.val_str;

    req->n_tag = neu_json_decode_array_size(buf, "tags");
    req->tags  = calloc(req->n_tag, sizeof(struct neu_parse_write_req_tag));
    for (int i = 0; i < req->n_tag; i++) {
        neu_json_elem_t name = {
            .name = "name",
            .t    = NEU_JSON_STR,
        };
        neu_json_elem_t value = {
            .name = "value",
            .t    = NEU_JSON_UNDEFINE,
        };

        neu_json_decode_array(tags.v.object, i, 2, &name, &value);
        req->tags[i].name = name.v.val_str;
        req->tags[i].db   = value.v.val_double;
    }

    *result = req;

    return 0;
}
int neu_json_encode_write_res(struct neu_parse_write_res *res, char **buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = "function",
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_READ,
                                },
                                {
                                    .name      = "uuid",
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->uuid,
                                },
                                {
                                    .name      = "error",
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = res->error,
                                } };

    return neu_json_encode(elems, 3, buf);
}