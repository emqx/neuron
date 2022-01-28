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
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "event/event.h"
#include "neu_log.h"

#ifdef NEU_PLATFORM_LINUX
#include <sys/epoll.h>
#include <sys/timerfd.h>

struct neu_event_timer_ctx {
    int   fd;
    void *event_data;
};

struct neu_event_io_ctx {
    int   fd;
    void *event_data;
};

struct event_data {
    enum {
        TIMER,
        IO,
    } type;
    union {
        neu_event_io_callback    io;
        neu_event_timer_callback timer;
    } callback;
    union {
        struct neu_event_io_ctx *   io;
        struct neu_event_timer_ctx *timer;
    } ctx;

    void *usr_data;
    int   fd;

    TAILQ_ENTRY(event_data) node;
};

struct neu_event_ctx {
    int             epoll_fd;
    bool            running;
    pthread_t       thread;
    pthread_mutex_t mtx;

    TAILQ_HEAD(, event_data) datas;
};

static void *event_loop(void *arg)
{
    neu_event_ctx_t *events  = (neu_event_ctx_t *) arg;
    bool             running = events->running;

    while (running) {
        struct epoll_event event = { 0 };
        struct event_data *data  = NULL;

        int ret = epoll_wait(events->epoll_fd, &event, 1, 1000);
        if (ret == 0) {
            continue;
        }
        if (ret == -1) {
            break;
        }

        data = (struct event_data *) event.data.ptr;

        switch (data->type) {
        case TIMER:
            if ((event.events & EPOLLIN) == EPOLLIN) {
                uint64_t t;

                read(data->fd, &t, sizeof(t));

                ret = data->callback.timer(data->usr_data);
                log_debug("timer trigger: %d, ret: %d", data->fd, ret);
            }
            break;
        case IO:
            if ((event.events & EPOLLHUP) == EPOLLHUP) {
                data->callback.io(NEU_EVENT_IO_HUP, data->fd, data->usr_data);
                break;
            }

            if ((event.events & EPOLLRDHUP) == EPOLLRDHUP) {
                data->callback.io(NEU_EVENT_IO_CLOSED, data->fd,
                                  data->usr_data);
                break;
            }

            if ((event.events & EPOLLIN) == EPOLLIN) {
                data->callback.io(NEU_EVENT_IO_READ, data->fd, data->usr_data);
                break;
            }

            break;
        }

        pthread_mutex_lock(&events->mtx);
        running = events->running;
        pthread_mutex_unlock(&events->mtx);
    }

    return NULL;
};

neu_event_ctx_t *neu_event_new(void)
{
    neu_event_ctx_t *events = calloc(1, sizeof(struct neu_event_ctx));

    events->epoll_fd = epoll_create(1);
    events->running  = true;

    TAILQ_INIT(&events->datas);
    pthread_mutex_init(&events->mtx, NULL);

    pthread_create(&events->thread, NULL, event_loop, events);

    return events;
};

int neu_event_close(neu_event_ctx_t *events)
{
    struct event_data *data = NULL;

    pthread_mutex_lock(&events->mtx);
    events->running = false;

    data = TAILQ_FIRST(&events->datas);
    while (data != NULL) {
        epoll_ctl(events->epoll_fd, EPOLL_CTL_DEL, data->fd, NULL);
        TAILQ_REMOVE(&events->datas, data, node);
        close(data->fd);
        switch (data->type) {
        case TIMER:
            free(data->ctx.timer);
            break;
        case IO:
            free(data->ctx.io);
            break;
        }
        free(data);

        data = TAILQ_FIRST(&events->datas);
    }

    pthread_mutex_unlock(&events->mtx);

    close(events->epoll_fd);
    pthread_mutex_destroy(&events->mtx);

    free(events);
    return 0;
}

neu_event_timer_ctx_t *neu_event_add_timer(neu_event_ctx_t * events,
                                           neu_event_timer_t timer)
{
    int               ret      = 0;
    int               timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec value    = {
        .it_value.tv_sec     = timer.second,
        .it_value.tv_nsec    = timer.millisecond * 1000 * 1000,
        .it_interval.tv_sec  = timer.second,
        .it_interval.tv_nsec = timer.millisecond * 1000 * 1000,
    };
    struct event_data *    data      = calloc(1, sizeof(struct event_data));
    struct epoll_event     event     = { .events = EPOLLIN, .data.ptr = data };
    neu_event_timer_ctx_t *timer_ctx = calloc(1, sizeof(neu_event_timer_ctx_t));

    timerfd_settime(timer_fd, 0, &value, NULL);

    data->type           = TIMER;
    data->fd             = timer_fd;
    data->usr_data       = timer.usr_data;
    data->callback.timer = timer.cb;
    data->ctx.timer      = timer_ctx;

    timer_ctx->fd         = timer_fd;
    timer_ctx->event_data = data;

    pthread_mutex_lock(&events->mtx);
    TAILQ_INSERT_TAIL(&events->datas, data, node);
    pthread_mutex_unlock(&events->mtx);

    ret = epoll_ctl(events->epoll_fd, EPOLL_CTL_ADD, timer_fd, &event);

    log_info("add timer, second: %ld, millisecond: %ld, timer: %d in epoll %d, "
             "ret: %d",
             timer.second, timer.millisecond, timer_fd, events->epoll_fd, ret);

    return timer_ctx;
}

int neu_event_del_timer(neu_event_ctx_t *events, neu_event_timer_ctx_t *timer)
{
    struct event_data *data = NULL;
    log_info("del timer: %d from epoll: %d", timer->fd, events->epoll_fd);

    epoll_ctl(events->epoll_fd, EPOLL_CTL_DEL, timer->fd, NULL);

    pthread_mutex_lock(&events->mtx);
    TAILQ_FOREACH(data, &events->datas, node)
    {
        if (data->fd == timer->fd) {
            TAILQ_REMOVE(&events->datas, data, node);
            close(timer->fd);

            free(timer->event_data);
            free(timer);
            break;
        }
    }

    pthread_mutex_unlock(&events->mtx);

    return 0;
}

neu_event_io_ctx_t *neu_event_add_io(neu_event_ctx_t *events, neu_event_io_t io)
{
    int                 ret    = 0;
    neu_event_io_ctx_t *io_ctx = calloc(1, sizeof(neu_event_io_ctx_t));
    struct event_data * data   = calloc(1, sizeof(struct event_data));
    struct epoll_event  event  = { .events =
                                     EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP,
                                 .data.ptr = data };

    data->type        = IO;
    data->fd          = io.fd;
    data->usr_data    = io.usr_data;
    data->callback.io = io.cb;
    data->ctx.io      = io_ctx;

    io_ctx->fd         = io.fd;
    io_ctx->event_data = data;

    pthread_mutex_lock(&events->mtx);
    TAILQ_INSERT_TAIL(&events->datas, data, node);
    pthread_mutex_unlock(&events->mtx);

    ret = epoll_ctl(events->epoll_fd, EPOLL_CTL_ADD, io.fd, &event);

    log_info("add io, fd: %d, epoll: %d, ret: %d", io.fd, events->epoll_fd,
             ret);

    return io_ctx;
}

int neu_event_del_io(neu_event_ctx_t *events, neu_event_io_ctx_t *io)
{
    struct event_data *data = NULL;
    log_info("del io: %d from epoll: %d", io->fd, events->epoll_fd);

    epoll_ctl(events->epoll_fd, EPOLL_CTL_DEL, io->fd, NULL);

    pthread_mutex_lock(&events->mtx);
    TAILQ_FOREACH(data, &events->datas, node)
    {
        if (data->fd == io->fd) {
            TAILQ_REMOVE(&events->datas, data, node);

            free(io->event_data);
            free(io);
            break;
        }
    }

    pthread_mutex_unlock(&events->mtx);

    return 0;
}

#endif