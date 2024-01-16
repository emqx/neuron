/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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
#ifndef NEURON_UTILS_SET_H
#define NEURON_UTILS_SET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdlib.h>

#include "uthash.h"

typedef struct {
    const char *   str;
    UT_hash_handle hh;
} neu_strset_node_t;

/** Set of strings.
 */
typedef neu_strset_node_t *neu_strset_t;

/** Destructs the string set.
 */
static inline void neu_strset_free(neu_strset_t *set)
{
    neu_strset_node_t *e = NULL, *tmp = NULL;
    HASH_ITER(hh, *set, e, tmp)
    {
        HASH_DEL(*set, e);
        free(e);
    }
}

/** Return the cardinal of the set.
 */
static inline unsigned neu_strset_count(neu_strset_t *set)
{
    return HASH_COUNT(*set);
}

/** Return true if the given string is a member of the set, false otherwise.
 */
static inline bool neu_strset_test(neu_strset_t *set, const char *str)
{
    neu_strset_node_t *e = NULL;
    HASH_FIND_STR(*set, str, e);
    return NULL != e;
}

/** Add the given string to the set.
 * NOTE: The given string is referenced, not copied.
 * Return 1 if success, 0 if the string is already a member, -1 on error.
 */
static inline int neu_strset_add(neu_strset_t *set, const char *str)
{
    neu_strset_node_t *e = NULL;
    HASH_FIND_STR(*set, str, e);
    if (e) {
        return 0;
    }
    e = calloc(1, sizeof(*e));
    if (NULL == e) {
        return -1;
    }
    e->str = str;
    HASH_ADD_KEYPTR(hh, *set, e->str, strlen(e->str), e);
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif
