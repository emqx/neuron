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

#ifndef __NEU_LIST_H__
#define __NEU_LIST_H__

#include <stdlib.h>

// In order to make life easy, we just define the list structures
// directly, and let consumers directly inline structures.
typedef struct neu_list_node {
    struct neu_list_node *ln_next;
    struct neu_list_node *ln_prev;
} neu_list_node;

typedef struct neu_list {
    struct neu_list_node ll_head;
    size_t               ll_offset;
} neu_list;

extern void neu_list_init_offset(neu_list *list, size_t offset);

#define NEU_LIST_INIT(list, type, field) \
    neu_list_init_offset(list, offsetof(type, field))

#define NEU_LIST_NODE_INIT(node)               \
    do {                                       \
        (node)->ln_prev = (node)->ln_next = 0; \
    } while (0)

extern void *neu_list_first(const neu_list *);
extern void *neu_list_last(const neu_list *);
extern void  neu_list_append(neu_list *, void *);
extern void  neu_list_prepend(neu_list *, void *);
extern void  neu_list_insert_before(neu_list *, void *, void *);
extern void  neu_list_insert_after(neu_list *, void *, void *);
extern void *neu_list_next(const neu_list *, void *);
extern void *neu_list_prev(const neu_list *, void *);
extern void  neu_list_remove(neu_list *, void *);
extern int   neu_list_active(neu_list *, void *);
extern int   neu_list_empty(neu_list *);
extern int   neu_list_node_active(neu_list_node *);
extern void  neu_list_node_remove(neu_list_node *);

#define NEU_LIST_FOREACH(l, it) \
    for (it = neu_list_first(l); it != NULL; it = neu_list_next(l, it))

#endif // __NEU_LIST_H__