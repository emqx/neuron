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

#ifndef _NEU_GROUP_H_
#define _NEU_GROUP_H_

#include <stdint.h>

#include "utils/utextend.h"

#include "tag.h"

typedef struct neu_group neu_group_t;

neu_group_t *neu_group_new(const char *name, uint32_t interval);
const char * neu_group_get_name(const neu_group_t *group);
int          neu_group_set_name(neu_group_t *group, const char *name);
uint32_t     neu_group_get_interval(const neu_group_t *group);
void         neu_group_set_interval(neu_group_t *group, uint32_t interval);
void         neu_group_destroy(neu_group_t *group);
int          neu_group_update(neu_group_t *group, uint32_t interval);
int          neu_group_add_tag(neu_group_t *group, const neu_datatag_t *tag);
int          neu_group_update_tag(neu_group_t *group, const neu_datatag_t *tag);
int          neu_group_del_tag(neu_group_t *group, const char *tag_name);
UT_array *   neu_group_get_tag(neu_group_t *group);
UT_array *   neu_group_query_tag(neu_group_t *group, const char *name);
UT_array *   neu_group_get_read_tag(neu_group_t *group);
UT_array *   neu_group_query_read_tag(neu_group_t *group, const char *name,
                                      const char *desc);
UT_array *   neu_group_query_read_tag_paginate(neu_group_t *group,
                                               const char *name, const char *desc,
                                               int current_page, int page_size,
                                               int *total_count);
uint16_t     neu_group_tag_size(const neu_group_t *group);
neu_datatag_t *neu_group_find_tag(neu_group_t *group, const char *tag);

typedef void (*neu_group_change_fn)(void *arg, int64_t timestamp,
                                    UT_array *tags, uint32_t interval);
void neu_group_change_test(neu_group_t *group, int64_t timestamp, void *arg,
                           neu_group_change_fn fn);
bool neu_group_is_change(neu_group_t *group, int64_t timestamp);
#endif