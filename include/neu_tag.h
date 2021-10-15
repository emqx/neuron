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
config_ **/

#ifndef NEURON_TAG_H
#define NEURON_TAG_H

#include <stdint.h>

#include "neu_data_expr.h"

typedef uint32_t neu_datatag_id_t;
typedef char *   neu_addr_str_t;
typedef char *   neu_tag_name;

typedef enum {
    NEU_ATTRIBUTETYPE_READ      = 1,
    NEU_ATTRIBUTETYPE_WRITE     = 2,
    NEU_ATTRIBUTETYPE_SUBSCRIBE = 4,
} neu_attributetype_e;

typedef struct {
    neu_datatag_id_t    id;
    neu_attributetype_e attribute;
    neu_dtype_e         type;
    neu_addr_str_t      addr_str;
    neu_tag_name        name;
} neu_datatag_t;

#endif