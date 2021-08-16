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

#include "neu_json_login.h"

#define USER "user"
#define PASSWORD "password"
#define TOKEN "token"

int neu_parse_decode_login_req(char *buf, struct neu_parse_login_req **result)
{
    struct neu_parse_login_req *req =
        calloc(1, sizeof(struct neu_parse_login_req));
    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,

                               },
                               {
                                   .name = USER,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = PASSWORD,
                                   .t    = NEU_JSON_STR,
                               }

    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->function = NEU_PARSE_OP_LOGIN;
    req->uuid     = elem[0].v.val_str;
    req->user     = elem[1].v.val_str;
    req->password = elem[2].v.val_str;

    *result = req;

    return 0;
}

int neu_parse_encode_login_res(struct neu_parse_login_res *res, char **buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_LOGIN,

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
                                    .name      = TOKEN,
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->token,
                                }

    };
    return neu_json_encode(elems, NEU_JSON_ELEM_SIZE(elems), buf);
}

int neu_parse_decode_logout_req(char *buf, struct neu_parse_logout_req **result)
{
    (void) buf;
    (void) result;
    return 0;
}

int neu_parse_encode_logout_res(struct neu_parse_logout_res *res, char **buf)
{
    (void) res;
    (void) buf;
    return 0;
}