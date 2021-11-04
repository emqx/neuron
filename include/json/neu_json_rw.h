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
#ifndef _NEU_JSON_API_READ_H_
#define _NEU_JSON_API_READ_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "neu_data_expr.h"
#include "json/json.h"

typedef struct neu_parse_read_req {
    uint32_t node_id;
    char *   group_config_name;
} neu_parse_read_req_t;

typedef struct neu_parse_read_res_tag {
    uint32_t             tag_id;
    enum neu_json_type   t;
    union neu_json_value value;
} neu_parse_read_res_tag_t;
typedef struct {
    int                       n_tag;
    neu_parse_read_res_tag_t *tags;
} neu_parse_read_res_t;

int  neu_parse_decode_read(char *buf, neu_parse_read_req_t **result);
void neu_parse_decode_read_free(neu_parse_read_req_t *req);
int  neu_parse_encode_read(void *json_object, void *param);

typedef struct {
    uint32_t             tag_id;
    enum neu_json_type   t;
    union neu_json_value value;
} neu_parse_write_req_tag_t;

typedef struct neu_parse_write_req {
    uint32_t                   node_id;
    char *                     group_config_name;
    int                        n_tag;
    neu_parse_write_req_tag_t *tags;
} neu_parse_write_req_t;

int             neu_parse_decode_write(char *buf, neu_parse_write_req_t **req);
void            neu_parse_decode_write_free(neu_parse_write_req_t *req);
neu_data_val_t *neu_parse_write_req_to_val(neu_parse_write_req_t *req);

#ifdef __cplusplus
}
#endif

#endif