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
config_ **/

#ifndef _NEU_TAG_CLASS_H_
#define _NEU_TAG_CLASS_H_

#include <stdbool.h>
#include <stdint.h>

#include "utils/utarray.h"
#include "utils/utlist.h"

typedef struct neu_tag_class_el {
    void *                   tag;
    struct neu_tag_class_el *next, *prev;
} neu_tag_class_el;

typedef int (*neu_tag_el_sort_fn)(neu_tag_class_el *tag1,
                                  neu_tag_class_el *tag2);

typedef struct neu_tag_class_info {
    uint16_t size;
    void *   context;
} neu_tag_class_info_t;

typedef struct {
    neu_tag_class_info_t info;
    UT_array *           tags;
} neu_tag_class_t;

typedef struct {
    uint16_t n_class;
    neu_tag_class_t *class;
} neu_tag_class_result_t;

typedef bool (*neu_tag_class_fn)(neu_tag_class_info_t *info, void *tag,
                                 void *tag_to_be_classified);

neu_tag_class_result_t *neu_tag_class(UT_array *tags, neu_tag_class_fn class_fn,
                                      neu_tag_el_sort_fn sort_fn);

void neu_tag_class_free(neu_tag_class_result_t *result);

#endif