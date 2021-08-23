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

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/queue.h>
#include <sys/time.h>

#include "neu_log.h"

#include "connection/neu_tcp.h"

static void *process(void *arg);

struct client_elem;
struct neu_tcp_client_id {
    in_addr_t           server;
    uint16_t            port;
    int                 fd;
    struct client_elem *elem;
};

struct client_elem {
    struct neu_tcp_client_id *id;
    neu_tcp_process_response  callback;

    TAILQ_ENTRY(client_elem) node;
};

enum process_status {
    STOP    = 0,
    RUNNING = 1,
};

struct neu_tcp_client_context {
    enum process_status status;
    int                 epoll_fd;
    pthread_t           process;
    pthread_mutex_t     mtx;
    TAILQ_HEAD(, client_elem) client_elem_list;
};

static int client_connect(struct neu_tcp_client_id *id)
{
    int                ret  = -1;
    int                fd   = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { 0 };
    struct timeval     tm   = {
        .tv_sec  = 1,
        .tv_usec = 0,
    };

    addr.sin_family      = AF_INET;
    addr.sin_port        = id->port;
    addr.sin_addr.s_addr = id->server;

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tm, sizeof(tm));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tm, sizeof(tm));

    ret = connect(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

    if (ret != 0) {
        log_error("connect %d:%d error: %d", id->server, ntohs(id->server),
                  errno);
        close(fd);
        id->fd = -1;
        return -1;
    }

    id->fd = fd;
    return 0;
}

neu_tcp_client_context_t *neu_tcp_client_init()
{
    neu_tcp_client_context_t *ctx = (neu_tcp_client_context_t *) calloc(
        1, sizeof(neu_tcp_client_context_t));

    pthread_mutex_init(&ctx->mtx, NULL);
    TAILQ_INIT(&ctx->client_elem_list);

    ctx->epoll_fd = epoll_create(1);
    ctx->status   = RUNNING;

    pthread_create(&ctx->process, NULL, process, ctx);

    return ctx;
}

neu_tcp_client_id_t *neu_tcp_client_add(neu_tcp_client_context_t *ctx,
                                        char *server, uint16_t port,
                                        neu_tcp_process_response callback)
{
    struct client_elem * elem = NULL;
    neu_tcp_client_id_t *id   = NULL;

    elem = (struct client_elem *) calloc(1, sizeof(struct client_elem));
    elem->callback = callback;

    id         = (neu_tcp_client_id_t *) calloc(1, sizeof(neu_tcp_client_id_t));
    elem->id   = id;
    id->elem   = elem;
    id->port   = htons(port);
    id->server = inet_addr(server);

    pthread_mutex_lock(&ctx->mtx);
    TAILQ_INSERT_TAIL(&ctx->client_elem_list, elem, node);
    pthread_mutex_unlock(&ctx->mtx);

    if (client_connect(elem->id) == 0) {
        struct epoll_event event = { 0 };
        event.events             = EPOLLIN | EPOLLERR;
        event.data.ptr           = elem;
        epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, elem->id->fd, &event);
    }

    return id;
}

void neu_tcp_client_remove(neu_tcp_client_context_t *ctx,
                           neu_tcp_client_id_t *     id)
{
    struct client_elem *elem = NULL;

    pthread_mutex_lock(&ctx->mtx);
    TAILQ_FOREACH(elem, &ctx->client_elem_list, node)
    {
        if (elem->id == id) {
            break;
        }
    }

    if (elem != NULL) {
        TAILQ_REMOVE(&ctx->client_elem_list, elem, node);

        epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, elem->id->fd, NULL);
        close(elem->id->fd);
        free(elem->id);
        free(elem);
    }

    pthread_mutex_unlock(&ctx->mtx);
}

int neu_tcp_client_send(neu_tcp_client_context_t *ctx, neu_tcp_client_id_t *id,
                        char *buf, ssize_t len)
{
    ssize_t ret = 0;
    if (id->fd <= 0) {
        if (client_connect(id) == 0) {
            struct epoll_event event = { 0 };
            event.events             = EPOLLIN | EPOLLERR;
            event.data.ptr           = id->elem;
            epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, id->fd, &event);
        }
    }

    if (id->fd <= 0) {
        log_error("send buf, fd invalid");
        return -1;
    }

    ret = send(id->fd, buf, len, 0);

    if (ret != len) {
        log_error("send buf, fd: %d, ret: %ld, errno: %d", id->fd, ret, errno);
    }

    return ret;
}

void neu_tcp_client_free(neu_tcp_client_context_t *ctx)
{
    struct client_elem *elem = NULL;

    pthread_mutex_lock(&ctx->mtx);
    ctx->status = STOP;

    elem = TAILQ_FIRST(&ctx->client_elem_list);
    while (elem != NULL) {
        TAILQ_REMOVE(&ctx->client_elem_list, elem, node);

        epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, elem->id->fd, NULL);
        close(elem->id->fd);
        free(elem->id);
        free(elem);

        elem = TAILQ_FIRST(&ctx->client_elem_list);
    }

    pthread_mutex_unlock(&ctx->mtx);

    pthread_cancel(ctx->process);
    close(ctx->epoll_fd);
    pthread_mutex_destroy(&ctx->mtx);
    free(ctx);
}

static void *process(void *arg)
{
    neu_tcp_client_context_t *ctx = (neu_tcp_client_context_t *) arg;

    while (1) {
        int                ret        = -1;
        struct epoll_event events[10] = { 0 };

        pthread_mutex_lock(&ctx->mtx);
        if (ctx->status != RUNNING) {
            pthread_mutex_unlock(&ctx->mtx);
            break;
        }

        ret = epoll_wait(ctx->epoll_fd, events, 10, 100);
        if (ret == -1) {
            log_error("epoll wait error: %d", errno);
            continue;
        }

        for (int i = 0; i < ret; i++) {
            struct client_elem *elem         = NULL;
            char                buffer[1024] = { 0 };
            ssize_t             len          = -1;

            switch (events[i].events) {
            case EPOLLIN:
                elem = (struct client_elem *) events[i].data.ptr;

                len = read(elem->id->fd, buffer, sizeof(buffer));
                if (len <= 0) {
                    log_error("read fd: %d, ret: %d ,error: %d", elem->id->fd,
                              len, errno);
                    continue;
                }

                elem->callback(buffer, len);

                break;
            default:
                log_warn("no process event: %d", events[i].events);
                break;
            }
        }
        pthread_mutex_unlock(&ctx->mtx);
    }
    return NULL;
}
