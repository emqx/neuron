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

#ifndef __NEU_HT_H__
#define __NEU_HT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

// Open addressing hash table, but we use a better probe ( taken from Python)
// to avoid hitting the same positions.  Our hash algorithm is just the low
// order bits, and we use table sizes that are powers of two.

typedef struct neu_ht_map   neu_ht_map;
typedef struct neu_ht_entry neu_ht_entry;

typedef size_t (*neu_ht_hash_cb)(void *);
typedef _Bool (*neu_ht_eq_cb)(void *, void *);
typedef void (*neu_ht_free_cb)(void *);

// NB: These details are entirely private to the hash implementation.
// They are provided here to facilitate inlining in structures.
struct neu_ht_map {
    size_t         ht_cap;
    size_t         ht_count;
    size_t         ht_load;
    size_t         ht_min_load; // considers placeholders
    size_t         ht_max_load;
    neu_ht_entry * ht_entries;
    neu_ht_hash_cb ht_hash_cb;
    neu_ht_eq_cb   ht_eq_cb;
    neu_ht_free_cb ht_free_cb;
};

// The provided `free_cb` could be NULL, so that you manage memory yourself.
extern void  neu_ht_map_init(neu_ht_map *m, neu_ht_hash_cb hash_cb,
                             neu_ht_eq_cb eq_cb, neu_ht_free_cb free_cb);
extern void  neu_ht_map_fini(neu_ht_map *m);
extern void *neu_ht_get(neu_ht_map *m, void *key);
extern int   neu_ht_set(neu_ht_map *m, void *key, void *val);
extern int   neu_ht_remove(neu_ht_map *m, void *key);

extern size_t neu_ht_djb33(unsigned char *str);

#ifdef __cplusplus
}
#endif

#endif // __NEU_HT_H__
