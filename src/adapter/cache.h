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

#ifndef _NEU_DRIVER_CACHE_H_
#define _NEU_DRIVER_CACHE_H_

#include <stdint.h>

#include "type.h"

typedef struct neu_driver_cache neu_driver_cache_t;

neu_driver_cache_t *neu_driver_cache_new();
void                neu_driver_cache_destroy(neu_driver_cache_t *cache);

void neu_driver_cache_error(neu_driver_cache_t *cache, const char *key,
                            int64_t timestamp, int32_t error);
void neu_driver_cache_update(neu_driver_cache_t *cache, const char *key,
                             int64_t timestamp, uint16_t n_byte,
                             uint8_t *bytes);
void neu_driver_cache_del(neu_driver_cache_t *cache, const char *key);

typedef struct {
    uint16_t    n_byte;
    neu_value_u value;

    int32_t error;
    int64_t timestamp;
} neu_driver_cache_value_t;

int neu_driver_cache_get(neu_driver_cache_t *cache, const char *key,
                         neu_driver_cache_value_t *value);

#endif
