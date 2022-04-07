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

#include "utils/idhash.h"

struct neu_id_entry {
    uint32_t key;
    uint32_t skips;
    void *   val;
};

void neu_id_map_init(neu_id_map *m, uint32_t lo, uint32_t hi)
{
    if (lo == 0) {
        lo = 1;
    }
    if (hi == 0) {
        hi = 0xffffffffu;
    }
    assert(lo != 0);
    assert(hi > lo);
    m->id_entries  = NULL;
    m->id_count    = 0;
    m->id_load     = 0;
    m->id_cap      = 0;
    m->id_max_load = 0;
    m->id_min_load = 0; // never shrink below this
    m->id_min_val  = lo;
    m->id_max_val  = hi;
    m->id_dyn_val  = lo;
}

void neu_id_map_fini(neu_id_map *m)
{
    if (m->id_entries != NULL) {
        free(m->id_entries);
        m->id_entries = NULL;
        m->id_cap = m->id_count = 0;
        m->id_load = m->id_min_load = m->id_max_load = 0;
    }
}

// Inspired by Python dict implementation.  This probe will visit every
// cell.  We always hash consecutively assigned IDs.  This requires that
// the capacity is always a power of two.
#define ID_NEXT(m, j) ((((j) *5) + 1) & (m->id_cap - 1))
#define ID_INDEX(m, j) ((j) & (m->id_cap - 1))

static size_t id_find(neu_id_map *m, uint32_t id)
{
    size_t index;
    size_t start;
    if (m->id_count == 0) {
        return ((size_t) -1);
    }

    index = ID_INDEX(m, id);
    start = index;
    for (;;) {
        // The value of ihe_key is only valid if ihe_val is not NULL.
        if ((m->id_entries[index].key == id) &&
            (m->id_entries[index].val != NULL)) {
            return (index);
        }
        if (m->id_entries[index].skips == 0) {
            return ((size_t) -1);
        }
        index = ID_NEXT(m, index);

        if (index == start) {
            break;
        }
    }

    return ((size_t) -1);
}

void *neu_id_get(neu_id_map *m, uint32_t id)
{
    size_t index;
    if ((index = id_find(m, id)) == (size_t) -1) {
        return (NULL);
    }
    return (m->id_entries[index].val);
}

static int id_resize(neu_id_map *m)
{
    size_t        new_cap;
    size_t        old_cap;
    neu_id_entry *new_entries;
    neu_id_entry *old_entries;
    uint32_t      i;

    if ((m->id_load < m->id_max_load) && (m->id_load >= m->id_min_load)) {
        // No resize needed.
        return (0);
    }

    old_cap = m->id_cap;
    new_cap = 8;
    while (new_cap < (m->id_count * 2)) {
        new_cap *= 2;
    }
    if (new_cap == old_cap) {
        // Same size.
        return (0);
    }

    old_entries = m->id_entries;
    new_entries = calloc(new_cap, sizeof(*new_entries));
    if (new_entries == NULL) {
        return -2;
    }

    m->id_entries = new_entries;
    m->id_cap     = new_cap;
    m->id_load    = 0;
    if (new_cap > 8) {
        m->id_min_load = new_cap / 8;
        m->id_max_load = new_cap * 2 / 3;
    } else {
        m->id_min_load = 0;
        m->id_max_load = 5;
    }
    for (i = 0; i < old_cap; i++) {
        size_t index;
        if (old_entries[i].val == NULL) {
            continue;
        }
        index = old_entries[i].key & (new_cap - 1);
        for (;;) {
            // Increment the load unconditionally.  It counts
            // once for every item stored, plus once for each
            // hashing operation we use to store the item (i.e.
            // one for the item, plus once for each rehash.)
            m->id_load++;
            if (new_entries[index].val == NULL) {
                // As we are hitting this entry for the first
                // time, it won't have any skips.
                assert(new_entries[index].skips == 0);
                new_entries[index].val = old_entries[i].val;
                new_entries[index].key = old_entries[i].key;
                break;
            }
            new_entries[index].skips++;
            index = ID_NEXT(m, index);
        }
    }
    if (old_cap != 0) {
        free(old_entries);
    }
    return (0);
}

int neu_id_remove(neu_id_map *m, uint32_t id)
{
    size_t index;
    size_t probe;

    if ((index = id_find(m, id)) == (size_t) -1) {
        return -2;
    }

    // Now we have found the index where the object exists.  We are going
    // to restart the search, until the index matches, to decrement the
    // skips counter.
    probe = ID_INDEX(m, id);

    for (;;) {
        neu_id_entry *entry;

        // The load was increased once each hashing operation we used
        // to place the the item.  Decrement it accordingly.
        m->id_load--;
        entry = &m->id_entries[probe];
        if (probe == index) {
            entry->val = NULL;
            entry->key = 0; // invalid key
            break;
        }
        assert(entry->skips > 0);
        entry->skips--;
        probe = ID_NEXT(m, probe);
    }

    m->id_count--;

    // Shrink -- but it's ok if we can't.
    (void) id_resize(m);

    return (0);
}

int neu_id_set(neu_id_map *m, uint32_t id, void *val)
{
    size_t        index;
    neu_id_entry *ent;

    // Try to resize -- if we don't need to, this will be a no-op.
    if (id_resize(m) != 0) {
        return -2;
    }

    // If it already exists, just overwrite the old value.
    if ((index = id_find(m, id)) != (size_t) -1) {
        ent      = &m->id_entries[index];
        ent->val = val;
        return (0);
    }

    index = ID_INDEX(m, id);
    for (;;) {
        ent = &m->id_entries[index];

        // Increment the load count.  We do this each time time we
        // rehash.  This may over-count items that collide on the
        // same rehashing, but this should just cause a table to
        // grow sooner, which is probably a good thing.
        m->id_load++;
        if (ent->val == NULL) {
            m->id_count++;
            ent->key = id;
            ent->val = val;
            return (0);
        }
        // Record the skip count.  This being non-zero informs
        // that a rehash will be necessary.  Without this we
        // would need to scan the entire hash for the match.
        ent->skips++;
        index = ID_NEXT(m, index);
    }
}

int neu_id_alloc(neu_id_map *m, uint32_t *idp, void *val)
{
    uint32_t id;
    int      rv;

    assert(val != NULL);

    // range is inclusive, so > to get +1 effect.
    if (m->id_count > (m->id_max_val - m->id_min_val)) {
        // Really more like ENOSPC.. the table is filled to max.
        return -2;
    }

    for (;;) {
        id = m->id_dyn_val;
        m->id_dyn_val++;
        if (m->id_dyn_val > m->id_max_val) {
            m->id_dyn_val = m->id_min_val;
        }

        if (id_find(m, id) == (size_t) -1) {
            break;
        }
    }

    rv = neu_id_set(m, id, val);
    if (rv == 0) {
        *idp = id;
    }
    return (rv);
}

size_t neu_id_traverse(neu_id_map *m, void (*cb)(uint32_t, void *, void *),
                       void *      userdata)
{
    size_t index;
    size_t n = 0;
    if (m->id_count == 0) {
        return 0;
    }

    for (index = 0; index < m->id_cap; index++) {
        if (m->id_entries[index].val) {
            cb(m->id_entries[index].key, m->id_entries[index].val, userdata);
            n++;
        }
    }

    return n;
}
