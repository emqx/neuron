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

#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

#include "utils/uthash.h"

#include "tag_table.h"

struct tag_elem {
    neu_tag_t tag;

    char           name[NEU_TAG_NAME_LEN];
    UT_hash_handle hh;
};

struct neu_tag_table {
    pthread_mutex_t mtx;
    int64_t         timestamp;

    struct tag_elem *tags;
};

static void update_timestamp(neu_tag_table_t *tbl)
{
    struct timeval tv = { 0 };

    gettimeofday(&tv, NULL);

    tbl->timestamp = tv.tv_sec * 1000 * 1000 + tv.tv_usec;
}

neu_tag_table_t *neu_tag_table_create(void)
{
    neu_tag_table_t *table = calloc(1, sizeof(neu_tag_table_t));

    pthread_mutex_init(&table->mtx, NULL);

    return table;
}

void neu_tag_table_destory(neu_tag_table_t *tbl)
{
    struct tag_elem *elem = NULL;
    struct tag_elem *tmp  = NULL;

    pthread_mutex_lock(&tbl->mtx);
    HASH_ITER(hh, tbl->tags, elem, tmp)
    {
        HASH_DEL(tbl->tags, elem);
        free(elem);
    }
    pthread_mutex_unlock(&tbl->mtx);

    pthread_mutex_destroy(&tbl->mtx);

    free(tbl);
}

uint32_t neu_tag_table_size(neu_tag_table_t *tbl)
{
    uint32_t size = 0;

    pthread_mutex_lock(&tbl->mtx);
    size = HASH_COUNT(tbl->tags);
    pthread_mutex_unlock(&tbl->mtx);

    return size;
}

neu_tag_t *neu_tag_table_get(neu_tag_table_t *tbl, const char *name)
{
    struct tag_elem *elem = NULL;
    neu_tag_t *      tag  = NULL;

    pthread_mutex_lock(&tbl->mtx);
    HASH_FIND_STR(tbl->tags, name, elem);
    if (elem != NULL) {
        tag = calloc(1, sizeof(neu_tag_t));
        memcpy(tag, &elem->tag, sizeof(neu_tag_t));
    }
    pthread_mutex_unlock(&tbl->mtx);

    return tag;
}

int neu_tag_table_add(neu_tag_table_t *tbl, neu_tag_t *tag)
{
    int              ret  = -1;
    struct tag_elem *elem = NULL;

    pthread_mutex_lock(&tbl->mtx);
    HASH_FIND_STR(tbl->tags, tag->name, elem);
    if (elem == NULL) {
        elem = calloc(1, sizeof(struct tag_elem));

        elem->tag = *tag;
        strncpy(elem->name, tag->name, sizeof(elem->name));

        HASH_ADD_STR(tbl->tags, name, elem);

        update_timestamp(tbl);
        ret = 0;
    }
    pthread_mutex_unlock(&tbl->mtx);

    return ret;
}

int neu_tag_table_update(neu_tag_table_t *tbl, neu_tag_t *tag)
{
    int              ret  = -1;
    struct tag_elem *elem = NULL;

    pthread_mutex_lock(&tbl->mtx);
    HASH_FIND_STR(tbl->tags, tag->name, elem);
    if (elem != NULL) {
        elem->tag = *tag;

        update_timestamp(tbl);
        ret = 0;
    }
    pthread_mutex_unlock(&tbl->mtx);

    return ret;
}

int neu_tag_table_delete(neu_tag_table_t *tbl, const char *name)
{
    int              ret  = -1;
    struct tag_elem *elem = NULL;

    pthread_mutex_lock(&tbl->mtx);
    HASH_FIND_STR(tbl->tags, name, elem);
    if (elem != NULL) {
        HASH_DEL(tbl->tags, elem);

        update_timestamp(tbl);
        ret = 0;
    }
    pthread_mutex_unlock(&tbl->mtx);

    return ret;
}
void neu_tag_table_foreach(neu_tag_table_t *tbl, void *arg,
                           neu_tag_table_foreach_fn fn)
{
    struct tag_elem *elem = NULL;

    pthread_mutex_lock(&tbl->mtx);
    for (elem = tbl->tags; elem != NULL; elem = elem->hh.next) {
        fn(arg, &elem->tag);
    }
    pthread_mutex_unlock(&tbl->mtx);
}

int64_t neu_tag_table_change_foreach(neu_tag_table_t *tbl, int64_t timestamp,
                                     void *arg, neu_tag_table_foreach_fn fn)
{
    struct tag_elem *elem = NULL;

    pthread_mutex_lock(&tbl->mtx);
    if (timestamp != tbl->timestamp) {
        for (elem = tbl->tags; elem != NULL; elem = elem->hh.next) {
            fn(arg, &elem->tag);
        }
    }
    pthread_mutex_unlock(&tbl->mtx);

    return tbl->timestamp;
}