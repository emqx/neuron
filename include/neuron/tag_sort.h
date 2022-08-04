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
config_ **/

#ifndef _NEU_TAG_SORT_H_
#define _NEU_TAG_SORT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "utils/utarray.h"
#include "utils/utlist.h"

typedef struct neu_tag_sort_elem {
    void *                    tag;
    struct neu_tag_sort_elem *next, *prev;
} neu_tag_sort_elem_t;

typedef int (*neu_tag_sort_cmp)(neu_tag_sort_elem_t *tag1,
                                neu_tag_sort_elem_t *tag2);

typedef struct {
    struct {
        uint16_t size;
        void *   context;
    } info;

    UT_array *tags;
} neu_tag_sort_t;

typedef struct {
    uint16_t        n_sort;
    neu_tag_sort_t *sorts;
} neu_tag_sort_result_t;

typedef bool (*neu_tag_sort_fn)(neu_tag_sort_t *sort, void *tag,
                                void *tag_to_be_sorted);

/**
 * @brief Use sort and cmp to sort and classify tags.
 *
 * @param[in] tags The tags that needs to be processed.
 * @param[in] sort Function for tag sort.
 * @param[in] cmp Function for tags comparison.
 * @return processed tags.
 */
neu_tag_sort_result_t *neu_tag_sort(UT_array *tags, neu_tag_sort_fn sort,
                                    neu_tag_sort_cmp cmp);

/**
 * @brief free the result.
 *
 * @param[in] result the tags result that needs to be relesed.
 */
void neu_tag_sort_free(neu_tag_sort_result_t *result);

#ifdef __cplusplus
}
#endif

#endif