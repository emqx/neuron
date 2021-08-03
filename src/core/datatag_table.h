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

#include "neu_types.h"
#include "utils/idhash.h"

typedef struct datatag_table_t datatag_table_t;

datatag_table_t *datatag_tbl_create(void);
void             datatag_tbl_destroy(datatag_table_t *);
neu_datatag_t *  datatag_tbl_id_get(datatag_table_t *, uint32_t);
int              datatag_tbl_id_remove(datatag_table_t *, uint32_t);
int datatag_tbl_id_update(datatag_table_t *, uint32_t, neu_datatag_t *);
int datatag_tbl_id_add(datatag_table_t *, neu_datatag_t *);

#endif //__DATATAG_TABLE_H__