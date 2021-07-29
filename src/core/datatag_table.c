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

#include "datatag_table.h"
#include "utils/mem_alloc.h"

struct datatag_table_t {
    neu_id_map datatag_table;
};

datatag_table_t *datatag_tbl_create(void)
{
    datatag_table_t *tag_tbl;
    tag_tbl = NEU_ALLOC_STRUCT(tag_tbl);

    if (tag_tbl != NULL) {
        neu_id_map_init(&tag_tbl->datatag_table, 0, 0);
    }

    return tag_tbl;
}

void datatag_tbl_destroy(datatag_table_t *tag_tbl)
{
    neu_id_map_fini(&tag_tbl->datatag_table);
    NEU_FREE_STRUCT(tag_tbl);
}

neu_datatag_t *datatag_tbl_id_get(datatag_table_t *tag_tbl, uint32_t tag_id)
{
    return neu_id_get(&tag_tbl->datatag_table, tag_id);
}

int datatag_tbl_id_update(
    datatag_table_t *tag_tbl, uint32_t tag_id, neu_datatag_t *datatag)
{
    return neu_id_set(&tag_tbl->datatag_table, tag_id, datatag);
}

int datatag_tbl_id_add(datatag_table_t *tag_tbl, neu_datatag_t *datatag)
{
    if (neu_id_alloc(&tag_tbl->datatag_table, &datatag->id, datatag) == 0) {
        return datatag->id;
    } else {
        return 0;
    }
}

int datatag_tbl_id_remove(datatag_table_t *tag_tbl, uint32_t tag_id)
{
    return neu_id_remove(&tag_tbl->datatag_table, tag_id);
}