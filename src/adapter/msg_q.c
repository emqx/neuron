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
#include <pthread.h>

#include "utils/log.h"
#include "utils/utextend.h"

#include "msg_q.h"

struct item {
    neu_msg_t *  msg;
    struct item *prev, *next;
};

struct adapter_msg_q {
    struct item *list;
    uint32_t     max;
    uint32_t     current;
    char *       name;

    pthread_mutex_t mtx;
    pthread_cond_t  cond;
    volatile bool   exit_flag;
};

void adapter_msg_q_exit(adapter_msg_q_t *q);

adapter_msg_q_t *adapter_msg_q_new(const char *name, uint32_t size)
{
    struct adapter_msg_q *q = calloc(1, sizeof(struct adapter_msg_q));

    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->max     = size;
    q->name    = strdup(name);
    q->current = 0;

    return q;
}

void adapter_msg_q_free(adapter_msg_q_t *q)
{
    adapter_msg_q_exit(q);
    struct item *tmp = NULL, *elt = NULL;
    nlog_warn("app: %s, drop %u msg", q->name, q->current);
    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->cond);
    DL_FOREACH_SAFE(q->list, elt, tmp)
    {
        DL_DELETE(q->list, elt);
        neu_reqresp_head_t *header = neu_msg_get_header(elt->msg);
        neu_trans_data_free((neu_reqresp_trans_data_t *) &header[1]);
        neu_msg_free(elt->msg);
        free(elt);
    }
    free(q->name);
    free(q);
}

void adapter_msg_q_exit(adapter_msg_q_t *q)
{
    pthread_mutex_lock(&q->mtx);
    q->exit_flag = true;
    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->mtx);
}

int adapter_msg_q_push(adapter_msg_q_t *q, neu_msg_t *msg)
{
    int ret = -1;
    pthread_mutex_lock(&q->mtx);
    if (q->current < q->max) {
        q->current += 1;
        struct item *elt = calloc(1, sizeof(struct item));
        elt->msg         = msg;
        DL_APPEND(q->list, elt);
        ret = 0;
    }
    pthread_mutex_unlock(&q->mtx);

    if (ret == -1) {
        nlog_warn("app: %s, msg q is full, %u(%u)", q->name, q->current,
                  q->max);
    } else {
        pthread_cond_signal(&q->cond);
    }

    return ret;
}

uint32_t adapter_msg_q_pop(adapter_msg_q_t *q, neu_msg_t **p_data)
{
    uint32_t ret = 0;
    pthread_mutex_lock(&q->mtx);
    while (q->current == 0 && !q->exit_flag) {
        pthread_cond_wait(&q->cond, &q->mtx);
    }
    if (q->exit_flag) {
        pthread_mutex_unlock(&q->mtx);
        *p_data = NULL;
        return 0;
    }
    struct item *elt = DL_LAST(q->list);
    if (elt != NULL) {
        DL_DELETE(q->list, elt);
        *p_data = elt->msg;
        free(elt);
        q->current -= 1;
        ret = q->current;
    }
    pthread_mutex_unlock(&q->mtx);
    return ret;
}
