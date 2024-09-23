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
#include <math.h>
#include <pthread.h>
#include <string.h>

#include "utils/uthash.h"

#include "define.h"
#include "tag.h"

#include "cache.h"

extern bool sub_filter_err;

typedef struct {
    char group[NEU_GROUP_NAME_LEN];
    char tag[NEU_TAG_NAME_LEN];
} tkey_t;

struct elem {
    int64_t timestamp;
    bool    changed;

    neu_dvalue_t value;
    neu_dvalue_t value_old;

    neu_tag_meta_t metas[NEU_TAG_META_SIZE];

    tkey_t         key;
    UT_hash_handle hh;
};

typedef struct {
    char           key[NEU_GROUP_NAME_LEN];
    void *         trace_ctx;
    UT_hash_handle hh;
} group_trace_t;

struct neu_driver_cache {
    pthread_mutex_t mtx;
    group_trace_t * trace_table;
    struct elem *   table;
};

// static void update_tag_error(neu_driver_cache_t *cache, const char *group,
// const char *tag, int64_t timestamp, int error);

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
        if (elem->value.type == NEU_TYPE_PTR) {
            if (elem->value.value.ptr.ptr != NULL) {
                free(elem->value.value.ptr.ptr);
                elem->value.value.ptr.ptr = NULL;
            }
        }
        free(elem);
    }
    pthread_mutex_unlock(&cache->mtx);

    pthread_mutex_destroy(&cache->mtx);

    free(cache);
}

// void neu_driver_cache_error(neu_driver_cache_t *cache, const char *group,
// const char *tag, int64_t timestamp, int32_t error)
//{
// update_tag_error(cache, group, tag, timestamp, error);
//}

void neu_driver_cache_add(neu_driver_cache_t *cache, const char *group,
                          const char *tag, neu_dvalue_t value)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem == NULL) {
        elem = calloc(1, sizeof(struct elem));

        strcpy(elem->key.group, group);
        strcpy(elem->key.tag, tag);

        HASH_ADD(hh, cache->table, key, sizeof(tkey_t), elem);
    }

    elem->timestamp = 0;
    elem->changed   = false;
    elem->value     = value;

    pthread_mutex_unlock(&cache->mtx);
}

void neu_driver_cache_update_trace(neu_driver_cache_t *cache, const char *group,
                                   void *trace_ctx)
{
    group_trace_t *elem                    = NULL;
    char           key[NEU_GROUP_NAME_LEN] = { 0 };

    strcpy(key, group);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->trace_table, &key, sizeof(key), elem);

    if (elem == NULL) {
        elem = calloc(1, sizeof(group_trace_t));

        strcpy(elem->key, group);
        elem->trace_ctx = trace_ctx;

        HASH_ADD(hh, cache->trace_table, key, sizeof(key), elem);
    }

    elem->trace_ctx = trace_ctx;

    pthread_mutex_unlock(&cache->mtx);
}

void *neu_driver_cache_get_trace(neu_driver_cache_t *cache, const char *group)
{
    group_trace_t *elem                    = NULL;
    char           key[NEU_GROUP_NAME_LEN] = { 0 };

    void *trace = NULL;

    strcpy(key, group);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->trace_table, &key, sizeof(key), elem);

    if (elem != NULL) {
        trace = elem->trace_ctx;
    }

    pthread_mutex_unlock(&cache->mtx);

    return trace;
}

void neu_driver_cache_update_change(neu_driver_cache_t *cache,
                                    const char *group, const char *tag,
                                    int64_t timestamp, neu_dvalue_t value,
                                    neu_tag_meta_t *metas, int n_meta,
                                    bool change)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);
    if (elem != NULL) {
        elem->timestamp = timestamp;

        if (sub_filter_err && value.type == NEU_TYPE_ERROR) {
            goto error_not_report;
        }

        if ((!sub_filter_err && elem->value.type != value.type) ||
            (sub_filter_err && elem->value.type != value.type &&
             elem->value.type != NEU_TYPE_ERROR)) {
            elem->changed = true;
        } else if (sub_filter_err && elem->value.type != value.type &&
                   elem->value.type == NEU_TYPE_ERROR) {
            switch (value.type) {
            case NEU_TYPE_INT8:
            case NEU_TYPE_UINT8:
            case NEU_TYPE_INT16:
            case NEU_TYPE_UINT16:
            case NEU_TYPE_INT32:
            case NEU_TYPE_UINT32:
            case NEU_TYPE_INT64:
            case NEU_TYPE_UINT64:
            case NEU_TYPE_BIT:
            case NEU_TYPE_BOOL:
            case NEU_TYPE_STRING:
            case NEU_TYPE_TIME:
            case NEU_TYPE_DATA_AND_TIME:
            case NEU_TYPE_WORD:
            case NEU_TYPE_DWORD:
            case NEU_TYPE_LWORD:
                if (memcmp(&elem->value_old.value, &value.value,
                           sizeof(value.value)) != 0) {
                    elem->changed = true;
                }
                break;
            case NEU_TYPE_BYTES:
                if (elem->value_old.value.bytes.length !=
                    value.value.bytes.length) {
                    elem->changed = true;
                } else {
                    if (memcpy(elem->value_old.value.bytes.bytes,
                               value.value.bytes.bytes,
                               value.value.bytes.length) != 0) {
                        elem->changed = true;
                    }
                }
                break;
            case NEU_TYPE_PTR: {
                if (elem->value_old.value.ptr.length !=
                    value.value.ptr.length) {
                    elem->changed = true;
                } else {
                    if (memcmp(elem->value_old.value.ptr.ptr,
                               value.value.ptr.ptr,
                               value.value.ptr.length) != 0) {
                        elem->changed = true;
                    }
                }

                break;
            }
            case NEU_TYPE_FLOAT:
                if (elem->value_old.precision == 0) {
                    elem->changed =
                        elem->value_old.value.f32 != value.value.f32;
                } else {
                    if (fabs(elem->value_old.value.f32 - value.value.f32) >
                        pow(0.1, elem->value_old.precision)) {
                        elem->changed = true;
                    }
                }
                break;
            case NEU_TYPE_DOUBLE:
                if (elem->value_old.precision == 0) {
                    elem->changed =
                        elem->value_old.value.d64 != value.value.d64;
                } else {
                    if (fabs(elem->value_old.value.d64 - value.value.d64) >
                        pow(0.1, elem->value_old.precision)) {
                        elem->changed = true;
                    }
                }

                break;
            case NEU_TYPE_ERROR:
                break;
            }
        } else {
            switch (value.type) {
            case NEU_TYPE_INT8:
            case NEU_TYPE_UINT8:
            case NEU_TYPE_INT16:
            case NEU_TYPE_UINT16:
            case NEU_TYPE_INT32:
            case NEU_TYPE_UINT32:
            case NEU_TYPE_INT64:
            case NEU_TYPE_UINT64:
            case NEU_TYPE_BIT:
            case NEU_TYPE_BOOL:
            case NEU_TYPE_STRING:
            case NEU_TYPE_TIME:
            case NEU_TYPE_DATA_AND_TIME:
            case NEU_TYPE_WORD:
            case NEU_TYPE_DWORD:
            case NEU_TYPE_LWORD:
                if (memcmp(&elem->value.value, &value.value,
                           sizeof(value.value)) != 0) {
                    elem->changed = true;
                }
                break;
            case NEU_TYPE_BYTES:
                if (elem->value.value.bytes.length !=
                    value.value.bytes.length) {
                    elem->changed = true;
                } else {
                    if (memcpy(elem->value.value.bytes.bytes,
                               value.value.bytes.bytes,
                               value.value.bytes.length) != 0) {
                        elem->changed = true;
                    }
                }
                break;
            case NEU_TYPE_PTR: {
                if (elem->value.value.ptr.length != value.value.ptr.length) {
                    elem->changed = true;
                } else {
                    if (memcmp(elem->value.value.ptr.ptr, value.value.ptr.ptr,
                               value.value.ptr.length) != 0) {
                        elem->changed = true;
                    }
                }

                break;
            }
            case NEU_TYPE_FLOAT:
                if (elem->value.precision == 0) {
                    elem->changed = elem->value.value.f32 != value.value.f32;
                } else {
                    if (fabs(elem->value.value.f32 - value.value.f32) >
                        pow(0.1, elem->value.precision)) {
                        elem->changed = true;
                    }
                }
                break;
            case NEU_TYPE_DOUBLE:
                if (elem->value.precision == 0) {
                    elem->changed = elem->value.value.d64 != value.value.d64;
                } else {
                    if (fabs(elem->value.value.d64 - value.value.d64) >
                        pow(0.1, elem->value.precision)) {
                        elem->changed = true;
                    }
                }

                break;
            case NEU_TYPE_ERROR:
                elem->changed = true;
                break;
            }
        }

        if (sub_filter_err && value.type != NEU_TYPE_ERROR) {
            elem->value_old.type      = value.type;
            elem->value_old.value     = value.value;
            elem->value_old.precision = value.precision;
        }

    error_not_report:

        if (change) {
            elem->changed = true;
        }

        elem->value.type = value.type;
        if (elem->value.type == NEU_TYPE_PTR) {
            elem->value.value.ptr.length = value.value.ptr.length;
            elem->value.value.ptr.type   = value.value.ptr.type;
            if (elem->value.value.ptr.ptr != NULL) {
                free(elem->value.value.ptr.ptr);
            }
            elem->value.value.ptr.ptr = calloc(1, value.value.ptr.length);
            memcpy(elem->value.value.ptr.ptr, value.value.ptr.ptr,
                   value.value.ptr.length);
        } else {
            elem->value.value = value.value;
        }

        memset(elem->metas, 0, sizeof(neu_tag_meta_t) * NEU_TAG_META_SIZE);
        for (int i = 0; i < n_meta; i++) {
            memcpy(&elem->metas[i], &metas[i], sizeof(neu_tag_meta_t));
        }
    }

    pthread_mutex_unlock(&cache->mtx);
}

void neu_driver_cache_update(neu_driver_cache_t *cache, const char *group,
                             const char *tag, int64_t timestamp,
                             neu_dvalue_t value, neu_tag_meta_t *metas,
                             int n_meta)
{
    neu_driver_cache_update_change(cache, group, tag, timestamp, value, metas,
                                   n_meta, false);
}

int neu_driver_cache_meta_get(neu_driver_cache_t *cache, const char *group,
                              const char *tag, neu_driver_cache_value_t *value,
                              neu_tag_meta_t *metas, int n_meta)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        value->timestamp       = elem->timestamp;
        value->value.type      = elem->value.type;
        value->value.precision = elem->value.precision;

        assert(n_meta <= NEU_TAG_META_SIZE);
        memcpy(metas, elem->metas, sizeof(neu_tag_meta_t) * NEU_TAG_META_SIZE);

        switch (elem->value.type) {
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
        case NEU_TYPE_BIT:
            value->value.value.u8 = elem->value.value.u8;
            break;
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
        case NEU_TYPE_WORD:
            value->value.value.u16 = elem->value.value.u16;
            break;
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_DWORD:
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_ERROR:
            value->value.value.u32 = elem->value.value.u32;
            break;
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_LWORD:
            value->value.value.u64 = elem->value.value.u64;
            break;
        case NEU_TYPE_BOOL:
            value->value.value.boolean = elem->value.value.boolean;
            break;
        case NEU_TYPE_STRING:
        case NEU_TYPE_TIME:
        case NEU_TYPE_DATA_AND_TIME:
            memcpy(value->value.value.str, elem->value.value.str,
                   sizeof(elem->value.value.str));
            break;
        case NEU_TYPE_BYTES:
            value->value.value.bytes.length = elem->value.value.bytes.length;
            memcpy(value->value.value.bytes.bytes,
                   elem->value.value.bytes.bytes,
                   elem->value.value.bytes.length);
            break;
        case NEU_TYPE_PTR:
            value->value.value.ptr.length = elem->value.value.ptr.length;
            value->value.value.ptr.type   = elem->value.value.ptr.type;
            value->value.value.ptr.ptr =
                calloc(1, elem->value.value.ptr.length);
            memcpy(value->value.value.ptr.ptr, elem->value.value.ptr.ptr,
                   elem->value.value.ptr.length);
            break;
        }

        for (int i = 0; i < NEU_TAG_META_SIZE; i++) {
            if (strlen(elem->metas[i].name) > 0) {
                memcpy(&value->metas[i], &elem->metas[i],
                       sizeof(neu_tag_meta_t));
            }
        }

        ret = 0;
    }

    pthread_mutex_unlock(&cache->mtx);

    return ret;
}

int neu_driver_cache_meta_get_changed(neu_driver_cache_t *cache,
                                      const char *group, const char *tag,
                                      neu_driver_cache_value_t *value,
                                      neu_tag_meta_t *metas, int n_meta)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL && elem->changed) {
        value->timestamp       = elem->timestamp;
        value->value.type      = elem->value.type;
        value->value.precision = elem->value.precision;

        assert(n_meta <= NEU_TAG_META_SIZE);
        memcpy(metas, elem->metas, sizeof(neu_tag_meta_t) * NEU_TAG_META_SIZE);

        switch (elem->value.type) {
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
        case NEU_TYPE_BIT:
            value->value.value.u8 = elem->value.value.u8;
            break;
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
        case NEU_TYPE_WORD:
            value->value.value.u16 = elem->value.value.u16;
            break;
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_ERROR:
        case NEU_TYPE_DWORD:
            value->value.value.u32 = elem->value.value.u32;
            break;
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_LWORD:
            value->value.value.u64 = elem->value.value.u64;
            break;
        case NEU_TYPE_BOOL:
            value->value.value.boolean = elem->value.value.boolean;
            break;
        case NEU_TYPE_STRING:
        case NEU_TYPE_TIME:
        case NEU_TYPE_DATA_AND_TIME:
            memcpy(value->value.value.str, elem->value.value.str,
                   sizeof(elem->value.value.str));
            break;
        case NEU_TYPE_BYTES:
            value->value.value.bytes.length = elem->value.value.bytes.length;
            memcpy(value->value.value.bytes.bytes,
                   elem->value.value.bytes.bytes,
                   elem->value.value.bytes.length);
            break;
        case NEU_TYPE_PTR:
            value->value.value.ptr.length = elem->value.value.ptr.length;
            value->value.value.ptr.type   = elem->value.value.ptr.type;
            value->value.value.ptr.ptr =
                calloc(1, elem->value.value.ptr.length);
            memcpy(value->value.value.ptr.ptr, elem->value.value.ptr.ptr,
                   elem->value.value.ptr.length);
            break;
        }

        for (int i = 0; i < NEU_TAG_META_SIZE; i++) {
            if (strlen(elem->metas[i].name) > 0) {
                memcpy(&value->metas[i], &elem->metas[i],
                       sizeof(neu_tag_meta_t));
            }
        }

        if (elem->value.type != NEU_TYPE_ERROR) {
            elem->changed = false;
        }
        ret = 0;
    }

    pthread_mutex_unlock(&cache->mtx);

    return ret;
}

void neu_driver_cache_del(neu_driver_cache_t *cache, const char *group,
                          const char *tag)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        HASH_DEL(cache->table, elem);
        if (elem->value.type == NEU_TYPE_PTR) {
            if (elem->value.value.ptr.ptr != NULL) {
                free(elem->value.value.ptr.ptr);
                elem->value.value.ptr.ptr = NULL;
            }
        }
        free(elem);
    }

    pthread_mutex_unlock(&cache->mtx);
}
