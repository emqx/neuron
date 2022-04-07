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

#ifndef NEU_VECTOR_H
#define NEU_VECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include "types.h"
#include "vector.h"

typedef struct Vector   vector_t;
typedef struct Vector   neu_vector_t;
typedef struct Iterator iterator_t;

#define vector_init vector_setup
#define vector_uninit vector_destroy

static inline vector_t *vector_new(size_t capacity, size_t elem_size)
{
    vector_t *vec;

    vec = (vector_t *) malloc(sizeof(vector_t));
    if (vec != NULL) {
        vector_setup(vec, capacity, elem_size);
    }

    return vec;
}

static inline void vector_free(vector_t *vec)
{
    if (vec != NULL) {
        vector_destroy(vec);
        free(vec);
    }
    return;
}

// vec should point to an uninitialised vector
static inline void vector_init_move_from_buf(vector_t *vec, void *data,
                                             size_t capacity, size_t size,
                                             size_t element_size)
{
    vec->size         = size;
    vec->capacity     = capacity;
    vec->element_size = element_size;
    vec->data         = data;
}

static inline vector_t *vector_new_move_from_buf(void *data, size_t capacity,
                                                 size_t size,
                                                 size_t element_size)
{
    vector_t *vec = (vector_t *) malloc(sizeof(vector_t));
    if (vec != NULL) {
        vector_init_move_from_buf(vec, data, capacity, size, element_size);
    }

    return vec;
}

static inline vector_t *vector_clone(vector_t *vec)
{
    vector_t *new_vec;

    new_vec = (vector_t *) malloc(sizeof(vector_t));
    if (new_vec == NULL) {
        return NULL;
    }

    new_vec->data = NULL;
    vector_copy(new_vec, vec);
    return new_vec;
}

static inline size_t vector_find_index(vector_t *vec, const void *key,
                                       neu_key_match_func match_func)
{
    const void *item;

    VECTOR_FOR_EACH(vec, iter)
    {
        item = (const void *) iterator_get(&iter);
        if (match_func(key, item)) {
            return iterator_index(vec, &iter);
        }
    }

    return SIZE_MAX;
}

static inline void *vector_find_item(vector_t *vec, const void *key,
                                     neu_key_match_func match_func)
{
    const void *item;

    VECTOR_FOR_EACH(vec, iter)
    {
        item = (const void *) iterator_get(&iter);
        if (match_func(key, item)) {
            return (void *) item;
        }
    }

    return NULL;
}

static inline bool vector_has_elem(vector_t *vec, const void *elem,
                                   neu_equal_func equ_func)
{
    const void *item;

    VECTOR_FOR_EACH(vec, iter)
    {
        item = (const void *) iterator_get(&iter);
        if (equ_func(elem, item)) {
            return true;
        }
    }

    return false;
}

#ifdef __cplusplus
}
#endif

#endif
