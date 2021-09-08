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
#ifndef _NEU_JSON_API_GROUP_CONFIG_H_
#define _NEU_JSON_API_GROUP_CONFIG_H_

#include <stdint.h>

#include "neu_json_parser.h"

struct neu_parse_get_group_config_req {
    enum neu_parse_function function;
    char *                  uuid;
    char *                  config;
    int                     node_id;
};

struct neu_parse_get_group_config_res_row {
    char *   name;
    uint32_t read_interval;
    int      pipe_count;
    int      tag_count;
};

struct neu_parse_get_group_config_res {
    enum neu_parse_function                    function;
    char *                                     uuid;
    int                                        error;
    int                                        n_config;
    struct neu_parse_get_group_config_res_row *rows;
};

struct neu_parse_add_group_config_req {
    enum neu_parse_function function;
    char *                  uuid;
    char *                  config;
    uint32_t                src_node_id;
    uint32_t                dst_node_id;
    uint32_t                read_interval;
};

struct neu_parse_update_group_config_req {
    enum neu_parse_function function;
    char *                  uuid;
    char *                  config;
    uint32_t                src_node_id;
    uint32_t                dst_node_id;
    uint32_t                read_interval;
};

struct neu_parse_delete_group_config_req {
    enum neu_parse_function function;
    char *                  uuid;
    char *                  config;
    uint32_t                node_id;
};

struct neu_parse_group_config_res {
    enum neu_parse_function function;
    char *                  uuid;
    int                     error;
};

struct neu_parse_add_tag_ids_req_row {
    char *   datatag_name;
    uint32_t datatag_id;
};

struct neu_parse_add_tag_ids_req {
    enum neu_parse_function               function;
    char *                                uuid;
    char *                                config;
    int                                   node_id;
    int                                   n_id;
    struct neu_parse_add_tag_ids_req_row *rows;
};

struct neu_parse_add_tag_ids_res {
    enum neu_parse_function function;
    char *                  uuid;
    int                     error;
};

struct neu_parse_delete_tag_ids_req_row {
    char *   datatag_name;
    uint32_t datatag_id;
};

struct neu_parse_delete_tag_ids_req {
    enum neu_parse_function                  function;
    char *                                   uuid;
    char *                                   config;
    int                                      node_id;
    int                                      n_id;
    struct neu_parse_delete_tag_ids_req_row *rows;
};

struct neu_parse_delete_tag_ids_res {
    enum neu_parse_function function;
    char *                  uuid;
    int                     error;
};

int neu_parse_decode_get_group_config_req(
    char *buf, struct neu_parse_get_group_config_req **result);
int neu_parse_decode_add_group_config_req(
    char *buf, struct neu_parse_add_group_config_req **result);
int neu_parse_decode_update_group_config_req(
    char *buf, struct neu_parse_update_group_config_req **result);
int neu_parse_decode_delete_group_config_req(
    char *buf, struct neu_parse_delete_group_config_req **result);

int neu_parse_encode_get_group_config_res(
    struct neu_parse_get_group_config_res *res, char **buf);
int neu_parse_encode_add_group_config_res(
    struct neu_parse_group_config_res *res, char **buf);
int neu_parse_encode_update_group_config_res(
    struct neu_parse_group_config_res *res, char **buf);
int neu_parse_encode_delete_group_config_res(
    struct neu_parse_group_config_res *res, char **buf);

void neu_parse_encode_get_group_config_free(
    struct neu_parse_get_group_config_req *req);
void neu_parse_decode_add_group_config_free(
    struct neu_parse_add_group_config_req *req);
void neu_parse_decode_update_group_config_free(
    struct neu_parse_update_group_config_req *req);
void neu_parse_decode_delete_group_config_free(
    struct neu_parse_delete_group_config_req *req);

int neu_parse_decode_add_tag_ids_req(char *                             buf,
                                     struct neu_parse_add_tag_ids_req **result);
int neu_parse_decode_delete_tag_ids_req(
    char *buf, struct neu_parse_delete_tag_ids_req **result);

int neu_parse_encode_add_tag_ids_res(struct neu_parse_add_tag_ids_res *res,
                                     char **                           buf);
int neu_parse_encode_delete_tag_ids_res(
    struct neu_parse_delete_tag_ids_res *res, char **buf);

void neu_parse_decode_add_tag_ids_free(struct neu_parse_add_tag_ids_req *req);
void neu_parse_decode_delete_tag_ids_free(
    struct neu_parse_delete_tag_ids_req *req);

#endif