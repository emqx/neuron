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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "json/json.h"
#include "json/neu_json_error.h"

int neu_parse_encode_error(void *json_object, void *param)
{
    neu_parse_error_t *res = (neu_parse_error_t *) param;

    neu_json_elem_t elems[] = {
        {
            .name      = "error",
            .t         = NEU_JSON_INT,
            .v.val_int = res->error,
        },
    };

    return neu_json_encode_field(json_object, elems, 1);
}