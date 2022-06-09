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
#include <stdlib.h>

#include "tag_sort.h"

static neu_tag_sort_elem_t *array_to_list(UT_array *tags);
static void                 tag_sort(neu_tag_sort_result_t *result, void *tag,
                                     neu_tag_sort_fn fn, UT_icd icd);

neu_tag_sort_result_t *neu_tag_sort(UT_array *tags, neu_tag_sort_fn sort,
                                    neu_tag_sort_cmp cmp)
{
    neu_tag_sort_result_t *result = calloc(1, sizeof(neu_tag_sort_result_t));
    neu_tag_sort_elem_t *  elt = NULL, *tmp = NULL;
    neu_tag_sort_elem_t *  head = array_to_list(tags);

    DL_SORT(head, cmp);
    DL_FOREACH_SAFE(head, elt, tmp)
    {
        tag_sort(result, elt->tag, sort, tags->icd);
        DL_DELETE(head, elt);
        free(elt);
    }

    return result;
}

void neu_tag_sort_free(neu_tag_sort_result_t *result)
{
    for (uint16_t i = 0; i < result->n_sort; i++) {
        utarray_free(result->sorts[i].tags);
    }

    free(result->sorts);
    free(result);
}

static neu_tag_sort_elem_t *array_to_list(UT_array *tags)
{
    neu_tag_sort_elem_t *head = NULL, *tmp = NULL;

    for (void **tag = utarray_front(tags); tag != NULL;
         tag        = utarray_next(tags, tag)) {
        tmp      = calloc(1, sizeof(neu_tag_sort_elem_t));
        tmp->tag = *tag;
        DL_APPEND(head, tmp);
    }

    return head;
}

static void tag_sort(neu_tag_sort_result_t *result, void *tag,
                     neu_tag_sort_fn fn, UT_icd icd)
{
    bool sorted = false;

    for (uint16_t i = 0; i < result->n_sort; i++) {
        if (fn(&result->sorts[i],
               *(void **) utarray_back(result->sorts[i].tags), tag)) {
            utarray_push_back(result->sorts[i].tags, &tag);
            result->sorts[i].info.size += 1;
            sorted = true;
            break;
        }
    }

    if (!sorted) {
        result->n_sort += 1;
        result->sorts =
            realloc(result->sorts, sizeof(neu_tag_sort_t) * result->n_sort);

        memset(&result->sorts[result->n_sort - 1], 0, sizeof(neu_tag_sort_t));
        utarray_new(result->sorts[result->n_sort - 1].tags, &icd);
        utarray_push_back(result->sorts[result->n_sort - 1].tags, &tag);
        result->sorts[result->n_sort - 1].info.size = 1;
        fn(&result->sorts[result->n_sort - 1],
           *(void **) utarray_back(result->sorts[result->n_sort - 1].tags),
           tag);
    }
}