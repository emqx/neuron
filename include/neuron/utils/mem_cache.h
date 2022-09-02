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

#ifndef NEU_MEM_CACHE
#define NEU_MEM_CACHE

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

typedef struct neu_mem_cache neu_mem_cache_t;
typedef void (*cache_data_release)(void *data);

typedef struct {
    size_t             size;
    void *             data;
    cache_data_release release;
    uint32_t           timestamp;
} cache_item_t;

typedef void (*cache_dump)(cache_item_t *item, void *ctx);

neu_mem_cache_t *neu_mem_cache_create(const size_t max_bytes,
                                      const size_t max_items);
int              neu_mem_cache_add(neu_mem_cache_t *cache, cache_item_t *item);
cache_item_t     neu_mem_cache_earliest(neu_mem_cache_t *cache);
cache_item_t     neu_mem_cache_latest(neu_mem_cache_t *cache);
void             neu_mem_cache_used(neu_mem_cache_t *cache, size_t *used_bytes,
                                    size_t *used_items);
void             neu_mem_cache_resize(neu_mem_cache_t *cache, size_t new_bytes,
                                      size_t new_items);
int  neu_mem_cache_dump(neu_mem_cache_t *cache, cache_dump callback, void *ctx);
int  neu_mem_cache_clear(neu_mem_cache_t *cache);
void neu_mem_cache_destroy(neu_mem_cache_t *cache);

#ifdef __cplusplus
}
#endif
#endif