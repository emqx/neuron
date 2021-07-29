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

void datatag_tbl_create(neu_id_map *tag_tbl)
{
    neu_id_map_init(tag_tbl, 0, 0);
}

void datatag_tbl_destroy(neu_id_map *tag_tbl)
{
    neu_id_map_fini(tag_tbl);
}

neu_datatag_t *datatag_tbl_id_get(neu_id_map *tag_tbl, uint32_t tag_id)
{
    return neu_id_get(tag_tbl, tag_id);
}

int datatag_tbl_id_update(
    neu_id_map *tag_tbl, uint32_t tag_id, neu_datatag_t *datatag)
{
    return neu_id_set(tag_tbl, tag_id, datatag);
}

int datatag_tbl_id_add(neu_id_map *tag_tbl, neu_datatag_t *datatag)
{
    return neu_id_alloc(tag_tbl, &datatag->id, datatag);
}

int datatag_tbl_id_remove(neu_id_map *tag_tbl, uint32_t tag_id)
{
    return neu_id_remove(tag_tbl, tag_id);
}