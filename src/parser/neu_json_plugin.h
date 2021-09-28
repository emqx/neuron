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

typedef struct {
    plugin_kind_e   kind;
    neu_node_type_e node_type;
    char *          plugin_name;
    char *          plugin_lib_name;
} neu_parse_add_plugin_req_t;

int neu_parse_decode_add_plugin(char *buf, neu_parse_add_plugin_req_t **result);
void neu_parse_decode_add_plugin_free(neu_parse_add_plugin_req_t *req);

typedef struct {
    uint32_t plugin_id;
} neu_parse_del_plugin_req_t;

int neu_parse_decode_del_plugin(char *buf, neu_parse_del_plugin_req_t **result);
void neu_parse_decode_del_plugin_free(neu_parse_del_plugin_req_t *req);

typedef struct {
    plugin_kind_e   kind;
    neu_node_type_e node_type;
    char *          plugin_name;
    char *          plugin_lib_name;
} neu_parse_update_plugin_req_t;

int  neu_parse_decode_update_plugin(char *                          buf,
                                    neu_parse_update_plugin_req_t **result);
void neu_parse_decode_update_plugin_free(neu_parse_update_plugin_req_t *req);

typedef struct {
    uint32_t reserved;
} neu_parse_get_plugins_req_t;

typedef struct {
    uint32_t        plugin_id;
    plugin_kind_e   kind;
    neu_node_type_e node_type;
    char *          plugin_name;
    char *          plugin_lib_name;
} neu_parse_get_plugins_res_lib_t;

typedef struct neu_parse_get_plugins_res {
    uint16_t                         n_plugin;
    neu_parse_get_plugins_res_lib_t *plugin_libs;
} neu_parse_get_plugins_res_t;

int  neu_parse_decode_get_plugins(char *                        buf,
                                  neu_parse_get_plugins_req_t **result);
void neu_parse_decode_get_plugins_free(neu_parse_get_plugins_req_t *req);
int  neu_parse_encode_get_plugins(void *json_object, void *param);

#ifdef __cplusplus
}
#endif

#endif
