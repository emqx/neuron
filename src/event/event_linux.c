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
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "event/event.h"
#include "utils/log.h"

#ifdef NEU_PLATFORM_LINUX
#include <sys/epoll.h>
#include <sys/timerfd.h>

struct neu_event_timer {
    int                    fd;
    struct event_data *    event_data;
    struct itimerspec      value;
    neu_event_timer_type_e type;
    pthread_mutex_t        mtx;
    bool                   stop;
};

struct neu_event_io {
    int                fd;
    struct event_data *event_data;
};
struct event_data {
    enum {
        TIMER = 0,
        IO    = 1,
    } type;
    union {
        neu_event_io_callback    io;
        neu_event_timer_callback timer;
    } callback;
    union {
        neu_event_io_t    io;
        neu_event_timer_t timer;
    } ctx;

    void *usr_data;
    int   fd;
    int   index;
    bool  use;
};

#define EVENT_SIZE 1400

struct neu_events {
    int       epoll_fd;
    pthread_t thread;
    bool      stop;

    pthread_mutex_t   mtx;
    int               n_event;
    struct event_data event_datas[EVENT_SIZE];
};

static int get_free_event(neu_events_t *events)
{
    int ret = -1;
    pthread_mutex_lock(&events->mtx);
    for (int i = 0; i < EVENT_SIZE; i++) {
        if (events->event_datas[i].use == false) {
            events->event_datas[i].use   = true;
            events->event_datas[i].index = i;
            ret                          = i;
            break;
        }
    }

    pthread_mutex_unlock(&events->mtx);
    return ret;
}

static void free_event(neu_events_t *events, int index)
{
    pthread_mutex_lock(&events->mtx);
    events->event_datas[index].use   = false;
    events->event_datas[index].index = 0;
    pthread_mutex_unlock(&events->mtx);
}

static void *event_loop(void *arg)
{
    neu_events_t *events   = (neu_events_t *) arg;
    int           epoll_fd = events->epoll_fd;

    while (!events->stop) {
        struct epoll_event event = { 0 };
        struct event_data *data  = NULL;

        int ret = epoll_wait(epoll_fd, &event, 1, 1000);
        if (ret == 0) {
            continue;
        }

        if (ret == -1 && errno == EINTR) {
            continue;
        }

        if (ret == -1 || events->stop) {
            zlog_warn(neuron, "event loop exit, errno: %s(%d), stop: %d",
                      strerror(errno), errno, events->stop);
            break;
        }

        data = (struct event_data *) event.data.ptr;

        switch (data->type) {
        case TIMER:
            pthread_mutex_lock(&data->ctx.timer.mtx);
            if ((event.events & EPOLLIN) == EPOLLIN) {
                uint64_t t;

                ssize_t size = read(data->fd, &t, sizeof(t));
                (void) size;

                if (!data->ctx.timer.stop) {
                    if (data->ctx.timer.type == NEU_EVENT_TIMER_BLOCK) {
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, data->fd, NULL);
                        ret = data->callback.timer(data->usr_data);
                        timerfd_settime(data->fd, 0, &data->ctx.timer.value,
                                        NULL);
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, data->fd, &event);
                    } else {
                        ret = data->callback.timer(data->usr_data);
                    }
                }
            }

            pthread_mutex_unlock(&data->ctx.timer.mtx);
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
    }

    return NULL;
};

neu_events_t *neu_event_new(void)
{
    neu_events_t *events = calloc(1, sizeof(struct neu_events));

    events->epoll_fd = epoll_create(1);

    nlog_notice("create epoll: %d(%d)", events->epoll_fd, errno);
    assert(events->epoll_fd > 0);

    events->stop    = false;
    events->n_event = 0;
    pthread_mutex_init(&events->mtx, NULL);

    pthread_create(&events->thread, NULL, event_loop, events);

    return events;
};

int neu_event_close(neu_events_t *events)
{
    events->stop = true;
    close(events->epoll_fd);

    pthread_join(events->thread, NULL);
    pthread_mutex_destroy(&events->mtx);

    free(events);
    return 0;
}

neu_event_timer_t *neu_event_add_timer(neu_events_t *          events,
                                       neu_event_timer_param_t timer)
{
    int               ret      = 0;
    int               timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec value    = {
        .it_value.tv_sec     = timer.second,
        .it_value.tv_nsec    = timer.millisecond * 1000 * 1000,
        .it_interval.tv_sec  = timer.second,
        .it_interval.tv_nsec = timer.millisecond * 1000 * 1000,
    };
    int index = get_free_event(events);
    if (index < 0) {
        zlog_fatal(neuron, "no free event: %d", events->epoll_fd);
    }
    assert(index >= 0);

    neu_event_timer_t *timer_ctx = &events->event_datas[index].ctx.timer;
    timer_ctx->event_data        = &events->event_datas[index];

    struct epoll_event event = {
        .events   = EPOLLIN,
        .data.ptr = timer_ctx->event_data,
    };

    timerfd_settime(timer_fd, 0, &value, NULL);

    timer_ctx->event_data->type           = TIMER;
    timer_ctx->event_data->fd             = timer_fd;
    timer_ctx->event_data->usr_data       = timer.usr_data;
    timer_ctx->event_data->callback.timer = timer.cb;
    timer_ctx->event_data->ctx.timer = events->event_datas[index].ctx.timer;
    timer_ctx->event_data->index     = index;

    timer_ctx->value = value;
    timer_ctx->fd    = timer_fd;
    timer_ctx->type  = timer.type;
    timer_ctx->stop  = false;
    pthread_mutex_init(&timer_ctx->mtx, NULL);

    ret = epoll_ctl(events->epoll_fd, EPOLL_CTL_ADD, timer_fd, &event);

    zlog_notice(neuron,
                "add timer, second: %" PRId64 ", millisecond: %" PRId64
                ", timer: %d in epoll %d, "
                "ret: %d, index: %d",
                timer.second, timer.millisecond, timer_fd, events->epoll_fd,
                ret, index);

    return timer_ctx;
}

int neu_event_del_timer(neu_events_t *events, neu_event_timer_t *timer)
{
    zlog_notice(neuron, "del timer: %d from epoll: %d, index: %d", timer->fd,
                events->epoll_fd, timer->event_data->index);

    timer->stop = true;
    epoll_ctl(events->epoll_fd, EPOLL_CTL_DEL, timer->fd, NULL);

    pthread_mutex_lock(&timer->mtx);
    close(timer->fd);
    pthread_mutex_unlock(&timer->mtx);

    pthread_mutex_destroy(&timer->mtx);
    free_event(events, timer->event_data->index);
    return 0;
}

neu_event_io_t *neu_event_add_io(neu_events_t *events, neu_event_io_param_t io)
{
    int ret   = 0;
    int index = get_free_event(events);

    nlog_notice("add io, fd: %d, epoll: %d, index: %d", io.fd, events->epoll_fd,
                index);
    assert(index >= 0);

    neu_event_io_t *io_ctx   = &events->event_datas[index].ctx.io;
    io_ctx->event_data       = &events->event_datas[index];
    struct epoll_event event = {
        .events   = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP,
        .data.ptr = io_ctx->event_data,
    };

    io_ctx->event_data->type        = IO;
    io_ctx->event_data->fd          = io.fd;
    io_ctx->event_data->usr_data    = io.usr_data;
    io_ctx->event_data->callback.io = io.cb;
    io_ctx->event_data->ctx.io      = events->event_datas[index].ctx.io;
    io_ctx->event_data->index       = index;

    io_ctx->fd = io.fd;

    ret = epoll_ctl(events->epoll_fd, EPOLL_CTL_ADD, io.fd, &event);

    nlog_notice("add io, fd: %d, epoll: %d, ret: %d(%d), index: %d", io.fd,
                events->epoll_fd, ret, errno, index);
    assert(ret == 0);

    return io_ctx;
}

int neu_event_del_io(neu_events_t *events, neu_event_io_t *io)
{
    if (io == NULL) {
        return 0;
    }

    zlog_notice(neuron, "del io: %d from epoll: %d, index: %d", io->fd,
                events->epoll_fd, io->event_data->index);

    epoll_ctl(events->epoll_fd, EPOLL_CTL_DEL, io->fd, NULL);
    free_event(events, io->event_data->index);

    return 0;
}

#endif