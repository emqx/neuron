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

#include <jansson.h>

#include "utils/uthash.h"

#include "define.h"
#include "errcodes.h"
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
    bool         has_old;
    bool         configured;
    neu_type_e   config_type;
    neu_type_e   resolved_type;
    uint64_t     generation;
    uint8_t      precision;
    double       decimal;
    double       bias;

    neu_tag_meta_t *metas;
    int             n_meta;

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
    uint64_t        generation;
};

static void value_free(neu_dvalue_t *value)
{
    if (value->type == NEU_TYPE_PTR) {
        free(value->value.ptr.ptr);
    } else {
        neu_free_dvalue(value);
    }
    memset(value, 0, sizeof(*value));
}

static void value_copy(const neu_dvalue_t *src, neu_dvalue_t *dst)
{
    if (src->type >= NEU_TYPE_ARRAY_INT8 &&
        src->type <= NEU_TYPE_ARRAY_STRING && src->value.bools.length == 0) {
        *dst                   = *src;
        dst->value.bools.bools = NULL;
        return;
    }
    neu_copy_dvalue(src, dst);
    if (src->type == NEU_TYPE_PTR) {
        dst->value.ptr.ptr = calloc(1, src->value.ptr.length);
        if (src->value.ptr.length != 0) {
            memcpy(dst->value.ptr.ptr, src->value.ptr.ptr,
                   src->value.ptr.length);
        }
    }
}

static void metas_free(neu_tag_meta_t *metas, int n_meta)
{
    for (int i = 0; i < n_meta; i++) {
        value_free(&metas[i].value);
    }
    free(metas);
}

static void elem_clear(struct elem *elem)
{
    value_free(&elem->value);
    value_free(&elem->value_old);
    metas_free(elem->metas, elem->n_meta);
    elem->metas   = NULL;
    elem->n_meta  = 0;
    elem->has_old = false;
}

static bool real_type(neu_type_e type)
{
    return (type >= NEU_TYPE_INT8 && type <= NEU_TYPE_BYTES) ||
        (type >= NEU_TYPE_WORD && type <= NEU_TYPE_LWORD) ||
        (type >= NEU_TYPE_TIME && type <= NEU_TYPE_ARRAY_STRING) ||
        type == NEU_TYPE_CUSTOM;
}

static neu_type_e semantic_type(const neu_dvalue_t *value)
{
    if (value->type != NEU_TYPE_PTR) {
        return value->type;
    }
    return value->value.ptr.type == NEU_TYPE_STRING ||
            value->value.ptr.type == NEU_TYPE_BYTES
        ? value->value.ptr.type
        : NEU_TYPE_AUTO;
}

static int sample_validate(const struct elem *elem, const neu_dvalue_t *value)
{
    neu_type_e type = semantic_type(value);
    if (!real_type(type)) {
        return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
    }
    if (elem->resolved_type != NEU_TYPE_AUTO && elem->resolved_type != type) {
        return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
    }
    if ((value->type == NEU_TYPE_FLOAT && !isfinite(value->value.f32)) ||
        (value->type == NEU_TYPE_DOUBLE && !isfinite(value->value.d64))) {
        return NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE;
    }
    if (type < NEU_TYPE_INT8 || type > NEU_TYPE_DOUBLE) {
        if (elem->decimal != 0) {
            return NEU_ERR_TAG_DECIMAL_INVALID;
        }
        if (elem->bias != 0) {
            return NEU_ERR_TAG_BIAS_INVALID;
        }
        if (elem->precision != 0) {
            return NEU_ERR_TAG_PRECISION_INVALID;
        }
    }
    return NEU_ERR_SUCCESS;
}

inline static tkey_t to_key(const char *group, const char *tag)
{
    tkey_t key = { 0 };

    strncpy(key.group, group, sizeof(key.group) - 1);
    strncpy(key.tag, tag, sizeof(key.tag) - 1);

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
        elem_clear(elem);

        free(elem);
    }

    group_trace_t *elem1 = NULL;
    group_trace_t *tmp1  = NULL;

    HASH_ITER(hh, cache->trace_table, elem1, tmp1)
    {
        HASH_DEL(cache->trace_table, elem1);
        free(elem1);
    }

    pthread_mutex_unlock(&cache->mtx);

    pthread_mutex_destroy(&cache->mtx);

    free(cache);
}

static void cache_add(neu_driver_cache_t *cache, const char *group,
                      const char *tag, const neu_datatag_t *config,
                      neu_dvalue_t initial)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);
    if (elem == NULL) {
        elem      = calloc(1, sizeof(struct elem));
        elem->key = key;
        HASH_ADD(hh, cache->table, key, sizeof(tkey_t), elem);
    } else {
        elem_clear(elem);
    }

    elem->timestamp   = 0;
    elem->changed     = false;
    elem->configured  = config != NULL;
    elem->config_type = config ? config->type : NEU_TYPE_AUTO;
    elem->resolved_type =
        config && real_type(config->type) ? config->type : NEU_TYPE_AUTO;
    elem->generation = ++cache->generation;
    elem->precision  = config ? config->precision : initial.precision;
    elem->decimal    = config ? config->decimal : 0;
    elem->bias       = config ? config->bias : 0;
    if (config && config->type == NEU_TYPE_AUTO &&
        initial.type != NEU_TYPE_ERROR) {
        neu_type_e type = semantic_type(&initial);
        int        ret  = sample_validate(elem, &initial);
        if (ret == NEU_ERR_SUCCESS) {
            elem->resolved_type = type;
        } else {
            neu_free_dvalue(&initial);
            memset(&initial, 0, sizeof(initial));
            initial.type      = NEU_TYPE_ERROR;
            initial.value.i32 = ret;
        }
    }
    elem->value = initial;
    if (initial.type == NEU_TYPE_PTR) {
        value_copy(&initial, &elem->value);
    }
    if (config) {
        elem->value.precision = elem->precision;
        elem->changed         = config->type == NEU_TYPE_AUTO &&
            elem->resolved_type != NEU_TYPE_AUTO;
    }
    if (sub_filter_err && elem->value.type != NEU_TYPE_ERROR) {
        value_copy(&elem->value, &elem->value_old);
        elem->has_old = true;
    }
    pthread_mutex_unlock(&cache->mtx);
}

void neu_driver_cache_add(neu_driver_cache_t *cache, const char *group,
                          const char *tag, neu_dvalue_t value)
{
    cache_add(cache, group, tag, NULL, value);
}

void neu_driver_cache_add_tag(neu_driver_cache_t *cache, const char *group,
                              const neu_datatag_t *tag, neu_dvalue_t initial)
{
    cache_add(cache, group, tag->name, tag, initial);
}

int neu_driver_cache_type_get(neu_driver_cache_t *cache, const char *group,
                              const char *tag, neu_type_e *type,
                              uint64_t *generation)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);
    int          ret  = NEU_ERR_PLUGIN_TAG_NOT_READY;

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);
    if (elem != NULL) {
        *type       = elem->resolved_type;
        *generation = elem->generation;
        if (real_type(elem->resolved_type)) {
            ret = NEU_ERR_SUCCESS;
        }
    }
    pthread_mutex_unlock(&cache->mtx);
    return ret;
}

void neu_driver_cache_update_trace(neu_driver_cache_t *cache, const char *group,
                                   void *trace_ctx)
{
    group_trace_t *elem                    = NULL;
    char           key[NEU_GROUP_NAME_LEN] = { 0 };

    snprintf(key, sizeof(key), "%s", group);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->trace_table, &key, sizeof(key), elem);

    if (elem == NULL) {
        elem = calloc(1, sizeof(group_trace_t));

        snprintf(elem->key, sizeof(elem->key), "%s", group);
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

    snprintf(key, sizeof(key), "%s", group);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->trace_table, &key, sizeof(key), elem);

    if (elem != NULL) {
        trace = elem->trace_ctx;
    }

    pthread_mutex_unlock(&cache->mtx);

    return trace;
}

static bool value_equal(const neu_dvalue_t *previous, neu_dvalue_t *value)
{
    if (previous->type != value->type) {
        return false;
    }
    if (value->type >= NEU_TYPE_ARRAY_INT8 &&
        value->type <= NEU_TYPE_ARRAY_STRING &&
        value->value.bools.length == 0) {
        return previous->value.bools.length == 0;
    }
    if (value->type == NEU_TYPE_PTR && value->value.ptr.length == 0) {
        return previous->value.ptr.length == 0 &&
            previous->value.ptr.type == value->value.ptr.type;
    }
    switch (value->type) {
    case NEU_TYPE_STRING:
    case NEU_TYPE_TIME:
    case NEU_TYPE_DATA_AND_TIME:
        return strncmp(previous->value.str, value->value.str,
                       sizeof(value->value.str)) == 0;
    case NEU_TYPE_ARRAY_CHAR:
        return memcmp(previous->value.str, value->value.str,
                      sizeof(value->value.str)) == 0;
    case NEU_TYPE_FLOAT:
        return previous->precision == 0
            ? previous->value.f32 == value->value.f32
            : !(fabs(previous->value.f32 - value->value.f32) >
                pow(0.1, previous->precision));
    case NEU_TYPE_DOUBLE:
        return previous->precision == 0
            ? previous->value.d64 == value->value.d64
            : !(fabs(previous->value.d64 - value->value.d64) >
                pow(0.1, previous->precision));
    case NEU_TYPE_ARRAY_FLOAT:
        return previous->value.f32s.length == value->value.f32s.length &&
            (value->value.f32s.length == 0 ||
             memcmp(previous->value.f32s.f32s, value->value.f32s.f32s,
                    value->value.f32s.length * sizeof(float)) == 0);
    case NEU_TYPE_ARRAY_DOUBLE:
        return previous->value.f64s.length == value->value.f64s.length &&
            (value->value.f64s.length == 0 ||
             memcmp(previous->value.f64s.f64s, value->value.f64s.f64s,
                    value->value.f64s.length * sizeof(double)) == 0);
    case NEU_TYPE_ERROR:
        return false;
    default: {
        neu_dvalue_t other = *value;
        other.precision    = previous->precision;
        return neu_equal_dvalue(previous, &other);
    }
    }
}

int neu_driver_cache_update_checked(neu_driver_cache_t *cache,
                                    const char *group, const char *tag,
                                    int64_t timestamp, neu_dvalue_t value,
                                    neu_tag_meta_t *metas, int n_meta,
                                    bool force, bool *changed)
{
    struct elem *elem        = NULL;
    tkey_t       key         = to_key(group, tag);
    bool         tag_changed = false;
    int          ret         = NEU_ERR_SUCCESS;

    if (changed) {
        *changed = false;
    }
    if (!value.attrs.has_timestamp) {
        value.attrs.timestamp = 0;
    }
    if (!value.attrs.has_source_timestamp) {
        value.attrs.source_timestamp = 0;
    }
    if (!value.attrs.has_server_timestamp) {
        value.attrs.server_timestamp = 0;
    }
    if (!value.attrs.has_quality) {
        value.attrs.quality = 0;
    }

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);
    if (elem == NULL) {
        ret = NEU_ERR_PLUGIN_TAG_NOT_READY;
        neu_free_dvalue(&value);
        for (int i = 0; metas && i < n_meta; i++) {
            neu_free_dvalue(&metas[i].value);
        }
        goto done;
    }

    bool resolved = false;
    if (elem->configured && elem->config_type == NEU_TYPE_AUTO &&
        value.type != NEU_TYPE_ERROR) {
        neu_type_e type = semantic_type(&value);
        ret             = sample_validate(elem, &value);
        if (ret != NEU_ERR_SUCCESS) {
            neu_free_dvalue(&value);
            memset(&value, 0, sizeof(value));
            value.type      = NEU_TYPE_ERROR;
            value.value.i32 = ret;
        } else if (elem->resolved_type == NEU_TYPE_AUTO) {
            elem->resolved_type = type;
            resolved            = true;
        }
    }
    if (elem->configured) {
        value.precision = elem->precision;
    } else {
        value.precision = elem->value.precision;
    }

    elem->timestamp = timestamp;
    if (!(sub_filter_err && value.type == NEU_TYPE_ERROR)) {
        const neu_dvalue_t *previous     = &elem->value;
        bool                has_previous = true;
        if (sub_filter_err && elem->value.type == NEU_TYPE_ERROR) {
            previous     = &elem->value_old;
            has_previous = elem->has_old;
        }
        tag_changed = resolved || !has_previous ||
            !value_equal(previous, &value) ||
            previous->attrs.has_quality != value.attrs.has_quality ||
            (value.attrs.has_quality &&
             previous->attrs.quality != value.attrs.quality);
        elem->changed |= tag_changed;
    }
    if (sub_filter_err && value.type != NEU_TYPE_ERROR) {
        value_free(&elem->value_old);
        value_copy(&value, &elem->value_old);
        elem->has_old = true;
    }
    elem->changed |= force;

    neu_dvalue_t stored = value;
    if (value.type == NEU_TYPE_PTR) {
        value_copy(&value, &stored);
    }
    value_free(&elem->value);
    elem->value = stored;

    if (metas != NULL && n_meta > 0) {
        metas_free(elem->metas, elem->n_meta);
        elem->metas  = calloc(n_meta, sizeof(neu_tag_meta_t));
        elem->n_meta = n_meta;
        for (int i = 0; i < n_meta; i++) {
            if (!neu_dvalue_is_array(&metas[i].value)) {
                elem->metas[i] = metas[i];
                if (metas[i].value.type == NEU_TYPE_PTR) {
                    value_copy(&metas[i].value, &elem->metas[i].value);
                }
            } else {
                neu_free_dvalue(&metas[i].value);
                elem->metas[i].value.type = NEU_TYPE_INT8;
            }
        }
    }

done:
    pthread_mutex_unlock(&cache->mtx);
    if (changed) {
        *changed = tag_changed;
    }
    return ret;
}

bool neu_driver_cache_update_change(neu_driver_cache_t *cache,
                                    const char *group, const char *tag,
                                    int64_t timestamp, neu_dvalue_t value,
                                    neu_tag_meta_t *metas, int n_meta,
                                    bool change)
{
    bool changed = false;
    neu_driver_cache_update_checked(cache, group, tag, timestamp, value, metas,
                                    n_meta, change, &changed);
    return changed;
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
                              neu_tag_meta_t **metas, int *n_meta)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        value->resolved_type = elem->resolved_type;
        value->generation    = elem->generation;
        value->timestamp     = elem->timestamp;
        value_copy(&elem->value, &value->value);

        // assert(n_meta <= NEU_TAG_META_SIZE);
        if (elem->metas) {
            *metas = calloc(elem->n_meta, sizeof(neu_tag_meta_t));
            for (int i = 0; i < elem->n_meta; i++) {
                (*metas)[i] = elem->metas[i];
                value_copy(&elem->metas[i].value, &(*metas)[i].value);
            }
        } else {
            *metas = NULL;
        }
        *n_meta = elem->n_meta;

        ret = 0;
    }

    pthread_mutex_unlock(&cache->mtx);

    return ret;
}

int neu_driver_cache_meta_get_changed(neu_driver_cache_t *cache,
                                      const char *group, const char *tag,
                                      neu_driver_cache_value_t *value,
                                      neu_tag_meta_t **metas, int *n_meta)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL && elem->changed) {
        value->resolved_type = elem->resolved_type;
        value->generation    = elem->generation;
        value->timestamp     = elem->timestamp;
        value_copy(&elem->value, &value->value);

        // assert(n_meta <= NEU_TAG_META_SIZE);
        if (elem->metas) {
            *metas = calloc(elem->n_meta, sizeof(neu_tag_meta_t));
            for (int i = 0; i < elem->n_meta; i++) {
                (*metas)[i] = elem->metas[i];
                value_copy(&elem->metas[i].value, &(*metas)[i].value);
            }
        } else {
            *metas = NULL;
        }
        *n_meta = elem->n_meta;

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
        elem_clear(elem);
        free(elem);
    }

    pthread_mutex_unlock(&cache->mtx);
}

void neu_driver_cache_rename(neu_driver_cache_t *cache, const char *group,
                             const char *old_tag, const char *new_tag)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, old_tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        HASH_DEL(cache->table, elem);
        elem->key = to_key(group, new_tag);
        HASH_ADD(hh, cache->table, key, sizeof(tkey_t), elem);
    }

    pthread_mutex_unlock(&cache->mtx);
}
