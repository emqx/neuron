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

#ifndef _NEU_JSON_API_PLUGIN_H_
#define _NEU_JSON_API_PLUGIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "neu_adapter.h"
#include "neu_json_parser.h"

struct neu_parse_add_plugin_req {
    enum neu_parse_function function;
    char *                  uuid;

    plugin_kind_e   kind;
    neu_node_type_e node_type;
    char *          plugin_name;
    char *          plugin_lib_name;
};

struct neu_parse_add_plugin_res {
    enum neu_parse_function function;
    char *                  uuid;
    int64_t                 error;
};

struct neu_parse_delete_plugin_req {
    enum neu_parse_function function;
    char *                  uuid;
    uint32_t                plugin_id;
};

struct neu_parse_delete_plugin_res {
    enum neu_parse_function function;
    char *                  uuid;
    int64_t                 error;
};

struct neu_parse_update_plugin_req {
    enum neu_parse_function function;
    char *                  uuid;

    plugin_kind_e   kind;
    neu_node_type_e node_type;
    char *          plugin_name;
    char *          plugin_lib_name;
};

struct neu_parse_update_plugin_res {
    enum neu_parse_function function;
    char *                  uuid;
    int64_t                 error;
};

struct neu_parse_get_plugin_req {
    enum neu_parse_function function;
    char *                  uuid;
};

struct neu_parse_get_plugin_res_libs {
    uint32_t        plugin_id;
    plugin_kind_e   kind;
    neu_node_type_e node_type;
    char *          plugin_name;
    char *          plugin_lib_name;
};

struct neu_parse_get_plugin_res {
    enum neu_parse_function               function;
    char *                                uuid;
    uint16_t                              n_plugin;
    struct neu_parse_get_plugin_res_libs *plugin_libs;
};

int neu_parse_decode_add_plugin_req(char *                            buf,
                                    struct neu_parse_add_plugin_req **req);
int neu_parse_encode_add_plugin_res(struct neu_parse_add_plugin_res *res,
                                    char **                          buf);
int neu_parse_decode_delete_plugin_req(
    char *buf, struct neu_parse_delete_plugin_req **req);
int neu_parse_encode_delete_plugin_res(struct neu_parse_delete_plugin_res *res,
                                       char **                             buf);
int neu_parse_decode_update_plugin_req(
    char *buf, struct neu_parse_update_plugin_req **req);
int neu_parse_encode_update_plugin_res(struct neu_parse_update_plugin_res *res,
                                       char **                             buf);
int neu_parse_decode_get_plugin_req(char *                            buf,
                                    struct neu_parse_get_plugin_req **req);
int neu_parse_encode_get_plugin_res(struct neu_parse_get_plugin_res *res,
                                    char **                          buf);

void neu_parse_decode_add_plugin_free(struct neu_parse_add_plugin_req *req);
void neu_parse_decode_delete_plugin_free(
    struct neu_parse_delete_plugin_req *req);
void neu_parse_decode_update_plugin_free(
    struct neu_parse_update_plugin_req *req);
void neu_parse_decode_get_plugin_free(struct neu_parse_get_plugin_req *req);

#ifdef __cplusplus
}
#endif

#endif