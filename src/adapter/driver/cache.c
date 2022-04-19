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
#include <pthread.h>
#include <string.h>

#include "utils/uthash.h"

#include "tag.h"

#include "cache.h"

struct elem {
    int32_t error;
    int64_t timestamp;

    uint16_t n_byte;
    uint8_t *bytes;

    char           name[NEU_TAG_NAME_LEN];
    UT_hash_handle hh;
};

struct neu_driver_cache {
    pthread_mutex_t mtx;

    struct elem *table;
};

neu_driver_cache_t *neu_driver_cache_new()
{
    neu_driver_cache_t *cache = calloc(1, sizeof(neu_driver_cache_t));

    pthread_mutex_init(&cache->mtx, NULL);

    return cache;
}

void neu_driver_cache_destroy(neu_driver_cache_t *cache)
{
    struct elem *elem = NULL;
    struct elem *tmp  = NULL;

    pthread_mutex_lock(&cache->mtx);
    HASH_ITER(hh, cache->table, elem, tmp)
    {
        HASH_DEL(cache->table, elem);
        if (elem->n_byte > 0) {
            free(elem->bytes);
        }
        free(elem);
    }
    pthread_mutex_unlock(&cache->mtx);

    pthread_mutex_destroy(&cache->mtx);

    free(cache);
}

void neu_driver_cache_error(neu_driver_cache_t *cache, const char *key,
                            int64_t timestamp, int32_t error)
{
    struct elem *elem = NULL;

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND_STR(cache->table, key, elem);

    if (elem == NULL) {
        elem = calloc(1, sizeof(struct elem));
        strncpy(elem->name, key, sizeof(elem->name));
        HASH_ADD_STR(cache->table, name, elem);
    }
    elem->timestamp = timestamp;
    elem->error     = error;

    pthread_mutex_unlock(&cache->mtx);
}

void neu_driver_cache_update(neu_driver_cache_t *cache, const char *key,
                             int64_t timestamp, uint16_t n_byte, uint8_t *bytes)
{
    struct elem *elem = NULL;

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND_STR(cache->table, key, elem);

    if (elem == NULL) {
        elem = calloc(1, sizeof(struct elem));
        strncpy(elem->name, key, sizeof(elem->name));
        assert(n_byte != 0);
        assert(n_byte <= NEU_VALUE_SIZE);
        elem->n_byte = n_byte;
        elem->bytes  = calloc(elem->n_byte, 1);
        HASH_ADD_STR(cache->table, name, elem);
    }

    elem->error     = 0;
    elem->timestamp = timestamp;

    if (elem->n_byte == 0) {
        assert(n_byte != 0);
        assert(n_byte <= NEU_VALUE_SIZE);
        elem->n_byte = n_byte;
        elem->bytes  = calloc(elem->n_byte, 1);
    } else {
        assert(n_byte == elem->n_byte);
    }

    memcpy(elem->bytes, bytes, elem->n_byte);

    pthread_mutex_unlock(&cache->mtx);
}

int neu_driver_cache_get(neu_driver_cache_t *cache, const char *key,
                         neu_driver_cache_value_t *value)
{
    struct elem *elem = NULL;
    int          ret  = -1;

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND_STR(cache->table, key, elem);

    if (elem != NULL) {
        value->error     = elem->error;
        value->n_byte    = elem->n_byte;
        value->timestamp = elem->timestamp;
        memcpy(value->value.bytes, elem->bytes, elem->n_byte);

        ret = 0;
    }

    pthread_mutex_unlock(&cache->mtx);

    return ret;
}

void neu_driver_cache_del(neu_driver_cache_t *cache, const char *key)
{
    struct elem *elem = NULL;

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND_STR(cache->table, key, elem);

    if (elem != NULL) {
        HASH_DEL(cache->table, elem);
        if (elem->n_byte != 0) {
            free(elem->bytes);
        }
        free(elem);
    }

    pthread_mutex_unlock(&cache->mtx);
}
