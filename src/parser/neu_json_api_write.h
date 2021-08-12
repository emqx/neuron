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
#ifndef _NEU_JSON_API_WRITE_H_
#define _NEU_JSON_API_WRITE_H_

#include "neu_json_api_parser.h"

struct neu_parse_write_req_tag {
    char * name;
    double db;
};

struct neu_parse_write_req {
    struct neu_parse_op             op;
    char *                          uuid;
    char *                          group;
    int                             n_tag;
    struct neu_parse_write_req_tag *tags;
};

struct neu_parse_write_res {
    struct neu_parse_op op;
    char *              uuid;
    int                 error;
};

int neu_json_decode_write_req(char *buf, struct neu_parse_write_req **req);
int neu_json_encode_write_res(struct neu_parse_write_res *res, char **buf);
#endif