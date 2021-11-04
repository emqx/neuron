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

typedef struct {
    char *name;
    char *address;
    int   type;
    int   attribute;
} neu_parse_add_tags_req_tag_t;

typedef struct {
    uint32_t                      node_id;
    char *                        group_config_name;
    int                           n_tag;
    neu_parse_add_tags_req_tag_t *tags;
} neu_parse_add_tags_req_t;

int  neu_parse_decode_add_tags(char *buf, neu_parse_add_tags_req_t **req);
void neu_parse_decode_add_tags_free(neu_parse_add_tags_req_t *req);

typedef struct {
    char *    group_config_name;
    uint32_t  node_id;
    int       n_tag_id;
    uint32_t *tag_ids;
} neu_parse_del_tags_req_t;

int  neu_parse_decode_del_tags(char *buf, neu_parse_del_tags_req_t **req);
void neu_parse_decode_del_tags_free(neu_parse_del_tags_req_t *req);

typedef struct {
    uint32_t tag_id;
    char *   name;
    int      type;
    char *   address;
    int      attribute;
} neu_parse_update_tags_req_tag_t;

typedef struct {
    uint32_t                         node_id;
    int                              n_tag;
    neu_parse_update_tags_req_tag_t *tags;
} neu_parse_update_tags_req_t;

int  neu_parse_decode_update_tags(char *buf, neu_parse_update_tags_req_t **req);
void neu_parse_decode_update_tags_free(neu_parse_update_tags_req_t *req);

typedef struct {
    uint32_t id;
    char *   name;
    char *   group_config_name;
    char *   address;
    int      type;
    int      attribute;
} neu_parse_get_tags_res_tag_t;

typedef struct {
    uint32_t node_id;
} neu_parse_get_tags_req_t;

typedef struct {
    int                           n_tag;
    neu_parse_get_tags_res_tag_t *tags;
} neu_parse_get_tags_res_t;

int  neu_parse_decode_get_tags(char *buf, neu_parse_get_tags_req_t **req);
void neu_parse_decode_get_tags_free(neu_parse_get_tags_req_t *req);
int  neu_parse_encode_get_tags(void *json_object, void *param);

#ifdef __cplusplus
}
#endif

#endif