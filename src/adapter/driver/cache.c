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
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "utils/uthash.h"

#include "define.h"
#include "tag.h"

#include "cache.h"

typedef struct {
    char group[NEU_GROUP_NAME_LEN];
    char tag[NEU_TAG_NAME_LEN];
} tkey_t;

struct elem {
    int32_t error;
    int64_t timestamp;

    uint16_t n_byte;
    uint8_t *bytes;

    tkey_t         key;
    UT_hash_handle hh;
};

struct neu_driver_cache {
    nng_mtx *mtx;

    struct elem *table;
};

static void update_tag_error(neu_driver_cache_t *cache, const char *group,
                             const char *tag, int64_t timestamp, int error);
static void update_group_error(neu_driver_cache_t *cache, const char *group,
                               int64_t timestamp, int error);

inline static tkey_t to_key(const char *group, const char *tag)
{
    tkey_t key = { 0 };

    strcpy(key.group, group);
    strcpy(key.tag, tag);

    return key;
}

neu_driver_cache_t *neu_driver_cache_new()
{
    neu_driver_cache_t *cache = calloc(1, sizeof(neu_driver_cache_t));

    nng_mtx_alloc(&cache->mtx);

    return cache;
}

void neu_driver_cache_destroy(neu_driver_cache_t *cache)
{
    struct elem *elem = NULL;
    struct elem *tmp  = NULL;

    nng_mtx_lock(cache->mtx);
    HASH_ITER(hh, cache->table, elem, tmp)
    {
        HASH_DEL(cache->table, elem);
        if (elem->n_byte > 0) {
            free(elem->bytes);
        }
        free(elem);
    }
    nng_mtx_unlock(cache->mtx);

    nng_mtx_free(cache->mtx);

    free(cache);
}

void neu_driver_cache_error(neu_driver_cache_t *cache, const char *group,
                            const char *tag, int64_t timestamp, int32_t error)
{
    if (tag == NULL) {
        update_group_error(cache, group, timestamp, error);
    } else {
        update_tag_error(cache, group, tag, timestamp, error);
    }
}

void neu_driver_cache_update(neu_driver_cache_t *cache, const char *group,
                             const char *tag, int64_t timestamp,
                             uint16_t n_byte, uint8_t *bytes)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    nng_mtx_lock(cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem == NULL) {
        elem = calloc(1, sizeof(struct elem));

        strcpy(elem->key.group, group);
        strcpy(elem->key.tag, tag);

        assert(n_byte != 0);
        assert(n_byte <= NEU_VALUE_SIZE);
        elem->n_byte = n_byte;
        elem->bytes  = calloc(elem->n_byte, 1);

        HASH_ADD(hh, cache->table, key, sizeof(tkey_t), elem);
    }

    elem->error     = 0;
    elem->timestamp = timestamp;

    if (elem->n_byte == 0) {
        assert(n_byte != 0);
        assert(n_byte <= NEU_VALUE_SIZE);
        elem->n_byte = n_byte;
        elem->bytes  = calloc(elem->n_byte, n_byte);
    } else {
        if (n_byte != elem->n_byte) {
            elem->n_byte = n_byte;
            elem->bytes  = realloc(elem->bytes, n_byte);
            memset(elem->bytes, 0, elem->n_byte);
        }
    }

    memcpy(elem->bytes, bytes, elem->n_byte);

    nng_mtx_unlock(cache->mtx);
}

int neu_driver_cache_get(neu_driver_cache_t *cache, const char *group,
                         const char *tag, neu_driver_cache_value_t *value)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    nng_mtx_lock(cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        value->error     = elem->error;
        value->n_byte    = elem->n_byte;
        value->timestamp = elem->timestamp;
        memcpy(value->value.bytes, elem->bytes, elem->n_byte);

        ret = 0;
    }

    nng_mtx_unlock(cache->mtx);

    return ret;
}

void neu_driver_cache_del(neu_driver_cache_t *cache, const char *group,
                          const char *tag)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    nng_mtx_lock(cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        HASH_DEL(cache->table, elem);
        if (elem->n_byte != 0) {
            free(elem->bytes);
        }
        free(elem);
    }

    nng_mtx_unlock(cache->mtx);
}

static void update_tag_error(neu_driver_cache_t *cache, const char *group,
                             const char *tag, int64_t timestamp, int error)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    nng_mtx_lock(cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem == NULL) {
        elem = calloc(1, sizeof(struct elem));
        strcpy(elem->key.group, group);
        strcpy(elem->key.tag, tag);

        HASH_ADD(hh, cache->table, key, sizeof(tkey_t), elem);
    }
    elem->error     = error;
    elem->timestamp = timestamp;

    nng_mtx_unlock(cache->mtx);
}

static void update_group_error(neu_driver_cache_t *cache, const char *group,
                               int64_t timestamp, int error)
{
    struct elem *elem = NULL, *tmp = NULL;

    nng_mtx_lock(cache->mtx);
    HASH_ITER(hh, cache->table, elem, tmp)
    {
        if (strcmp(elem->key.group, group) == 0) {
            elem->timestamp = timestamp;
            elem->error     = error;
        }
    }

    nng_mtx_unlock(cache->mtx);
}