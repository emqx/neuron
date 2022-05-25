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

#ifndef _NEU_ASYNC_QUEUE_H_
#define _NEU_ASYNC_QUEUE_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct neu_async_queue neu_async_queue_t;

typedef uint64_t (*neu_async_queue_key)(void *elem);
typedef bool (*neu_async_queue_expire)(void *elem);
typedef void (*neu_async_queue_free)(void *elem);
typedef bool (*neu_async_queue_filter)(void *filter_elem, void *elem);

neu_async_queue_t *neu_async_queue_new(neu_async_queue_key    key_fn,
                                       neu_async_queue_expire expire_fn,
                                       neu_async_queue_free   free_fn,
                                       uint16_t               max_size);

void neu_async_queue_destroy(neu_async_queue_t *q);
void neu_async_queue_push(neu_async_queue_t *q, void *elem);
int  neu_async_queue_pop(neu_async_queue_t *q, uint64_t key, void **elem);

void neu_async_queue_remove(neu_async_queue_t *q, neu_async_queue_filter filter,
                            void *filter_elem);

#endif