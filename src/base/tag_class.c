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
#include <stdlib.h>

#include "tag_class.h"

static neu_tag_class_el *array_to_list(UT_array *tags);
static void              tag_class(neu_tag_class_result_t *result, void *tag,
                                   neu_tag_class_fn fn, UT_icd icd);

neu_tag_class_result_t *neu_tag_class(UT_array *tags, neu_tag_class_fn class_fn,
                                      neu_tag_el_sort_fn sort_fn)
{
    neu_tag_class_result_t *result = calloc(1, sizeof(neu_tag_class_result_t));
    neu_tag_class_el *      elt = NULL, *tmp = NULL;
    neu_tag_class_el *      head = array_to_list(tags);

    DL_SORT(head, sort_fn);
    DL_FOREACH_SAFE(head, elt, tmp)
    {
        tag_class(result, elt->tag, class_fn, tags->icd);
        DL_DELETE(head, elt);
        free(elt);
    }

    return result;
}

void neu_tag_class_free(neu_tag_class_result_t *result)
{
    for (uint16_t i = 0; i < result->n_class; i++) {
        utarray_free(result->class[i].tags);
    }

    free(result->class);
    free(result);
}

static neu_tag_class_el *array_to_list(UT_array *tags)
{
    neu_tag_class_el *head = NULL, *tmp = NULL;

    for (void **tag = utarray_front(tags); tag != NULL;
         tag        = utarray_next(tags, tag)) {
        tmp      = calloc(1, sizeof(neu_tag_class_el));
        tmp->tag = *tag;
        DL_APPEND(head, tmp);
    }

    return head;
}

static void tag_class(neu_tag_class_result_t *result, void *tag,
                      neu_tag_class_fn fn, UT_icd icd)
{
    bool classified = false;

    for (uint16_t i = 0; i < result->n_class; i++) {
        if (fn(&result->class[i].info,
               *(void **) utarray_back(result->class[i].tags), tag)) {
            utarray_push_back(result->class[i].tags, &tag);
            result->class[i].info.size += 1;
            classified = true;
            break;
        }
    }

    if (!classified) {
        result->n_class += 1;
        result->class =
            realloc(result->class, sizeof(neu_tag_class_t) * result->n_class);

        memset(&result->class[result->n_class - 1], 0, sizeof(neu_tag_class_t));
        utarray_new(result->class[result->n_class - 1].tags, &icd);
        utarray_push_back(result->class[result->n_class - 1].tags, &tag);
        result->class[result->n_class - 1].info.size = 1;
    }
}