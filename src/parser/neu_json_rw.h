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
#ifndef _NEU_JSON_API_READ_H_
#define _NEU_JSON_API_READ_H_

#include <stdint.h>

#include "utils/json.h"

#include "neu_json_parser.h"

struct neu_parse_read_req_name {
    char *name;
};

struct neu_parse_read_req {
    enum neu_parse_function         function;
    char *                          uuid;
    int                             n_group;
    struct neu_parse_read_req_name *group_names;
};

struct neu_parse_read_res_tag {
    char *  name;
    uint8_t type;
    int64_t timestamp;
    double  value;
};
struct neu_parse_read_res {
    enum neu_parse_function        function;
    char *                         uuid;
    int64_t                        error;
    int                            n_tag;
    struct neu_parse_read_res_tag *tags;
};

struct neu_parse_write_req_tag {
    uint32_t             tag_id;
    enum neu_json_type   t;
    union neu_json_value value;
};

struct neu_parse_write_req {
    enum neu_parse_function         function;
    char *                          uuid;
    uint32_t                        node_id;
    int                             n_tag;
    struct neu_parse_write_req_tag *tags;
};

struct neu_parse_write_res {
    enum neu_parse_function function;
    char *                  uuid;
    int64_t                 error;
};

int  neu_parse_decode_write_req(char *buf, struct neu_parse_write_req **req);
int  neu_parse_encode_write_res(struct neu_parse_write_res *res, char **buf);
void neu_parse_decode_write_free(struct neu_parse_write_req *req);

int  neu_parse_decode_read_req(char *buf, struct neu_parse_read_req **req);
int  neu_parse_encode_read_res(struct neu_parse_read_res *res, char **buf);
void neu_parse_decode_read_free(struct neu_parse_read_req *req);

#endif