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
 **/
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "event/event.h"

#ifdef NEU_PLATFORM_DARWIN

#include <sys/event.h>

struct neu_event_timer {
    int                      id;
    void *                   usr_data;
    neu_event_timer_callback timer;
};

struct neu_event_io {
};

struct neu_events {
    int             kq;
    bool            running;
    pthread_t       thread;
    pthread_mutex_t mtx;
    int32_t         timer_id;
};

static void *event_loop(void *arg)
{
    neu_events_t *events  = (neu_events_t *) arg;
    bool          running = events->running;

    while (running) {
        struct kevent   event   = { 0 };
        struct timespec timeout = { .tv_sec = 1, .tv_nsec = 0 };
        int             ret = kevent(events->kq, NULL, 0, &event, 1, &timeout);

        if (ret == 0) {
            continue;
        }

        if (ret == -1) {
            log_error("kevent wait error: %d(%s), fd: %d", errno,
                      strerror(errno), events->kq);
            continue;
        }

        if (event.filter == EVFILT_TIMER) {
            neu_event_timer_t *ctx = (neu_event_timer_t *) event.udata;

            ret = ctx->timer(ctx->usr_data);
            log_debug("timer trigger: %d, ret: %d", ctx->id, ret);
        }

        pthread_mutex_lock(&events->mtx);
        running = events->running;
        pthread_mutex_unlock(&events->mtx);
    }

    return NULL;
}

neu_events_t *neu_event_new(void)
{
    neu_events_t *events = calloc(1, sizeof(neu_events_t));

    pthread_mutex_init(&events->mtx, NULL);

    events->running  = true;
    events->kq       = kqueue();
    events->timer_id = 1;

    pthread_create(&events->thread, NULL, event_loop, events);

    return events;
};

int neu_event_close(neu_events_t *events)
{
    pthread_mutex_lock(&events->mtx);
    events->running = false;
    pthread_mutex_unlock(&events->mtx);

    pthread_join(events->thread, NULL);
    pthread_mutex_destroy(&events->mtx);

    free(events);
    return 0;
}

neu_event_timer_t *neu_event_add_timer(neu_events_t *          events,
                                       neu_event_timer_param_t timer)
{
    struct kevent      ke  = { 0 };
    neu_event_timer_t *ctx = calloc(1, sizeof(neu_event_timer_t));
    int                ret = 0;

    ctx->id       = events->timer_id++;
    ctx->usr_data = timer.usr_data;
    ctx->timer    = timer.cb;

    EV_SET(&ke, ctx->id, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0,
           timer.second * 1000 + timer.millisecond, ctx);

    ret = kevent(events->kq, &ke, 1, NULL, 0, NULL);

    log_info("add timer, second: %ld, millisecond: %ld, timer: %d in epoll %d, "
             "ret: %d",
             timer.second, timer.millisecond, ctx->id, events->kq, ret);
    return ctx;
}

int neu_event_del_timer(neu_events_t *events, neu_event_timer_t *timer)
{
    struct kevent ke = { 0 };

    EV_SET(&ke, timer->id, EVFILT_TIMER, EV_DELETE, 0, 0, timer);

    kevent(events->kq, &ke, 1, NULL, 0, NULL);

    free(timer);
    return 0;
}

neu_event_io_t *neu_event_add_io(neu_events_t *events, neu_event_io_param_t io)
{
    (void) events;
    (void) io;
    return NULL;
}

int neu_event_del_io(neu_events_t *events, neu_event_io_t *io)
{
    (void) events;
    (void) io;
    return 0;
}

#endif
