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

#include <assert.h>
#include <stdlib.h>

#include "errcodes.h"
#include "utils/hash_table.h"

struct neu_hash_table_entry {
    size_t hash;
    size_t skips;
    void * key;
    void * val;
};

size_t neu_hash_cstr(const char *cstr)
{
    size_t hash = 5381;
    int    c;

    while ((c = (unsigned) *cstr++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

void neu_hash_table_init(neu_hash_table *tbl, neu_hash_table_hash_cb hash_cb,
                         neu_hash_table_free_cb free_cb)
{
    assert(hash_cb != NULL);
    tbl->entries  = NULL;
    tbl->count    = 0;
    tbl->load     = 0;
    tbl->cap      = 0;
    tbl->max_load = 0;
    tbl->min_load = 0; // never shrink below this
    tbl->hash_cb  = hash_cb;
    tbl->free_cb  = free_cb;
}

void neu_hash_table_fini(neu_hash_table *tbl)
{
    if (tbl->entries != NULL) {
        if (tbl->free_cb) {
            for (size_t i = 0; i < tbl->cap; ++i) {
                if (tbl->entries[i].val != NULL) {
                    tbl->free_cb(tbl->entries[i].val);
                }
            }
        }
        free(tbl->entries);
        tbl->entries = NULL;
        tbl->cap = tbl->count = 0;
        tbl->load = tbl->min_load = tbl->max_load = 0;
    }
}

// Inspired by Python dict implementation.  This probe will visit every
// cell.  We always hash consecutively assigned IDs.  This requires that
// the capacity is always a power of two.
#define ID_NEXT(t, j) ((((j) *5) + 1) & (t->cap - 1))
#define ID_INDEX(t, j) ((j) & (t->cap - 1))

static size_t hash_table_find(neu_hash_table *tbl, const char *key)
{
    size_t                index;
    size_t                start;
    uint32_t              hash;
    neu_hash_table_entry *ent;

    if (tbl->count == 0) {
        return ((size_t) -1);
    }

    hash  = tbl->hash_cb(key);
    index = ID_INDEX(tbl, hash);
    start = index;
    for (;;) {
        ent = &tbl->entries[index];
        // The entry is valid only if key not NULL
        if ((ent->hash == hash) && (ent->key != NULL) &&
            (0 == strcmp(ent->key, key))) {
            return (index);
        }
        if (ent->skips == 0) {
            return ((size_t) -1);
        }
        index = ID_NEXT(tbl, index);

        if (index == start) {
            break;
        }
    }

    return ((size_t) -1);
}

void *neu_hash_table_get(neu_hash_table *tbl, const char *key)
{
    size_t index;
    if ((index = hash_table_find(tbl, key)) == (size_t) -1) {
        return (NULL);
    }
    return (tbl->entries[index].val);
}

static int hash_table_resize(neu_hash_table *tbl)
{
    size_t                new_cap;
    size_t                old_cap;
    neu_hash_table_entry *new_entries;
    neu_hash_table_entry *old_entries;
    uint32_t              i;

    if ((tbl->load < tbl->max_load) && (tbl->load >= tbl->min_load)) {
        // No resize needed.
        return (0);
    }

    old_cap = tbl->cap;
    new_cap = 8;
    while (new_cap < (tbl->count * 2)) {
        new_cap *= 2;
    }
    if (new_cap == old_cap) {
        // Same size.
        return (0);
    }

    old_entries = tbl->entries;
    new_entries = calloc(new_cap, sizeof(*new_entries));
    if (new_entries == NULL) {
        return NEU_ERR_ENOMEM;
    }

    tbl->entries = new_entries;
    tbl->cap     = new_cap;
    tbl->load    = 0;
    if (new_cap > 8) {
        tbl->min_load = new_cap / 8;
        tbl->max_load = new_cap * 2 / 3;
    } else {
        tbl->min_load = 0;
        tbl->max_load = 5;
    }
    for (i = 0; i < old_cap; i++) {
        size_t index;
        if (old_entries[i].val == NULL) {
            continue;
        }
        index = old_entries[i].hash & (new_cap - 1);
        for (;;) {
            // Increment the load unconditionally.  It counts
            // once for every item stored, plus once for each
            // hashing operation we use to store the item (i.e.
            // one for the item, plus once for each rehash.)
            tbl->load++;
            if (new_entries[index].key == NULL) {
                // As we are hitting this entry for the first
                // time, it won't have any skips.
                assert(new_entries[index].skips == 0);
                new_entries[index].hash = old_entries[i].hash;
                new_entries[index].key  = old_entries[i].key;
                new_entries[index].val  = old_entries[i].val;
                break;
            }
            new_entries[index].skips++;
            index = ID_NEXT(tbl, index);
        }
    }
    if (old_cap != 0) {
        free(old_entries);
    }
    return (0);
}

int neu_hash_table_remove(neu_hash_table *tbl, const char *key)
{
    size_t   index;
    size_t   probe;
    uint32_t hash;

    if ((index = hash_table_find(tbl, key)) == (size_t) -1) {
        return NEU_ERR_ENOENT;
    }

    // Now we have found the index where the object exists.  We are going
    // to restart the search, until the index matches, to decrement the
    // skips counter.
    hash  = tbl->hash_cb(key);
    probe = ID_INDEX(tbl, hash);

    for (;;) {
        neu_hash_table_entry *entry;

        // The load was increased once each hashing operation we used
        // to place the the item.  Decrement it accordingly.
        tbl->load--;
        entry = &tbl->entries[probe];
        if (probe == index) {
            entry->hash = 0;
            if (NULL != tbl->free_cb) {
                tbl->free_cb(entry->val);
            }
            entry->val = NULL;
            entry->key = NULL; // invalid key
            break;
        }
        assert(entry->skips > 0);
        entry->skips--;
        probe = ID_NEXT(tbl, probe);
    }

    tbl->count--;

    // Shrink -- but it's ok if we can't.
    (void) hash_table_resize(tbl);

    return (0);
}

int neu_hash_table_set(neu_hash_table *tbl, const char *key, void *val)
{
    size_t                index;
    uint32_t              hash;
    neu_hash_table_entry *ent;

    if (NULL == val) {
        return NEU_ERR_EINVAL;
    }

    // the table is filled full
    if ((~(size_t) 0) == tbl->count) {
        return NEU_ERR_ENOMEM;
    }

    // Try to resize -- if we don't need to, this will be a no-op.
    if (hash_table_resize(tbl) != 0) {
        return NEU_ERR_ENOMEM;
    }

    // If it already exists, just overwrite the old value.
    if ((index = hash_table_find(tbl, key)) != (size_t) -1) {
        ent = &tbl->entries[index];
        if (NULL != tbl->free_cb) {
            tbl->free_cb(ent->val);
        }
        ent->val = val;
        return (0);
    }

    hash  = tbl->hash_cb(key);
    index = ID_INDEX(tbl, hash);
    for (;;) {
        ent = &tbl->entries[index];

        // Increment the load count.  We do this each time time we
        // rehash.  This may over-count items that collide on the
        // same rehashing, but this should just cause a table to
        // grow sooner, which is probably a good thing.
        tbl->load++;
        if (ent->val == NULL) {
            tbl->count++;
            ent->hash = hash;
            ent->key  = (void *) key;
            ent->val  = val;
            return (0);
        }
        // Record the skip count.  This being non-zero informs
        // that a rehash will be necessary.  Without this we
        // would need to scan the entire hash for the match.
        ent->skips++;
        index = ID_NEXT(tbl, index);
    }
}
