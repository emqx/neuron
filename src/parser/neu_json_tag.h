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
#ifndef _NEU_JSON_API_ADD_TAG_H_
#define _NEU_JSON_API_ADD_TAG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "neu_json_parser.h"

struct neu_parse_add_tags_req_tag {
    char *name;
    char *address;
    int   type;
    int   attribute;
};

struct neu_parse_add_tags_req {
    enum neu_parse_function            function;
    char *                             uuid;
    uint32_t                           node_id;
    char *                             group_config_name;
    int                                n_tag;
    struct neu_parse_add_tags_req_tag *tags;
};

struct neu_parse_add_tags_res {
    enum neu_parse_function function;
    char *                  uuid;
    int64_t                 error;
};

struct neu_parse_delete_tags_req {
    enum neu_parse_function function;
    char *                  uuid;
    char *                  group_config_name;
    uint32_t                node_id;
    int                     n_tag_id;
    uint32_t *              tag_ids;
};

struct neu_parse_delete_tags_res {
    enum neu_parse_function function;
    char *                  uuid;
    int64_t                 error;
};

struct neu_parse_update_tags_req_tag {
    uint32_t tag_id;
    char *   name;
    int      type;
    char *   address;
    int      attribute;
};

struct neu_parse_update_tags_req {
    enum neu_parse_function               function;
    char *                                uuid;
    uint32_t                              node_id;
    int                                   n_tag;
    struct neu_parse_update_tags_req_tag *tags;
};

struct neu_parse_update_tags_res {
    enum neu_parse_function function;
    char *                  uuid;
    int64_t                 error;
};

struct neu_parse_get_tags_res_tag {
    uint32_t id;
    char *   name;
    char *   group_config_name;
    char *   address;
    int      type;
    int      attribute;
};

struct neu_parse_get_tags_req {
    enum neu_parse_function function;
    char *                  uuid;
    uint32_t                node_id;
};

struct neu_parse_get_tags_res {
    enum neu_parse_function            function;
    char *                             uuid;
    int                                error;
    int                                n_tag;
    struct neu_parse_get_tags_res_tag *tags;
};

int  neu_parse_decode_get_tags_req(char *                          buf,
                                   struct neu_parse_get_tags_req **req);
int  neu_parse_encode_get_tags_res(struct neu_parse_get_tags_res *res,
                                   char **                        buf);
int  neu_parse_decode_update_tags_req(char *                             buf,
                                      struct neu_parse_update_tags_req **req);
int  neu_parse_encode_update_tags_res(struct neu_parse_update_tags_res *res,
                                      char **                           buf);
int  neu_parse_decode_add_tags_req(char *                          buf,
                                   struct neu_parse_add_tags_req **req);
int  neu_parse_encode_add_tags_res(struct neu_parse_add_tags_res *res,
                                   char **                        buf);
int  neu_parse_decode_delete_tags_req(char *                             buf,
                                      struct neu_parse_delete_tags_req **req);
int  neu_parse_encode_delete_tags_res(struct neu_parse_delete_tags_res *res,
                                      char **                           buf);
void neu_parse_decode_get_tags_free(struct neu_parse_get_tags_req *req);
void neu_parse_decode_update_tags_free(struct neu_parse_update_tags_req *req);
void neu_parse_decode_add_tags_free(struct neu_parse_add_tags_req *req);
void neu_parse_decode_delete_tags_free(struct neu_parse_delete_tags_req *req);
#ifdef __cplusplus
}
#endif

#endif