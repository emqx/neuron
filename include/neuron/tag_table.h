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

#ifndef _NEU_TAG_TABLE_H_
#define _NEU_TAG_TABLE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "tag.h"

typedef struct neu_tag_table neu_tag_table_t;

neu_tag_table_t *neu_tag_table_create(void);
void             neu_tag_table_destory(neu_tag_table_t *tbl);
uint32_t         neu_tag_table_size(neu_tag_table_t *tbl);
neu_tag_t *      neu_tag_table_get(neu_tag_table_t *tbl, const char *name);
int              neu_tag_table_add(neu_tag_table_t *tbl, neu_tag_t *tag);
int              neu_tag_table_update(neu_tag_table_t *tbl, neu_tag_t *tag);
int              neu_tag_table_delete(neu_tag_table_t *tbl, const char *name);

typedef void (*neu_tag_table_foreach_fn)(void *arg, neu_tag_t *tag);
void    neu_tag_table_foreach(neu_tag_table_t *tbl, void *arg,
                              neu_tag_table_foreach_fn fn);
int64_t neu_tag_table_change_foreach(neu_tag_table_t *tbl, int64_t timestamp,
                                     void *arg, neu_tag_table_foreach_fn fn);

#ifdef __cplusplus
}
#endif

#endif
