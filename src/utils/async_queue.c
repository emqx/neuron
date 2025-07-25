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

#include <pthread.h>
#include <stdlib.h>

#include "utils/async_queue.h"
#include "utils/utextend.h"

typedef struct element {
    uint64_t key;

    void *data;

    struct element *prev;
    struct element *next;
} element, element_list;

struct neu_async_queue {
    neu_async_queue_key    key_fn;
    neu_async_queue_expire expire_fn;
    neu_async_queue_free   free_fn;

    pthread_mutex_t mtx;

    uint16_t max;

    element_list *list;
};

/**
 * @brief 创建一个新的异步队列
 *
 * @param key_fn 生成元素键值的回调函数
 * @param expire_fn 检查元素是否过期的回调函数
 * @param free_fn 释放元素的回调函数
 * @param max_size 队列的最大容量
 * @return neu_async_queue_t* 返回创建的异步队列对象
 */
neu_async_queue_t *neu_async_queue_new(neu_async_queue_key    key_fn,
                                       neu_async_queue_expire expire_fn,
                                       neu_async_queue_free   free_fn,
                                       uint16_t               max_size)
{
    neu_async_queue_t *q = calloc(1, sizeof(neu_async_queue_t));

    q->key_fn    = key_fn;
    q->expire_fn = expire_fn;
    q->free_fn   = free_fn;

    q->max = max_size;

    pthread_mutex_init(&q->mtx, NULL);

    return q;
}

/**
 * @brief 销毁异步队列并释放所有资源
 *
 * @param q 要销毁的异步队列
 */
void neu_async_queue_destroy(neu_async_queue_t *q)
{
    element *elt = NULL, *tmp = NULL;

    pthread_mutex_lock(&q->mtx);
    DL_FOREACH_SAFE(q->list, elt, tmp)
    {
        q->free_fn(elt->data);
        DL_DELETE(q->list, elt);
        free(elt);
    }
    pthread_mutex_unlock(&q->mtx);

    pthread_mutex_destroy(&q->mtx);
    free(q);
}

/**
 * @brief 向异步队列中添加元素
 *
 * @param q 目标异步队列
 * @param elem 要添加的元素
 */
void neu_async_queue_push(neu_async_queue_t *q, void *elem)
{
    element *elt   = NULL;
    int      count = 0;

    pthread_mutex_lock(&q->mtx);
    DL_COUNT(q->list, elt, count);

    if (count == q->max && count > 0) {
        elt = q->list;
        DL_DELETE(q->list, elt);
        q->free_fn(elt->data);
        free(elt);
    }

    elt       = calloc(1, sizeof(element));
    elt->data = elem;
    elt->key  = q->key_fn(elem);

    DL_APPEND(q->list, elt);

    pthread_mutex_unlock(&q->mtx);
}

/**
 * @brief 从异步队列中取出指定键值的元素
 *
 * @param q 目标异步队列
 * @param key 要查找的元素键值
 * @param elem 用于存储取出元素的指针
 * @return int 成功取出返回0，失败返回-1
 */
int neu_async_queue_pop(neu_async_queue_t *q, uint64_t key, void **elem)
{
    element *elt = NULL, *tmp = NULL;
    int      ret = -1;

    pthread_mutex_lock(&q->mtx);
    DL_FOREACH_SAFE(q->list, elt, tmp)
    {
        if (elt->key == key) {
            DL_DELETE(q->list, elt);
            *elem = elt->data;
            free(elt);
            ret = 0;
            break;
        } else {
            if (q->expire_fn(elt->data)) {
                DL_DELETE(q->list, elt);
                q->free_fn(elt->data);
                free(elt);
            }
        }
    }
    pthread_mutex_unlock(&q->mtx);

    return ret;
}

/**
 * @brief 根据过滤条件移除队列中的元素
 *
 * @param q 目标异步队列
 * @param filter 过滤函数，返回true的元素将被移除
 * @param filter_elem 传递给过滤函数的参数
 */
void neu_async_queue_remove(neu_async_queue_t *q, neu_async_queue_filter filter,
                            void *filter_elem)
{
    element *el = NULL, *tmp = NULL;

    pthread_mutex_lock(&q->mtx);
    DL_FOREACH_SAFE(q->list, el, tmp)
    {
        if (filter(filter_elem, el->data)) {
            DL_DELETE(q->list, el);
            q->free_fn(el->data);
            free(el);
        }
    }
    pthread_mutex_unlock(&q->mtx);
}

/**
 * @brief 清空异步队列中的所有元素
 *
 * @param q 要清空的异步队列
 */
void neu_async_queue_clean(neu_async_queue_t *q)
{
    element *el = NULL, *tmp = NULL;

    pthread_mutex_lock(&q->mtx);
    DL_FOREACH_SAFE(q->list, el, tmp)
    {
        DL_DELETE(q->list, el);
        q->free_fn(el->data);
        free(el);
    }
    pthread_mutex_unlock(&q->mtx);
}