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

#include "vector.h"

typedef struct Vector   vector_t;
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

#ifdef __cplusplus
}
#endif

#endif
