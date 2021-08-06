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

#include "neu_datatag_table.h"
#include "utils/mem_alloc.h"
#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

struct neu_datatag_table {
    nng_mtx *  mtx;
    neu_id_map neu_datatag_table;
};

neu_datatag_table_t *neu_datatag_tbl_create(void)
{
    neu_datatag_table_t *tag_tbl;

    tag_tbl = NEU_ALLOC_STRUCT(tag_tbl);
    if (tag_tbl == NULL) {
        return NULL;
    }

    if (nng_mtx_alloc(&tag_tbl->mtx) != 0) {
        NEU_FREE_STRUCT(tag_tbl);
        return NULL;
    }

    neu_id_map_init(&tag_tbl->neu_datatag_table, 0, 0);
    return tag_tbl;
}

void neu_datatag_tbl_destroy(neu_datatag_table_t *tag_tbl)
{
    // TODO: free all datatag in datatag table
    neu_id_map_fini(&tag_tbl->neu_datatag_table);
    nng_mtx_free(tag_tbl->mtx);
    NEU_FREE_STRUCT(tag_tbl);
    return;
}

neu_datatag_t *neu_datatag_tbl_get(neu_datatag_table_t *tag_tbl,
                                   uint32_t             tag_id)
{
    neu_datatag_t *datatag;

    nng_mtx_lock(tag_tbl->mtx);
    datatag = neu_id_get(&tag_tbl->neu_datatag_table, tag_id);
    nng_mtx_unlock(tag_tbl->mtx);
    return datatag;
}

int neu_datatag_tbl_update(neu_datatag_table_t *tag_tbl, uint32_t tag_id,
                           neu_datatag_t *datatag)
{
    int rv;

    nng_mtx_lock(tag_tbl->mtx);
    rv = neu_id_set(&tag_tbl->neu_datatag_table, tag_id, datatag);
    nng_mtx_unlock(tag_tbl->mtx);
    return rv;
}

int neu_datatag_tbl_add(neu_datatag_table_t *tag_tbl, neu_datatag_t *datatag)
{
    int id = 0;

    nng_mtx_lock(tag_tbl->mtx);
    if (neu_id_alloc(&tag_tbl->neu_datatag_table, &datatag->id, datatag) == 0) {
        id = datatag->id;
    }
    nng_mtx_unlock(tag_tbl->mtx);
    return id;
}

int neu_datatag_tbl_remove(neu_datatag_table_t *tag_tbl, uint32_t tag_id)
{
    int rv;

    nng_mtx_lock(tag_tbl->mtx);
    rv = neu_id_remove(&tag_tbl->neu_datatag_table, tag_id);
    nng_mtx_unlock(tag_tbl->mtx);
    return rv;
}
