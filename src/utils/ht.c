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

#include "ht.h"
#include "mem_alloc.h"

struct neu_ht_entry {
    size_t hash;
    size_t skips;
    void * key;
    void * val;
};

size_t neu_ht_djb33(unsigned char *str)
{
    size_t hash = 5381;
    int    c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

void neu_ht_map_init(neu_ht_map *m, neu_ht_hash_cb hash_cb, neu_ht_eq_cb eq_cb,
                     neu_ht_free_cb free_cb)
{
    assert(hash_cb != NULL);
    assert(eq_cb != NULL);
    m->ht_entries  = NULL;
    m->ht_count    = 0;
    m->ht_load     = 0;
    m->ht_cap      = 0;
    m->ht_max_load = 0;
    m->ht_min_load = 0; // never shrink below this
    m->ht_hash_cb  = hash_cb;
    m->ht_eq_cb    = eq_cb;
    m->ht_free_cb  = free_cb;
}

void neu_ht_map_fini(neu_ht_map *m)
{
    if (m->ht_entries != NULL) {
        if (m->ht_free_cb) {
            for (size_t i = 0; i < m->ht_cap; ++i) {
                if (m->ht_entries[i].val != NULL) {
                    m->ht_free_cb(m->ht_entries[i].key);
                    m->ht_free_cb(m->ht_entries[i].val);
                }
            }
        }
        NEU_FREE_STRUCTS(m->ht_entries, m->ht_cap);
        m->ht_entries = NULL;
        m->ht_cap = m->ht_count = 0;
        m->ht_load = m->ht_min_load = m->ht_max_load = 0;
    }
}

// Inspired by Python dict implementation.  This probe will visit every
// cell.  We always hash consecutively assigned IDs.  This requires that
// the capacity is always a power of two.
#define ID_NEXT(m, j) ((((j) *5) + 1) & (m->ht_cap - 1))
#define ID_INDEX(m, j) ((j) & (m->ht_cap - 1))

static size_t ht_find(neu_ht_map *m, void *key)
{
    size_t        index;
    size_t        start;
    uint32_t      hash;
    neu_ht_entry *ent;

    if (m->ht_count == 0) {
        return ((size_t) -1);
    }

    hash  = m->ht_hash_cb(key);
    index = ID_INDEX(m, hash);
    start = index;
    for (;;) {
        ent = &m->ht_entries[index];
        // The entry is valid only if key not NULL
        if ((ent->hash == hash) && (ent->key != NULL) &&
            (m->ht_eq_cb(ent->key, key))) {
            return (index);
        }
        if (ent->skips == 0) {
            return ((size_t) -1);
        }
        index = ID_NEXT(m, index);

        if (index == start) {
            break;
        }
    }

    return ((size_t) -1);
}

void *neu_ht_get(neu_ht_map *m, void *key)
{
    size_t index;
    if ((index = ht_find(m, key)) == (size_t) -1) {
        return (NULL);
    }
    return (m->ht_entries[index].val);
}

static int ht_resize(neu_ht_map *m)
{
    size_t        new_cap;
    size_t        old_cap;
    neu_ht_entry *new_entries;
    neu_ht_entry *old_entries;
    uint32_t      i;

    if ((m->ht_load < m->ht_max_load) && (m->ht_load >= m->ht_min_load)) {
        // No resize needed.
        return (0);
    }

    old_cap = m->ht_cap;
    new_cap = 8;
    while (new_cap < (m->ht_count * 2)) {
        new_cap *= 2;
    }
    if (new_cap == old_cap) {
        // Same size.
        return (0);
    }

    old_entries = m->ht_entries;
    new_entries = NEU_ALLOC_STRUCTS(new_entries, new_cap);
    if (new_entries == NULL) {
        return -2;
    }

    m->ht_entries = new_entries;
    m->ht_cap     = new_cap;
    m->ht_load    = 0;
    if (new_cap > 8) {
        m->ht_min_load = new_cap / 8;
        m->ht_max_load = new_cap * 2 / 3;
    } else {
        m->ht_min_load = 0;
        m->ht_max_load = 5;
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
            m->ht_load++;
            if (new_entries[index].key == NULL) {
                // As we are hitting this entry for the first
                // time, it won't have any skips.
                NEU_ASSERT(new_entries[index].skips == 0);
                new_entries[index].hash = old_entries[i].hash;
                new_entries[index].key  = old_entries[i].key;
                new_entries[index].val  = old_entries[i].val;
                break;
            }
            new_entries[index].skips++;
            index = ID_NEXT(m, index);
        }
    }
    if (old_cap != 0) {
        NEU_FREE_STRUCTS(old_entries, old_cap);
    }
    return (0);
}

int neu_ht_remove(neu_ht_map *m, void *key)
{
    size_t   index;
    size_t   probe;
    uint32_t hash;

    if ((index = ht_find(m, key)) == (size_t) -1) {
        return -2;
    }

    // Now we have found the index where the object exists.  We are going
    // to restart the search, until the index matches, to decrement the
    // skips counter.
    hash  = m->ht_hash_cb(key);
    probe = ID_INDEX(m, hash);

    for (;;) {
        neu_ht_entry *entry;

        // The load was increased once each hashing operation we used
        // to place the the item.  Decrement it accordingly.
        m->ht_load--;
        entry = &m->ht_entries[probe];
        if (probe == index) {
            entry->hash = 0;
            if (NULL != m->ht_free_cb) {
                m->ht_free_cb(entry->key);
                m->ht_free_cb(entry->val);
            }
            entry->val = NULL;
            entry->key = NULL; // invalid key
            break;
        }
        NEU_ASSERT(entry->skips > 0);
        entry->skips--;
        probe = ID_NEXT(m, probe);
    }

    m->ht_count--;

    // Shrink -- but it's ok if we can't.
    (void) ht_resize(m);

    return (0);
}

int neu_ht_set(neu_ht_map *m, void *key, void *val)
{
    size_t        index;
    uint32_t      hash;
    neu_ht_entry *ent;

    // the table is filled full
    if ((~(size_t) 0) == m->ht_count) {
        return -2;
    }

    // Try to resize -- if we don't need to, this will be a no-op.
    if (ht_resize(m) != 0) {
        return -2;
    }

    // If it already exists, just overwrite the old value.
    if ((index = ht_find(m, key)) != (size_t) -1) {
        ent = &m->ht_entries[index];
        if (NULL != m->ht_free_cb) {
            m->ht_free_cb(ent->val);
        }
        ent->val = val;
        return (0);
    }

    hash  = m->ht_hash_cb(key);
    index = ID_INDEX(m, hash);
    for (;;) {
        ent = &m->ht_entries[index];

        // Increment the load count.  We do this each time time we
        // rehash.  This may over-count items that collide on the
        // same rehashing, but this should just cause a table to
        // grow sooner, which is probably a good thing.
        m->ht_load++;
        if (ent->val == NULL) {
            m->ht_count++;
            ent->hash = hash;
            ent->key  = key;
            ent->val  = val;
            return (0);
        }
        // Record the skip count.  This being non-zero informs
        // that a rehash will be necessary.  Without this we
        // would need to scan the entire hash for the match.
        ent->skips++;
        index = ID_NEXT(m, index);
    }
}
