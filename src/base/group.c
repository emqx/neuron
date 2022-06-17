/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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
#include <string.h>
#include <sys/time.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "define.h"
#include "errcodes.h"

#include "group.h"

typedef struct tag_elem {
    char *name;

    neu_datatag_t *tag;

    UT_hash_handle hh;
} tag_elem_t;

struct neu_group {
    char *name;

    tag_elem_t *tags;
    uint32_t    interval;

    nng_mtx *mtx;
    int64_t  timestamp;
};

static UT_array *to_array(tag_elem_t *tags);
static UT_array *to_read_array(tag_elem_t *tags);
static void      update_timestamp(neu_group_t *group);

neu_group_t *neu_group_new(const char *name, uint32_t interval)
{
    neu_group_t *group = calloc(1, sizeof(neu_group_t));

    group->name     = strdup(name);
    group->interval = interval;

    nng_mtx_alloc(&group->mtx);

    return group;
}

void neu_group_destroy(neu_group_t *group)
{
    tag_elem_t *el = NULL, *tmp = NULL;

    nng_mtx_lock(group->mtx);

    HASH_ITER(hh, group->tags, el, tmp)
    {
        HASH_DEL(group->tags, el);
        free(el->name);
        free(el->tag->name);
        free(el->tag->address);
        free(el->tag->description);
        free(el->tag);
        free(el);
    }

    nng_mtx_unlock(group->mtx);

    nng_mtx_free(group->mtx);

    free(group->name);
    free(group);
}

const char *neu_group_get_name(neu_group_t *group)
{
    static __thread char name[NEU_GROUP_NAME_LEN] = { 0 };

    nng_mtx_lock(group->mtx);
    strncpy(name, group->name, sizeof(name));
    nng_mtx_unlock(group->mtx);

    return name;
}

uint32_t neu_group_get_interval(neu_group_t *group)
{
    uint32_t interval = 0;

    nng_mtx_lock(group->mtx);
    interval = group->interval;
    nng_mtx_unlock(group->mtx);

    return interval;
}

int neu_group_update(neu_group_t *group, uint32_t interval)
{
    nng_mtx_lock(group->mtx);
    if (group->interval != interval) {
        group->interval = interval;
        update_timestamp(group);
    }

    nng_mtx_unlock(group->mtx);

    return 0;
}

int neu_group_add_tag(neu_group_t *group, neu_datatag_t *tag)
{
    tag_elem_t *el = NULL;

    nng_mtx_lock(group->mtx);

    HASH_FIND_STR(group->tags, tag->name, el);
    if (el != NULL) {
        nng_mtx_unlock(group->mtx);
        return NEU_ERR_TAG_NAME_CONFLICT;
    }

    el                   = calloc(1, sizeof(tag_elem_t));
    el->name             = strdup(tag->name);
    el->tag              = calloc(1, sizeof(neu_datatag_t));
    el->tag->name        = strdup(tag->name);
    el->tag->address     = strdup(tag->address);
    el->tag->description = strdup(tag->description);
    el->tag->type        = tag->type;
    el->tag->attribute   = tag->attribute;

    HASH_ADD_STR(group->tags, name, el);
    update_timestamp(group);

    nng_mtx_unlock(group->mtx);

    return 0;
}

int neu_group_update_tag(neu_group_t *group, neu_datatag_t *tag)
{
    tag_elem_t *el  = NULL;
    int         ret = NEU_ERR_TAG_NOT_EXIST;

    nng_mtx_lock(group->mtx);

    HASH_FIND_STR(group->tags, tag->name, el);
    if (el != NULL) {
        HASH_DEL(group->tags, el);
        free(el->name);
        free(el->tag->name);
        free(el->tag->address);
        free(el->tag->description);
        free(el->tag);
        free(el);

        el                   = calloc(1, sizeof(tag_elem_t));
        el->name             = strdup(tag->name);
        el->tag              = calloc(1, sizeof(neu_datatag_t));
        el->tag->name        = strdup(tag->name);
        el->tag->address     = strdup(tag->address);
        el->tag->description = strdup(tag->description);
        el->tag->type        = tag->type;
        el->tag->attribute   = tag->attribute;

        HASH_ADD_STR(group->tags, name, el);
        update_timestamp(group);
        ret = NEU_ERR_SUCCESS;
    }

    nng_mtx_unlock(group->mtx);

    return ret;
}

int neu_group_del_tag(neu_group_t *group, const char *tag_name)
{
    tag_elem_t *el  = NULL;
    int         ret = NEU_ERR_TAG_NOT_EXIST;

    nng_mtx_lock(group->mtx);

    HASH_FIND_STR(group->tags, tag_name, el);
    if (el != NULL) {
        HASH_DEL(group->tags, el);
        free(el->name);
        free(el->tag->name);
        free(el->tag->address);
        free(el->tag->description);
        free(el->tag);
        free(el);

        update_timestamp(group);
        ret = NEU_ERR_SUCCESS;
    }

    nng_mtx_unlock(group->mtx);

    return ret;
}

UT_array *neu_group_get_tag(neu_group_t *group)
{
    UT_array *array = NULL;

    nng_mtx_lock(group->mtx);
    array = to_array(group->tags);
    nng_mtx_unlock(group->mtx);

    return array;
}

UT_array *neu_group_get_read_tag(neu_group_t *group)
{
    UT_array *array = NULL;

    nng_mtx_lock(group->mtx);
    array = to_read_array(group->tags);
    nng_mtx_unlock(group->mtx);

    return array;
}

uint16_t neu_group_tag_size(neu_group_t *group)
{
    uint16_t size = 0;

    nng_mtx_lock(group->mtx);
    size = HASH_COUNT(group->tags);
    nng_mtx_unlock(group->mtx);

    return size;
}

neu_datatag_t *neu_group_find_tag(neu_group_t *group, const char *tag)
{
    tag_elem_t *   find   = NULL;
    neu_datatag_t *result = NULL;

    nng_mtx_lock(group->mtx);
    HASH_FIND_STR(group->tags, tag, find);
    if (find != NULL) {
        result              = calloc(1, sizeof(neu_datatag_t));
        result->type        = find->tag->type;
        result->attribute   = find->tag->attribute;
        result->name        = strdup(find->tag->name);
        result->address     = strdup(find->tag->address);
        result->description = strdup(find->tag->description);
    }
    nng_mtx_unlock(group->mtx);

    return result;
}

void neu_group_change_test(neu_group_t *group, int64_t timestamp, void *arg,
                           neu_group_change_fn fn)
{
    nng_mtx_lock(group->mtx);
    if (group->timestamp != timestamp) {
        UT_array *tags = to_array(group->tags);
        fn(arg, group->timestamp, tags, group->interval);
        utarray_free(tags);
    }
    nng_mtx_unlock(group->mtx);
}

bool neu_group_is_change(neu_group_t *group, int64_t timestamp)
{
    bool change = false;

    nng_mtx_lock(group->mtx);
    change = group->timestamp != timestamp;
    nng_mtx_unlock(group->mtx);

    return change;
}

static void update_timestamp(neu_group_t *group)
{
    struct timeval tv = { 0 };

    gettimeofday(&tv, NULL);

    group->timestamp = tv.tv_sec * 1000 * 1000 + tv.tv_usec;
}

static UT_array *to_array(tag_elem_t *tags)
{
    tag_elem_t *el = NULL, *tmp = NULL;
    UT_array *  array = NULL;

    utarray_new(array, neu_tag_get_icd());
    HASH_ITER(hh, tags, el, tmp) { utarray_push_back(array, el->tag); }

    return array;
}

static UT_array *to_read_array(tag_elem_t *tags)
{
    tag_elem_t *el = NULL, *tmp = NULL;
    UT_array *  array = NULL;

    utarray_new(array, neu_tag_get_icd());
    HASH_ITER(hh, tags, el, tmp)
    {
        if ((el->tag->attribute & NEU_ATTRIBUTE_READ) == NEU_ATTRIBUTE_READ) {
            utarray_push_back(array, el->tag);
        }
    }

    return array;
}
