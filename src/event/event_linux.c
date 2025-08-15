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

/**
 * @file event_linux.c
 * @brief Linux平台上的事件处理实现
 *
 * 该文件实现了Neuron系统在Linux平台上的事件处理机制，基于epoll实现。
 * 主要功能包括：
 * 1. 事件循环创建和管理
 * 2. 定时器事件的添加和删除
 * 3. IO事件的添加和删除
 *
 * 系统使用epoll监听文件描述符上的事件，并在事件触发时调用相应的回调函数。
 * 定时器事件使用timerfd实现，IO事件直接使用文件描述符。
 */
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

/**
 * @brief 定时器事件结构体
 *
 * 表示一个定时器事件的上下文信息
 */
struct neu_event_timer {
    int                    fd;         /**< 定时器文件描述符 */
    struct event_data     *event_data; /**< 关联的事件数据指针 */
    struct itimerspec      value;      /**< 定时器的时间配置 */
    neu_event_timer_type_e type;       /**< 定时器类型(阻塞/非阻塞) */
    pthread_mutex_t        mtx;        /**< 保护定时器操作的互斥锁 */
    bool                   stop;       /**< 定时器是否已停止 */
};

/**
 * @brief IO事件结构体
 *
 * 表示一个IO事件的上下文信息
 */
struct neu_event_io {
    int                fd;         /**< IO事件的文件描述符 */
    struct event_data *event_data; /**< 关联的事件数据指针 */
};
/**
 * @brief 事件数据结构体
 *
 * 用于存储事件的详细信息，包括类型、回调函数、上下文等
 */
struct event_data {
    enum {
        TIMER = 0, /**< 定时器事件 */
        IO    = 1, /**< IO事件 */
    } type;
    union {
        neu_event_io_callback    io;    /**< IO事件回调函数 */
        neu_event_timer_callback timer; /**< 定时器事件回调函数 */
    } callback;
    union {
        neu_event_io_t    io;    /**< IO事件上下文 */
        neu_event_timer_t timer; /**< 定时器事件上下文 */
    } ctx;

    void *usr_data; /**< 用户数据，传递给回调函数 */
    int   fd;       /**< 事件关联的文件描述符 */
    int   index;    /**< 事件在事件数组中的索引 */
    bool  use;      /**< 标记该事件槽是否被使用 */
};

/** 事件数组的最大容量 */
#define EVENT_SIZE 1400

/**
 * @brief 事件管理器结构体
 *
 * 管理所有事件的核心数据结构
 */
struct neu_events {
    int       epoll_fd; /**< epoll实例的文件描述符 */
    pthread_t thread;   /**< 事件循环线程 */
    bool      stop;     /**< 事件循环是否应该停止 */

    pthread_mutex_t   mtx;                     /**< 保护事件数组的互斥锁 */
    int               n_event;                 /**< 当前事件数量 */
    struct event_data event_datas[EVENT_SIZE]; /**< 事件数据数组 */
};

/**
 * @brief 获取一个空闲的事件槽位
 *
 * 在事件数组中查找一个未使用的槽位，并标记为已使用
 *
 * @param events 事件管理器指针
 * @return 成功返回事件索引，失败返回-1
 */
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

/**
 * @brief 释放事件槽位
 *
 * 将指定索引的事件槽位标记为未使用
 *
 * @param events 事件管理器指针
 * @param index 要释放的事件索引
 */
static void free_event(neu_events_t *events, int index)
{
    pthread_mutex_lock(&events->mtx);
    events->event_datas[index].use   = false;
    events->event_datas[index].index = 0;
    pthread_mutex_unlock(&events->mtx);
}

/**
 * @brief 事件循环线程函数
 *
 * 持续监听epoll事件，并调用相应的回调函数处理事件
 *
 * @param arg 事件管理器指针
 * @return NULL
 */
static void *event_loop(void *arg)
{
    neu_events_t *events   = (neu_events_t *) arg;
    int           epoll_fd = events->epoll_fd;

    while (true) {
        struct epoll_event event = { 0 };
        struct event_data *data  = NULL;

        // 等待事件，超时时间为1秒
        int ret = epoll_wait(epoll_fd, &event, 1, 1000);
        if (ret == 0) {
            // 超时，继续等待
            continue;
        }

        if (ret == -1 && errno == EINTR) {
            // 被信号中断，继续等待
            continue;
        }

        if (ret == -1 || events->stop) {
            // 出错或被要求停止，退出循环
            zlog_warn(neuron, "event loop exit, errno: %s(%d), stop: %d",
                      strerror(errno), errno, events->stop);
            break;
        }

        // 获取事件关联的数据
        data = (struct event_data *) event.data.ptr;

        // 根据事件类型处理
        switch (data->type) {
        case TIMER: // 定时器事件
            pthread_mutex_lock(&data->ctx.timer.mtx);
            if ((event.events & EPOLLIN) == EPOLLIN) {
                uint64_t t;

                // 读取定时器数据，清除通知
                ssize_t size = read(data->fd, &t, sizeof(t));
                (void) size; // 忽略返回值

                if (!data->ctx.timer.stop) {
                    if (data->ctx.timer.type == NEU_EVENT_TIMER_BLOCK) {
                        // 阻塞型定时器：在回调执行期间从epoll中移除，执行完后再添加回来
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, data->fd, NULL);
                        ret = data->callback.timer(data->usr_data);
                        timerfd_settime(data->fd, 0, &data->ctx.timer.value,
                                        NULL);
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, data->fd, &event);
                    } else {
                        // 非阻塞型定时器：直接执行回调
                        ret = data->callback.timer(data->usr_data);
                    }
                }
            }

            pthread_mutex_unlock(&data->ctx.timer.mtx);
            break;
        case IO: // IO事件
            if ((event.events & EPOLLHUP) == EPOLLHUP) {
                // 挂起事件
                data->callback.io(NEU_EVENT_IO_HUP, data->fd, data->usr_data);
                break;
            }

            if ((event.events & EPOLLRDHUP) == EPOLLRDHUP) {
                // 对端关闭连接事件
                data->callback.io(NEU_EVENT_IO_CLOSED, data->fd,
                                  data->usr_data);
                break;
            }

            if ((event.events & EPOLLIN) == EPOLLIN) {
                // 可读事件
                data->callback.io(NEU_EVENT_IO_READ, data->fd, data->usr_data);
                break;
            }

            break;
        }
    }

    return NULL;
};

/**
 * @brief 创建新的事件管理器
 *
 * 分配并初始化事件管理器，创建epoll实例，启动事件循环线程
 *
 * @return 新创建的事件管理器指针
 */
neu_events_t *neu_event_new(void)
{
    // 分配事件管理器内存
    neu_events_t *events = calloc(1, sizeof(struct neu_events));

    // 创建epoll实例
    events->epoll_fd = epoll_create(1);

    nlog_notice("create epoll: %d(%d)", events->epoll_fd, errno);
    assert(events->epoll_fd > 0); // 确保创建成功

    // 初始化字段
    events->stop    = false;
    events->n_event = 0;
    pthread_mutex_init(&events->mtx, NULL);

    // 创建事件循环线程
    pthread_create(&events->thread, NULL, event_loop, events);

    return events;
};

/**
 * @brief 关闭事件管理器
 *
 * 停止事件循环线程，释放事件管理器资源
 *
 * @param events 要关闭的事件管理器指针
 * @return 始终返回0
 */
int neu_event_close(neu_events_t *events)
{
    // 设置停止标志
    events->stop = true;
    // 关闭epoll实例
    close(events->epoll_fd);

    // 等待事件循环线程结束
    pthread_join(events->thread, NULL);
    // 销毁互斥锁
    pthread_mutex_destroy(&events->mtx);

    // 释放事件管理器内存
    free(events);
    return 0;
}

/**
 * @brief 添加定时器事件
 *
 * 创建一个定时器事件并添加到事件管理器中
 *
 * @param events 事件管理器指针
 * @param timer 定时器参数
 * @return 新创建的定时器事件指针
 */
neu_event_timer_t *neu_event_add_timer(neu_events_t           *events,
                                       neu_event_timer_param_t timer)
{
    int ret = 0;
    // 创建timerfd
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    // 设置定时器参数
    struct itimerspec value = {
        .it_value.tv_sec     = timer.second,
        .it_value.tv_nsec    = timer.millisecond * 1000 * 1000,
        .it_interval.tv_sec  = timer.second,
        .it_interval.tv_nsec = timer.millisecond * 1000 * 1000,
    };

    // 获取空闲事件槽位
    int index = get_free_event(events);
    if (index < 0) {
        zlog_notice(neuron, "no free event: %d", events->epoll_fd);
    }
    assert(index >= 0);

    // 获取定时器上下文
    neu_event_timer_t *timer_ctx = &events->event_datas[index].ctx.timer;
    timer_ctx->event_data        = &events->event_datas[index];

    // 设置epoll事件
    struct epoll_event event = {
        .events   = EPOLLIN,
        .data.ptr = timer_ctx->event_data,
    };

    // 启动定时器
    timerfd_settime(timer_fd, 0, &value, NULL);

    // 设置事件数据
    timer_ctx->event_data->type           = TIMER;
    timer_ctx->event_data->fd             = timer_fd;
    timer_ctx->event_data->usr_data       = timer.usr_data;
    timer_ctx->event_data->callback.timer = timer.cb;
    timer_ctx->event_data->ctx.timer = events->event_datas[index].ctx.timer;
    timer_ctx->event_data->index     = index;

    // 设置定时器上下文
    timer_ctx->value = value;
    timer_ctx->fd    = timer_fd;
    timer_ctx->type  = timer.type;
    timer_ctx->stop  = false;
    pthread_mutex_init(&timer_ctx->mtx, NULL);

    // 添加到epoll
    ret = epoll_ctl(events->epoll_fd, EPOLL_CTL_ADD, timer_fd, &event);

    zlog_notice(neuron,
                "add timer, second: %" PRId64 ", millisecond: %" PRId64
                ", timer: %d in epoll %d, "
                "ret: %d, index: %d",
                timer.second, timer.millisecond, timer_fd, events->epoll_fd,
                ret, index);

    return timer_ctx;
}

/**
 * @brief 删除定时器事件
 *
 * 停止并删除指定的定时器事件，释放相关资源
 *
 * @param events 事件管理器指针
 * @param timer 要删除的定时器事件指针
 * @return 始终返回0
 */
int neu_event_del_timer(neu_events_t *events, neu_event_timer_t *timer)
{
    zlog_notice(neuron, "del timer: %d from epoll: %d, index: %d", timer->fd,
                events->epoll_fd, timer->event_data->index);

    // 设置停止标志
    timer->stop = true;
    // 从epoll中删除
    epoll_ctl(events->epoll_fd, EPOLL_CTL_DEL, timer->fd, NULL);

    // 安全关闭文件描述符
    pthread_mutex_lock(&timer->mtx);
    close(timer->fd);
    pthread_mutex_unlock(&timer->mtx);

    // 销毁互斥锁
    pthread_mutex_destroy(&timer->mtx);
    // 释放事件槽位
    free_event(events, timer->event_data->index);
    return 0;
}

/**
 * @brief 添加IO事件
 *
 * 将指定的文件描述符添加到事件管理器中监听IO事件
 *
 * @param events 事件管理器指针
 * @param io IO事件参数
 * @return 新创建的IO事件指针
 */
neu_event_io_t *neu_event_add_io(neu_events_t *events, neu_event_io_param_t io)
{
    int ret = 0;
    // 获取空闲事件槽位
    int index = get_free_event(events);

    nlog_notice("add io, fd: %d, epoll: %d, index: %d", io.fd, events->epoll_fd,
                index);
    assert(index >= 0);

    // 获取IO上下文
    neu_event_io_t *io_ctx = &events->event_datas[index].ctx.io;
    io_ctx->event_data     = &events->event_datas[index];

    // 设置epoll事件，监听读取、错误、挂起和对端关闭事件
    struct epoll_event event = {
        .events   = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP,
        .data.ptr = io_ctx->event_data,
    };

    // 设置事件数据
    io_ctx->event_data->type        = IO;
    io_ctx->event_data->fd          = io.fd;
    io_ctx->event_data->usr_data    = io.usr_data;
    io_ctx->event_data->callback.io = io.cb;
    io_ctx->event_data->ctx.io      = events->event_datas[index].ctx.io;
    io_ctx->event_data->index       = index;

    // 设置IO上下文
    io_ctx->fd = io.fd;

    // 添加到epoll
    ret = epoll_ctl(events->epoll_fd, EPOLL_CTL_ADD, io.fd, &event);

    nlog_notice("add io, fd: %d, epoll: %d, ret: %d(%d), index: %d", io.fd,
                events->epoll_fd, ret, errno, index);
    assert(ret == 0); // 确保添加成功

    return io_ctx;
}

/**
 * @brief 删除IO事件
 *
 * 从事件管理器中删除指定的IO事件，停止监听该文件描述符
 *
 * @param events 事件管理器指针
 * @param io 要删除的IO事件指针
 * @return 始终返回0
 */
int neu_event_del_io(neu_events_t *events, neu_event_io_t *io)
{
    if (io == NULL) {
        return 0; // 空指针直接返回
    }

    zlog_notice(neuron, "del io: %d from epoll: %d, index: %d", io->fd,
                events->epoll_fd, io->event_data->index);

    // 从epoll中删除
    epoll_ctl(events->epoll_fd, EPOLL_CTL_DEL, io->fd, NULL);
    // 释放事件槽位
    free_event(events, io->event_data->index);

    return 0;
}

#endif