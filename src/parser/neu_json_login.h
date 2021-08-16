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
#ifndef _NEU_JSON_API_LOGIN_H_
#define _NEU_JSON_API_LOGIN_H_

#include <stdint.h>

#include "neu_json_parser.h"

struct neu_parse_login_req {
    enum neu_parse_function function;
    char *                  uuid;
    char *                  user;
    char *                  password;
};

struct neu_parse_login_res {
    enum neu_parse_function function;
    char *                  uuid;
    int                     error;
    char *                  token;
};

struct neu_parse_logout_req {
    enum neu_parse_function function;
    char *                  uuid;
    char *                  user;
};

struct neu_parse_logout_res {
    enum neu_parse_function function;
    char *                  uuid;
    int                     error;
};

int neu_parse_decode_login_req(char *buf, struct neu_parse_login_req **req);
int neu_parse_encode_login_res(struct neu_parse_login_res *res, char **buf);
int neu_parse_decode_logout_req(char *buf, struct neu_parse_logout_req **req);
int neu_parse_encode_logout_res(struct neu_parse_logout_res *res, char **buf);

#endif