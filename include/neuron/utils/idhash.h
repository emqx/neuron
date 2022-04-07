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

#ifndef __NEU_IDHASH_H__
#define __NEU_IDHASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

// We find that we often want to have a list of things listed by a
// numeric ID, which is generally monotonically increasing.  This is
// most often a pipe ID.  To help keep collections of these things
// indexed by their ID (which might start from a very large value),
// we offer a hash table.  The hash table uses open addressing, but
// we use a better probe (taken from Python) to avoid hitting the same
// positions.  Our hash algorithm is just the low order bits, and we
// use table sizes that are powers of two.  Note that hash items
// must be non-NULL.  The table is protected by an internal lock.

typedef struct neu_id_map   neu_id_map;
typedef struct neu_id_entry neu_id_entry;

// NB: These details are entirely private to the hash implementation.
// They are provided here to facilitate inlining in structures.
struct neu_id_map {
    size_t        id_cap;
    size_t        id_count;
    size_t        id_load;
    size_t        id_min_load; // considers placeholders
    size_t        id_max_load;
    uint32_t      id_min_val;
    uint32_t      id_max_val;
    uint32_t      id_dyn_val;
    neu_id_entry *id_entries;
};

extern void   neu_id_map_init(neu_id_map *, uint32_t, uint32_t);
extern void   neu_id_map_fini(neu_id_map *);
extern void * neu_id_get(neu_id_map *, uint32_t);
extern int    neu_id_set(neu_id_map *, uint32_t, void *);
extern int    neu_id_alloc(neu_id_map *, uint32_t *, void *);
extern int    neu_id_remove(neu_id_map *, uint32_t);
extern size_t neu_id_traverse(neu_id_map *,
                              void (*cb)(uint32_t, void *, void *), void *);

#ifdef __cplusplus
}
#endif

#endif // __NEU_IDHASH_H__
