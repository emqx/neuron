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

#ifndef _NEU_JSON_API_NEU_JSON_TEMPLATE_H_
#define _NEU_JSON_API_NEU_JSON_TEMPLATE_H_

#include "json/json.h"

#include "neu_json_tag.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *               name;
    int64_t              interval;
    neu_json_tag_array_t tags;
} neu_json_template_group_t;

int  neu_json_encode_template_group(void *json_obj, void *param);
int  neu_json_decode_template_group(void *                     json_obj,
                                    neu_json_template_group_t *grp);
void neu_json_decode_template_group_fini(neu_json_template_group_t *grp);

typedef struct {
    int                        len;
    neu_json_template_group_t *groups;
} neu_json_template_group_array_t;

int neu_json_encode_template_group_array(void *json_obj, void *param);
int neu_json_decode_template_group_array_json(
    void *json_obj, neu_json_template_group_array_t *arr);
void neu_json_decode_template_group_array_fini(
    neu_json_template_group_array_t *arr);

typedef struct {
    char *                          name;
    char *                          plugin;
    neu_json_template_group_array_t groups;
} neu_json_template_t;

int  neu_json_encode_template(void *json_obj, void *param);
int  neu_json_decode_template(char *buf, neu_json_template_t **result);
int  neu_json_decode_template_json(void *json_obj, neu_json_template_t *tmpl);
void neu_json_decode_template_fini(neu_json_template_t *tmpl);
void neu_json_decode_template_free(neu_json_template_t *tmpl);

typedef struct {
    char *name;
    char *plugin;
} neu_json_template_info_t;

typedef struct {
    int                       len;
    neu_json_template_info_t *info;
} neu_json_template_info_array_t;

int neu_json_encode_template_info_array(void *json_obj, void *param);

// param should be a neu_json_template_info_array_t
int neu_json_encode_get_templates_resp(void *json_obj, void *param);

typedef struct {
    char *  tmpl;
    char *  group;
    int64_t interval;
} neu_json_template_add_group_req_t;

int neu_json_encode_template_add_group_req(void *json_obj, void *param);
int neu_json_decode_template_add_group_req(
    char *buf, neu_json_template_add_group_req_t **result);
void neu_json_decode_template_add_group_req_free(
    neu_json_template_add_group_req_t *req);

typedef struct {
    char *  tmpl;
    char *  group;
    char *  new_name;
    bool    set_interval;
    int64_t interval;
} neu_json_template_update_group_req_t;

int neu_json_decode_template_update_group_req(
    char *buf, neu_json_template_update_group_req_t **result);
void neu_json_decode_template_update_group_req_free(
    neu_json_template_update_group_req_t *req);

typedef struct {
    char *tmpl;
    char *group;
} neu_json_template_del_group_req_t;

int neu_json_encode_template_del_group_req(void *json_object, void *param);
int neu_json_decode_template_del_group_req(
    char *buf, neu_json_template_del_group_req_t **result);
void neu_json_decode_template_del_group_req_free(
    neu_json_template_del_group_req_t *req);

typedef struct {
    char *          tmpl;
    char *          group;
    int             n_tag;
    neu_json_tag_t *tags;
} neu_json_template_mod_tags_req_t;

int neu_json_encode_template_mod_tags_req(void *json_obj, void *param);
int neu_json_decode_template_mod_tags_req(
    char *buf, neu_json_template_mod_tags_req_t **result);
void neu_json_decode_template_mod_tags_req_free(
    neu_json_template_mod_tags_req_t *req);

typedef struct {
    char * tmpl;
    char * group;
    int    n_tags;
    char **tags;
} neu_json_template_del_tags_req_t;

int neu_json_encode_template_del_tags_req(void *json_object, void *param);
int neu_json_decode_template_del_tags_req(
    char *buf, neu_json_template_del_tags_req_t **result);
void neu_json_decode_template_del_tags_req_free(
    neu_json_template_del_tags_req_t *req);

typedef struct {
    char *name;
    char *node;
} neu_json_template_inst_req_t;

int  neu_json_decode_template_inst_req(char *                         buf,
                                       neu_json_template_inst_req_t **result);
void neu_json_decode_template_inst_req_free(neu_json_template_inst_req_t *req);

typedef struct {
    neu_json_template_inst_req_t *insts;
    int                           len;
} neu_json_template_insts_req_t;

int  neu_json_decode_template_insts_req(char *                          buf,
                                        neu_json_template_insts_req_t **result);
void neu_json_decode_template_insts_req_free(
    neu_json_template_insts_req_t *req);

#endif
