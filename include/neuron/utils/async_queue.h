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

/**
 * @brief Create a new asynchronous queue.
 *
 * @param[in] key_fn Calculate the key of the element.
 * @param[in] expire_fn Element invalidation check function in queue.
 * @param[in] free_fn Release expired elements and elements that exceed the
 * queue length.
 * @param[in] max_size Maximum length of the queue.
 * @return a created async queue.
 */
neu_async_queue_t *neu_async_queue_new(neu_async_queue_key    key_fn,
                                       neu_async_queue_expire expire_fn,
                                       neu_async_queue_free   free_fn,
                                       uint16_t               max_size);

/**
 * @brief destroy queue.
 *
 * @param[in] q the queue that needs to be destroyed.
 */
void neu_async_queue_destroy(neu_async_queue_t *q);

/**
 * @brief Add element at the end of the queue.
 *
 * @param[in] q the queue that requires an operation.
 * @param[in] elem that needs to be added at the end of the queue.
 */
void neu_async_queue_push(neu_async_queue_t *q, void *elem);

/**
 * @brief Pop the element corrseponding to key in the queue.
 *
 * @param[in] q the queue that requires an operation.
 * @param[in] key
 * @param[out] elem that needs to be deleted at the end of the queue.
 * @return 0 for success, -1 for failure.
 */
int neu_async_queue_pop(neu_async_queue_t *q, uint64_t key, void **elem);

/**
 * @brief remove some elements from the queue.
 *
 * @param[in] q the queue that requires an operation.
 * @param[in] filter element comparison function.
 * @param[in] filter_elem
 */
void neu_async_queue_remove(neu_async_queue_t *q, neu_async_queue_filter filter,
                            void *filter_elem);

/**
 * @brief remove all elements from the queue.
 *
 * @param[in] q the queue that requires an operation.
 */
void neu_async_queue_clean(neu_async_queue_t *q);

int neu_async_queue_len(neu_async_queue_t *q);

#endif