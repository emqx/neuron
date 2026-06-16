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

#ifndef _NEU_UTILS_EDE_H_
#define _NEU_UTILS_EDE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "msg.h"
#include "tag.h"
#include "type.h"

typedef struct {
    char            name[256];
    char            address[128];
    neu_type_e      data_type;
    char            description[512];
    neu_attribute_e attribute;
} neu_ede_entry_t;

typedef struct {
    neu_ede_entry_t *entries;
    size_t           count;
} neu_ede_result_t;

int  neu_ede_parse_file(const char *file_path, neu_ede_result_t *result);
void neu_ede_result_uninit(neu_ede_result_t *result);

int  neu_ede_parse_file_to_tags(const char *file_path, neu_datatag_t **tags,
                                size_t *count);
void neu_ede_tags_uninit(neu_datatag_t *tags, size_t count);
void neu_ede_to_msg(char *driver, const char *path, neu_req_add_gtag_t *cmd);

int neu_ede_format_address(const char *area, uint32_t address,
                           const char *property_id, bool custom,
                           uint32_t custom_id, char *out, size_t out_size);

neu_type_e neu_ede_get_data_type(const char *area, const char *property_id,
                                 bool custom);

#ifdef __cplusplus
}
#endif

#endif