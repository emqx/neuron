/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef _NEU_JSON_API_NEU_JSON_NDRIVER_H_
#define _NEU_JSON_API_NEU_JSON_NDRIVER_H_

#include "json/json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *ndriver;
    char *driver;
    char *group;
} neu_json_ndriver_map_t;

int  neu_json_encode_ndriver_map(void *json_object, void *param);
int  neu_json_decode_ndriver_map(char *buf, neu_json_ndriver_map_t **result);
void neu_json_decode_ndriver_map_free(neu_json_ndriver_map_t *req);

typedef struct {
    char *driver;
    char *group;
} neu_json_ndriver_map_group_t;

typedef struct {
    int                           n_group;
    neu_json_ndriver_map_group_t *groups;
} neu_json_ndriver_map_group_array_t;

int neu_json_encode_ndriver_group_array(void *json_obj, void *param);

int neu_json_encode_get_ndriver_maps_resp(void *json_object, void *param);

typedef struct {
    char *  name;
    char *  address;
    int64_t attribute;
    int64_t type;
    char *  params;
} neu_json_ndriver_tag_t;

int neu_json_encode_ndriver_tag(void *json_obj, void *param);

typedef struct {
    int                     len;
    neu_json_ndriver_tag_t *data;
} neu_json_ndriver_tag_array_t;

int neu_json_encode_ndriver_tag_array(void *json_obj, void *param);

int neu_json_encode_get_ndriver_tags_resp(void *json_obj, void *param);

typedef struct {
    char *name;
    char *params;
} neu_json_ndriver_tag_param_t;

void neu_json_ndriver_tag_param_fini(neu_json_ndriver_tag_param_t *tag_param);
int  neu_json_decode_ndriver_tag_param_json(
     void *json_obj, neu_json_ndriver_tag_param_t *tag_param_p);

typedef struct {
    int                           len;
    neu_json_ndriver_tag_param_t *data;
} neu_json_ndriver_tag_param_array_t;

void neu_json_ndriver_tag_param_array_fini(
    neu_json_ndriver_tag_param_array_t *arr);
int neu_json_decode_ndriver_tag_param_array_json(
    void *json_obj, neu_json_ndriver_tag_param_array_t *arr);

typedef struct {
    char *                             ndriver;
    char *                             driver;
    char *                             group;
    neu_json_ndriver_tag_param_array_t tags;
} neu_json_update_ndriver_tag_param_req_t;

int neu_json_decode_update_ndriver_tag_param_req(
    char *buf, neu_json_update_ndriver_tag_param_req_t **result);
void neu_json_decode_update_ndriver_tag_param_req_free(
    neu_json_update_ndriver_tag_param_req_t *req);

typedef struct {
    char *name;
    char *address;
} neu_json_ndriver_tag_info_t;

void neu_json_ndriver_tag_info_fini(neu_json_ndriver_tag_info_t *tag_info);
int  neu_json_decode_ndriver_tag_info_json(
     void *json_obj, neu_json_ndriver_tag_info_t *tag_info_p);

typedef struct {
    int                          len;
    neu_json_ndriver_tag_info_t *data;
} neu_json_ndriver_tag_info_array_t;

void neu_json_ndriver_tag_info_array_fini(
    neu_json_ndriver_tag_info_array_t *arr);
int neu_json_decode_ndriver_tag_info_array_json(
    void *json_obj, neu_json_ndriver_tag_info_array_t *arr);

typedef struct {
    char *                            ndriver;
    char *                            driver;
    char *                            group;
    neu_json_ndriver_tag_info_array_t tags;
} neu_json_update_ndriver_tag_info_req_t;

int neu_json_decode_update_ndriver_tag_info_req(
    char *buf, neu_json_update_ndriver_tag_info_req_t **result);
void neu_json_decode_update_ndriver_tag_info_req_free(
    neu_json_update_ndriver_tag_info_req_t *req);

#ifdef __cplusplus
}
#endif

#endif
