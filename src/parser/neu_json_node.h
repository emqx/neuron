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

#ifndef _NEU_JSON_API_NODES_H_
#define _NEU_JSON_API_NODES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "neu_adapter.h"
#include "neu_json_parser.h"

struct neu_parse_add_nodes_req {
    enum neu_parse_function function;
    char *                  uuid;
    enum neu_node_type      node_type;
    char *                  adapter_name;
    char *                  plugin_name;
};

struct neu_parse_add_nodes_res {
    enum neu_parse_function function;
    char *                  uuid;
    int64_t                 error;
};

struct neu_parse_get_nodes_req {
    enum neu_parse_function function;
    char *                  uuid;
    neu_node_type_e         node_type;
};

struct neu_parse_get_nodes_res_nodes {
    uint32_t id;
    char *   name;
};

struct neu_parse_get_nodes_res {
    enum neu_parse_function               function;
    char *                                uuid;
    uint16_t                              n_node;
    struct neu_parse_get_nodes_res_nodes *nodes;
};

struct neu_parse_delete_nodes_req {
    enum neu_parse_function function;
    char *                  uuid;
    uint32_t                node_id;
}; 

struct neu_parse_delete_nodes_res {
    enum neu_parse_function function;
    char *                  uuid;
    int64_t                 error;
};

struct neu_parse_update_nodes_req {
    enum neu_parse_function function;
    char *                  uuid;
    neu_node_type_e         node_type;
    char *                  adapter_name;
    char *                  plugin_name;
};

struct neu_parse_update_nodes_res {
    enum neu_parse_function function;
    char *                  uuid;
    int64_t                 error;
};

int neu_parse_decode_add_nodes_req(char *                           buf,
                                   struct neu_parse_add_nodes_req **req);
int neu_parse_encode_add_nodes_res(struct neu_parse_add_nodes_res *res,
                                   char **                         buf);
int neu_parse_decode_get_nodes_req(char *                           buf,
                                   struct neu_parse_get_nodes_req **req);
int neu_parse_encode_get_nodes_res(struct neu_parse_get_nodes_res *res,
                                   char **                         buf);
int neu_parse_decode_delete_nodes_req(char *                              buf,
                                      struct neu_parse_delete_nodes_req **req);
int neu_parse_encode_delete_nodes_res(struct neu_parse_delete_nodes_res *res,
                                      char **                            buf);
int neu_parse_decode_update_nodes_req(char *                              buf,
                                      struct neu_parse_update_nodes_req **req);
int neu_parse_encode_update_nodes_res(struct neu_parse_update_nodes_res *res,
                                      char **                            buf);

#ifdef __cplusplus
}
#endif

#endif