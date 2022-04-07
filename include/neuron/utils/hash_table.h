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

#ifndef __NEU_HASH_TABLE_H__
#define __NEU_HASH_TABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

// Open addressing hash table, but we use a better probe ( taken from Python)
// to avoid hitting the same positions.  Our hash algorithm is just the low
// order bits, and we use table sizes that are powers of two.

typedef struct neu_hash_table       neu_hash_table;
typedef struct neu_hash_table_entry neu_hash_table_entry;

typedef size_t (*neu_hash_table_hash_cb)(const char *);
typedef void (*neu_hash_table_free_cb)(void *);

// NB: These details are entirely private to the hash implementation.
// They are provided here to facilitate inlining in structures.
struct neu_hash_table {
    size_t                 cap;
    size_t                 count;
    size_t                 load;
    size_t                 min_load; // considers placeholders
    size_t                 max_load;
    neu_hash_table_entry * entries;
    neu_hash_table_hash_cb hash_cb;
    neu_hash_table_free_cb free_cb;
};

// The provided `free_cb` could be NULL, so that you manage memory yourself.
extern void  neu_hash_table_init(neu_hash_table *       m,
                                 neu_hash_table_hash_cb hash_cb,
                                 neu_hash_table_free_cb free_cb);
extern void  neu_hash_table_fini(neu_hash_table *m);
extern void *neu_hash_table_get(neu_hash_table *m, const char *key);
// The hash table take ownership of the value but not the key.
// Returns 0 on success, NEU_ERR_EINVAL if val is NULL, NEU_ERR_ENOMEM if not
// sufficient memory.
extern int neu_hash_table_set(neu_hash_table *m, const char *key, void *val);
// Returns 0 on success, NEU_ERR_ENOENT if the key does not exist.
extern int neu_hash_table_remove(neu_hash_table *m, const char *key);

extern size_t neu_hash_cstr(const char *cstr);

#ifdef __cplusplus
}
#endif

#endif // __NEU_HASH_TABLE_H__
