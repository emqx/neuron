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

#ifndef _NEU_SCHEMA_H_
#define _NEU_SCHEMA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum neu_schema_valid_type {
    NEU_SCHEMA_VALID_INT,
    NEU_SCHEMA_VALID_REAL,
    NEU_SCHEMA_VALID_STRING,
    NEU_SCHEMA_VALID_ENUM,
} neu_schema_valid_type_e;

typedef struct neu_schema_valid neu_schema_valid_t;

neu_schema_valid_t *neu_schema_load(char *buf, char *plugin);
void                neu_schema_free(neu_schema_valid_t *v);

int neu_schema_valid_param_int(neu_schema_valid_t *v, int64_t value,
                               char *name);
int neu_schema_valid_param_real(neu_schema_valid_t *v, double value,
                                char *name);
int neu_schema_valid_param_string(neu_schema_valid_t *v, char *value,
                                  char *name);
int neu_schema_valid_param_enum(neu_schema_valid_t *v, int64_t value,
                                char *name);
int neu_schema_valid_tag_type(neu_schema_valid_t *v, uint8_t t);

#ifdef __cplusplus
}
#endif

#endif
