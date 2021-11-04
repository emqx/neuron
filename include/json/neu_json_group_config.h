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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    char *name;
    int   node_id;
} neu_parse_get_group_config_req_t;

typedef struct {
    char *   name;
    uint32_t interval;
    int      pipe_count;
    int      tag_count;
} neu_parse_get_group_config_res_row_t;

typedef struct {
    int                                   n_config;
    neu_parse_get_group_config_res_row_t *rows;
} neu_parse_get_group_config_res_t;

int neu_parse_decode_get_group_config(
    char *buf, neu_parse_get_group_config_req_t **result);
void neu_parse_decode_get_group_config_free(
    neu_parse_get_group_config_req_t *req);
int neu_parse_encode_get_group_config(void *json_object, void *param);

typedef struct {
    char *   name;
    uint32_t node_id;
    uint32_t interval;
} neu_parse_add_group_config_req_t;

int neu_parse_decode_add_group_config(
    char *buf, neu_parse_add_group_config_req_t **result);
void neu_parse_decode_add_group_config_free(
    neu_parse_add_group_config_req_t *req);

typedef struct {
    char *   name;
    uint32_t node_id;
    uint32_t interval;
} neu_parse_update_group_config_req_t;

int neu_parse_decode_update_group_config(
    char *buf, neu_parse_update_group_config_req_t **result);
void neu_parse_decode_update_group_config_free(
    neu_parse_update_group_config_req_t *req);

typedef struct {
    char *   name;
    uint32_t node_id;
} neu_parse_del_group_config_req_t;

int neu_parse_decode_del_group_config(
    char *buf, neu_parse_del_group_config_req_t **result);
void neu_parse_decode_del_group_config_free(
    neu_parse_del_group_config_req_t *req);

typedef struct {
    uint32_t dst_node_id;
    uint32_t src_node_id;
    char *   name;
} neu_parse_subscribe_req_t, neu_parse_unsubscribe_req_t;

int  neu_parse_decode_subscribe(char *buf, neu_parse_subscribe_req_t **result);
void neu_parse_decode_subscribe_free(neu_parse_subscribe_req_t *req);

int  neu_parse_decode_unsubscribe(char *                        buf,
                                  neu_parse_unsubscribe_req_t **result);
void neu_parse_decode_unsubscribe_free(neu_parse_unsubscribe_req_t *req);

typedef struct {
    uint32_t node_id;
    char *   group_config_name;
} neu_parse_subscribe_res_grp_t;

typedef struct {
    int                            n_config;
    neu_parse_subscribe_res_grp_t *group_configs;
} neu_parse_get_subscribe_res_t;

int neu_parse_encode_get_subscribe(void *json_object, void *param);

#ifdef __cplusplus
}
#endif

#endif