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
#include <string.h>

#include "neu_log.h"
#include "utils/json.h"

#include "neu_json_api_parser.h"
#include "neu_json_api_read.h"
#include "neu_json_api_write.h"

int neu_parse_decode(char *buf, void **result)
{
    neu_json_elem_t func_code = {
        .name = "function",
        .t    = NEU_JSON_INT,
    };
    int ret = neu_json_decode(buf, 1, &func_code);
    if (ret != 0) {
        return -1;
    }

    switch (func_code.v.val_int) {
    case NEU_PARSE_OP_READ:
        ret = neu_json_decode_read_req(buf,
                                       (struct neu_parse_read_req **) result);
        break;
    case NEU_PARSE_OP_WRITE:
        ret = neu_json_decode_write_req(buf,
                                        (struct neu_parse_write_req **) result);
        break;
    }

    return 0;
}

int neu_parse_encode(void *result, char **buf)
{
    struct neu_parse_op *op = result;

    switch (op->function) {
    case NEU_PARSE_OP_READ:
        neu_json_encode_read_res((struct neu_parse_read_res *) result, buf);
        break;
    case NEU_PARSE_OP_WRITE:
        neu_json_encode_write_res((struct neu_parse_write_res *) result, buf);
        break;
    }

    return 0;
}

void neu_parse_decode_free(void *result)
{
    struct neu_parse_op *op = result;

    switch (op->function) {
    case NEU_PARSE_OP_READ: {
        struct neu_parse_read_req *req = result;

        free(req->uuid);
        free(req->group);
        for (int i = 0; i < req->n_name; i++) {
            free(req->names[i].name);
        }
        free(req->names);

        break;
    }
    case NEU_PARSE_OP_WRITE: {
        struct neu_parse_write_req *req = result;

        free(req->uuid);
        free(req->group);
        for (int i = 0; i < req->n_tag; i++) {
            free(req->tags[i].name);
        }
        break;
    }
    }
}