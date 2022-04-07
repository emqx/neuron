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

#ifndef __DATATAG_TABLE_H__
#define __DATATAG_TABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "neu_tag.h"

typedef uint32_t                 datatag_id_t;
typedef struct neu_datatag_table neu_datatag_table_t;

neu_datatag_table_t *neu_datatag_tbl_create(void);
void                 neu_datatag_tbl_destroy(neu_datatag_table_t *);
size_t               neu_datatag_tbl_size(neu_datatag_table_t *);
neu_datatag_t *      neu_datatag_tbl_get(neu_datatag_table_t *, datatag_id_t);
neu_datatag_t *      neu_datatag_tbl_get_by_name(neu_datatag_table_t *,
                                                 const char *name);

// We take ownership of the datatag, this implies that the datatag and the
// tag name should reside in heap.
datatag_id_t neu_datatag_tbl_add(neu_datatag_table_t *, neu_datatag_t *);

// The input datatag and its name should be newly allocated in the heap,
// since we will destroy the old datatag.
int neu_datatag_tbl_update(neu_datatag_table_t *, datatag_id_t,
                           neu_datatag_t *);

// Removing a datatag will also release it.
int neu_datatag_tbl_remove(neu_datatag_table_t *, datatag_id_t);

int neu_datatag_tbl_to_vector(neu_datatag_table_t *tag_tbl,
                              vector_t **          p_datatags_vec);

#ifdef __cplusplus
}
#endif

#endif //__DATATAG_TABLE_H__
