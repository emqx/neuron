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

#ifndef __MEM_ALLOC_H__
#define __MEM_ALLOC_H__

#include "neu_panic.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEU_ARG_UNUSED(x) ((void) x)

void *neu_alloc(size_t sz);
void *neu_zalloc(size_t sz);
void  neu_free(void *b, size_t z);

#define NEU_ALLOC_STRUCT(s) neu_zalloc(sizeof(*s))
#define NEU_FREE_STRUCT(s) neu_free((s), sizeof(*s))
#define NEU_ALLOC_STRUCTS(s, n) neu_zalloc(sizeof(*s) * n)
#define NEU_FREE_STRUCTS(s, n) neu_free(s, sizeof(*s) * n)

#ifdef __cplusplus
}
#endif

#endif
