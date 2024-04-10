/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef NEURON_PLUGIN_EKUIPER_JSON_RW_H
#define NEURON_PLUGIN_EKUIPER_JSON_RW_H

#include <sys/time.h>

#include "neuron.h"
#include "json/neu_json_rw.h"

#ifdef __cplusplus
extern "C" {
#endif

// NOTE: these utilities may better be reused by mqtt and restful plugin
//       should move to neuron public sources

typedef struct {
    char *   node_name;
    char *   group_name;
    uint64_t timestamp;
} json_read_resp_header_t;

int wrap_tag_data(neu_json_read_resp_tag_t * json_tag,
                  neu_resp_tag_value_meta_t *tag_value);

typedef struct {
    neu_plugin_t *            plugin;
    neu_reqresp_trans_data_t *trans_data;
} json_read_resp_t;

// { "node_name": "node0", "group_name": "grp0", "timestamp": 1649776722631 }
int json_encode_read_resp_header(void *json_object, void *param);

// { "values": { "tag0": 0 }, "errors": { "tag1": 3000 } }
int json_encode_read_resp_tags(void *json_object, void *param);

// {
//    "node_name": "node0",
//    "group_name": "grp0",
//    "timestamp": 1649776722631,
//    "values": { "tag0": 0 },
//    "errors": { "tag1": 3000 }
// }
int json_encode_read_resp(void *json_object, void *param);

typedef struct {
    char *               node_name;
    char *               group_name;
    char *               tag_name;
    enum neu_json_type   t;
    union neu_json_value value;
} json_write_req_t;

int json_decode_write_req(char *buf, size_t len, neu_json_write_t **result);

#ifdef __cplusplus
}
#endif

#endif
