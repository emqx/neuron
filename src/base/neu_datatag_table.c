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

#include "hash_table.h"
#include "idhash.h"
#include "mem_alloc.h"
#include "neu_datatag_table.h"

struct neu_datatag_table {
    pthread_mutex_t mtx;
    neu_id_map      datatag_table;
    neu_hash_table  tag_name_table; // take ownership of datatags
};

neu_datatag_table_t *neu_datatag_tbl_create(void)
{
    neu_datatag_table_t *tag_tbl;

    tag_tbl = NEU_ALLOC_STRUCT(tag_tbl);
    if (tag_tbl == NULL) {
        return NULL;
    }

    if (pthread_mutex_init(&tag_tbl->mtx, NULL) != 0) {
        NEU_FREE_STRUCT(tag_tbl);
        return NULL;
    }

    neu_id_map_init(&tag_tbl->datatag_table, 0, 0);
    neu_hash_table_init(&tag_tbl->tag_name_table, neu_hash_cstr,
                        (neu_hash_table_free_cb) neu_datatag_free);
    return tag_tbl;
}

void neu_datatag_tbl_destroy(neu_datatag_table_t *tag_tbl)
{
    neu_id_map_fini(&tag_tbl->datatag_table);
    // release all tags and its name
    neu_hash_table_fini(&tag_tbl->tag_name_table);
    pthread_mutex_destroy(&tag_tbl->mtx);
    NEU_FREE_STRUCT(tag_tbl);
    return;
}

neu_datatag_t *neu_datatag_tbl_get(neu_datatag_table_t *tag_tbl,
                                   datatag_id_t         tag_id)
{
    neu_datatag_t *datatag;

    pthread_mutex_lock(&tag_tbl->mtx);
    datatag = neu_id_get(&tag_tbl->datatag_table, tag_id);
    pthread_mutex_unlock(&tag_tbl->mtx);
    return datatag;
}

int neu_datatag_tbl_update(neu_datatag_table_t *tag_tbl, datatag_id_t tag_id,
                           neu_datatag_t *datatag)
{
    int rv;

    // not a valid id
    if (0 == tag_id) {
        return -1;
    }

    pthread_mutex_lock(&tag_tbl->mtx);

    neu_datatag_t *old = neu_id_get(&tag_tbl->datatag_table, tag_id);

    // the datatag and its name should be newly allocated
    if (NULL == old || old == datatag || old->name == datatag->name) {
        pthread_mutex_unlock(&tag_tbl->mtx);
        return -1;
    }

    bool is_new_name = (0 != strcmp(datatag->name, old->name));

    if (is_new_name &&
        NULL != neu_hash_table_get(&tag_tbl->tag_name_table, datatag->name)) {
        pthread_mutex_unlock(&tag_tbl->mtx);
        return -1;
    }

    rv = neu_id_set(&tag_tbl->datatag_table, tag_id, datatag);
    if (0 == rv) {
        // release old tag and name
        neu_hash_table_remove(&tag_tbl->tag_name_table, old->name);
        neu_hash_table_set(&tag_tbl->tag_name_table, datatag->name, datatag);
    }

    pthread_mutex_unlock(&tag_tbl->mtx);
    return rv;
}

datatag_id_t neu_datatag_tbl_add(neu_datatag_table_t *tag_tbl,
                                 neu_datatag_t *      datatag)
{
    datatag_id_t id = 0;

    pthread_mutex_lock(&tag_tbl->mtx);
    if (NULL == neu_hash_table_get(&tag_tbl->tag_name_table, datatag->name) &&
        neu_id_alloc(&tag_tbl->datatag_table, &datatag->id, datatag) == 0) {
        id = datatag->id;
        neu_hash_table_set(&tag_tbl->tag_name_table, datatag->name, datatag);
    }
    pthread_mutex_unlock(&tag_tbl->mtx);
    return id;
}

int neu_datatag_tbl_remove(neu_datatag_table_t *tag_tbl, datatag_id_t tag_id)
{
    int            rv;
    neu_datatag_t *datatag = NULL;

    pthread_mutex_lock(&tag_tbl->mtx);
    datatag = neu_id_get(&tag_tbl->datatag_table, tag_id);
    if (NULL != datatag) {
        neu_id_remove(&tag_tbl->datatag_table, tag_id);
        // release tag and its name
        rv = neu_hash_table_remove(&tag_tbl->tag_name_table, datatag->name);
    }
    pthread_mutex_unlock(&tag_tbl->mtx);
    return rv;
}
