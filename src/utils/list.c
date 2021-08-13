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
#include <string.h>

#include <neu_panic.h>

#include "list.h"

// Linked list implementation.  We implement a doubly linked list.
// Using pointer arithmetic, we can operate as a list of "anything".

#define NODE(list, item) \
    (neu_list_node *) (void *)(((char *) item) + list->ll_offset)
#define ITEM(list, node) (void *) (((char *) node) - list->ll_offset)

void neu_list_init_offset(neu_list *list, size_t offset)
{
    list->ll_offset       = offset;
    list->ll_head.ln_next = &list->ll_head;
    list->ll_head.ln_prev = &list->ll_head;
}

void *neu_list_first(const neu_list *list)
{
    neu_list_node *node = list->ll_head.ln_next;

    if (node == &list->ll_head) {
        return (NULL);
    }
    return (ITEM(list, node));
}

void *neu_list_last(const neu_list *list)
{
    neu_list_node *node = list->ll_head.ln_prev;

    if (node == &list->ll_head) {
        return (NULL);
    }
    return (ITEM(list, node));
}

void neu_list_append(neu_list *list, void *item)
{
    neu_list_node *node = NODE(list, item);

    if ((node->ln_next != NULL) || (node->ln_prev != NULL)) {
        neu_panic("appending node already on a list or not inited");
    }
    node->ln_prev          = list->ll_head.ln_prev;
    node->ln_next          = &list->ll_head;
    node->ln_next->ln_prev = node;
    node->ln_prev->ln_next = node;
}

void neu_list_prepend(neu_list *list, void *item)
{
    neu_list_node *node = NODE(list, item);

    if ((node->ln_next != NULL) || (node->ln_prev != NULL)) {
        neu_panic("prepending node already on a list or not inited");
    }
    node->ln_next          = list->ll_head.ln_next;
    node->ln_prev          = &list->ll_head;
    node->ln_next->ln_prev = node;
    node->ln_prev->ln_next = node;
}

void neu_list_insert_before(neu_list *list, void *item, void *before)
{
    neu_list_node *node  = NODE(list, item);
    neu_list_node *where = NODE(list, before);

    if ((node->ln_next != NULL) || (node->ln_prev != NULL)) {
        neu_panic("inserting node already on a list or not inited");
    }
    node->ln_next          = where;
    node->ln_prev          = where->ln_prev;
    node->ln_next->ln_prev = node;
    node->ln_prev->ln_next = node;
}

void neu_list_insert_after(neu_list *list, void *item, void *after)
{
    neu_list_node *node  = NODE(list, item);
    neu_list_node *where = NODE(list, after);

    if ((node->ln_next != NULL) || (node->ln_prev != NULL)) {
        abort();
    }
    node->ln_prev          = where;
    node->ln_next          = where->ln_next;
    node->ln_next->ln_prev = node;
    node->ln_prev->ln_next = node;
}

void *neu_list_next(const neu_list *list, void *item)
{
    neu_list_node *node = NODE(list, item);

    if (((node = node->ln_next) == &list->ll_head) || (node == NULL)) {
        return (NULL);
    }
    return (ITEM(list, node));
}

void *neu_list_prev(const neu_list *list, void *item)
{
    neu_list_node *node = NODE(list, item);

    if (((node = node->ln_prev) == &list->ll_head) || (node == NULL)) {
        return (NULL);
    }
    return (ITEM(list, node));
}

void neu_list_remove(neu_list *list, void *item)
{
    neu_list_node *node = NODE(list, item);

    node->ln_prev->ln_next = node->ln_next;
    node->ln_next->ln_prev = node->ln_prev;
    node->ln_next          = NULL;
    node->ln_prev          = NULL;
}

int neu_list_active(neu_list *list, void *item)
{
    neu_list_node *node = NODE(list, item);

    return (node->ln_next == NULL ? 0 : 1);
}

int neu_list_empty(neu_list *list)
{
    // The first check ensures that we treat an uninitialized list
    // as empty.  This use useful for statically initialized lists.
    return ((list->ll_head.ln_next == NULL) ||
            (list->ll_head.ln_next == &list->ll_head));
}

int neu_list_node_active(neu_list_node *node)
{
    return (node->ln_next == NULL ? 0 : 1);
}

void neu_list_node_remove(neu_list_node *node)
{
    if (node->ln_next != NULL) {
        node->ln_prev->ln_next = node->ln_next;
        node->ln_next->ln_prev = node->ln_prev;
        node->ln_next          = NULL;
        node->ln_prev          = NULL;
    }
}