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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/util/platform.h>
#include <nng/transport/ws/websocket.h>
#include <pthread.h>

#include "utils/log.h"
#include "utils/zlog.h"
#include "websocket.h"

#ifdef DEBUG
#define PRINT(...)                    \
    do {                              \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)
#else
#define PRINT(...)
#endif

// WebSocket stream
typedef struct {
    pthread_mutex_t lock;
    bool            closed;
    size_t          refcnt;
    nng_stream *    s;
} websocket_stream_t;

static inline websocket_stream_t *websocket_stream_new(nng_stream *s);
static inline websocket_stream_t *
                   websocket_stream_ref(websocket_stream_t *stream);
static inline void websocket_stream_deref(websocket_stream_t *stream);

// thread local context
typedef struct thread_ctx {
    nng_aio *           aio;
    websocket_stream_t *stream;
} thread_ctx_t;

static inline thread_ctx_t *thread_ctx_new();
static inline void          thread_ctx_free(thread_ctx_t *thread_ctx);

// WebSocket context
typedef struct {
    nng_stream_listener *listener;
    nng_aio *            accept_aio;

    pthread_rwlock_t    lock;
    websocket_stream_t *stream;
    pthread_key_t       thread_key;
} websocket_ctx_t;

static int  websocket_ctx_init(websocket_ctx_t *ctx, const char *url);
static void websocket_ctx_fini(websocket_ctx_t *ctx);
static inline thread_ctx_t *websocket_ctx_get_thread_ctx(websocket_ctx_t *ctx);
static inline void          websocket_ctx_del_thread_ctx(websocket_ctx_t *ctx,
                                                         thread_ctx_t *   thread_ctx);
static inline int websocket_ctx_send(websocket_ctx_t *ctx, const void *buf,
                                     size_t size);

static inline websocket_stream_t *websocket_stream_new(nng_stream *s)
{
    websocket_stream_t *stream = calloc(1, sizeof(*stream));

    if (NULL != stream) {
        pthread_mutex_init(&stream->lock, NULL);
        stream->closed = false;
        stream->refcnt = 1;
        stream->s      = s;
    }

    return stream;
}

static inline websocket_stream_t *
websocket_stream_ref(websocket_stream_t *stream)
{
    pthread_mutex_lock(&stream->lock);
    ++stream->refcnt;
    pthread_mutex_unlock(&stream->lock);
    return stream;
}

static inline void websocket_stream_deref(websocket_stream_t *stream)
{
    pthread_mutex_lock(&stream->lock);
    if (!stream->closed) {
        stream->closed = true;
        nng_stream_close(stream->s);
    }
    if (0 == --stream->refcnt) {
        PRINT("free stream\n");
        pthread_mutex_unlock(&stream->lock);
        pthread_mutex_destroy(&stream->lock);
        nng_stream_free(stream->s);
        free(stream);
        return;
    }
    pthread_mutex_unlock(&stream->lock);
}

// NOTE: Do not call log functions otherwise recursion will happen.
static inline thread_ctx_t *thread_ctx_new()
{
    thread_ctx_t *thread_ctx = calloc(1, sizeof(thread_ctx_t));

    if (NULL == thread_ctx) {
        PRINT("allocate websocket thread ctx fail\n");
        return NULL;
    }

    if (0 != nng_aio_alloc(&thread_ctx->aio, NULL, NULL)) {
        PRINT("allocate websocket thread aio fail\n");
        free(thread_ctx);
        return NULL;
    }

    nng_aio_set_timeout(thread_ctx->aio, 1000);

    return thread_ctx;
}

static inline void thread_ctx_free(thread_ctx_t *thread_ctx)
{
    if (NULL == thread_ctx) {
        return;
    }
    if (NULL != thread_ctx->stream) {
        websocket_stream_deref(thread_ctx->stream);
        thread_ctx->stream = NULL;
    }
    nng_aio_stop(thread_ctx->aio);
    nng_aio_free(thread_ctx->aio);
    free(thread_ctx);
}

static void ws_accept_cb(void *arg)
{
    int              rv  = 0;
    websocket_ctx_t *ctx = arg;

    rv = nng_aio_result(ctx->accept_aio);
    if (0 != rv) {
        PRINT("websocket accept error: %s\n", nng_strerror(rv));
        pthread_rwlock_wrlock(&ctx->lock);
        if (NULL != ctx->stream) {
            websocket_stream_deref(ctx->stream);
            ctx->stream = NULL;
        }
        pthread_rwlock_unlock(&ctx->lock);
        return;
    }

    nng_stream *s = nng_aio_get_output(ctx->accept_aio, 0);

    pthread_rwlock_wrlock(&ctx->lock);
    if (NULL == ctx->stream) {
        ctx->stream = websocket_stream_new(s);
        if (NULL == ctx->stream) {
            PRINT("allocate websocket stream fail\n");
            nng_stream_free(s);
        }
    } else {
        PRINT("websocket already connected, refused\n");
        nng_stream_close(s);
        nng_stream_free(s);
    }
    pthread_rwlock_unlock(&ctx->lock);

    nng_stream_listener_accept(ctx->listener, ctx->accept_aio);
}

static int websocket_ctx_init(websocket_ctx_t *ctx, const char *url)
{
    int rv = 0;

    ctx->stream = NULL;

    rv = nng_ws_register();
    if (0 != rv) {
        nlog_error("register websocket transport fail: %s", nng_strerror(rv));
        return rv;
    }

    rv = nng_stream_listener_alloc(&ctx->listener, url);
    if (0 != rv) {
        nlog_error("allocate websocket listener fail: %s", nng_strerror(rv));
        return rv;
    }

    rv = nng_stream_listener_listen(ctx->listener);
    if (0 != rv) {
        nlog_error("listen websocket listener fail: %s", nng_strerror(rv));
        goto err_listen;
    }

    rv = nng_aio_alloc(&ctx->accept_aio, ws_accept_cb, ctx);
    if (0 != rv) {
        nlog_error("allocate websocket accept aio fail: %s", nng_strerror(rv));
        goto err_alloc_accept_aio;
    }

    rv = pthread_rwlock_init(&ctx->lock, NULL);
    if (0 != rv) {
        nlog_error("pthread_rwlock_init fail: %d", rv);
        goto err_rwlock_init_fail;
    }

    rv = pthread_key_create(&ctx->thread_key,
                            (void (*)(void *)) thread_ctx_free);
    if (0 != rv) {
        nlog_error("pthread_key_create fail: %d", rv);
        goto err_key_create_fail;
    }

    nng_stream_listener_accept(ctx->listener, ctx->accept_aio);

    nlog_info("websocket ctx initialized successfully");

    return rv;

err_key_create_fail:
    pthread_rwlock_destroy(&ctx->lock);
err_rwlock_init_fail:
    nng_aio_free(ctx->accept_aio);
err_alloc_accept_aio:
    nng_stream_listener_close(ctx->listener);
err_listen:
    nng_stream_listener_free(ctx->listener);
    return rv;
}

static void websocket_ctx_fini(websocket_ctx_t *ctx)
{
    nng_aio_stop(ctx->accept_aio);
    nng_aio_free(ctx->accept_aio);
    ctx->accept_aio = NULL;

    nng_stream_listener_close(ctx->listener);
    nng_stream_listener_free(ctx->listener);
    ctx->listener = NULL;

    pthread_rwlock_wrlock(&ctx->lock);
    if (NULL != ctx->stream) {
        websocket_stream_deref(ctx->stream);
        ctx->stream = NULL;
    }
    pthread_rwlock_unlock(&ctx->lock);

    pthread_rwlock_destroy(&ctx->lock);
}

// NOTE: Do not call log functions otherwise recursion will happen.
static inline thread_ctx_t *websocket_ctx_get_thread_ctx(websocket_ctx_t *ctx)
{
    thread_ctx_t *      thread_ctx = NULL;
    websocket_stream_t *stream     = NULL;

    thread_ctx = pthread_getspecific(ctx->thread_key);

    pthread_rwlock_rdlock(&ctx->lock);
    if (NULL == ctx->stream) {
        pthread_rwlock_unlock(&ctx->lock);
        if (NULL != thread_ctx) {
            websocket_ctx_del_thread_ctx(ctx, thread_ctx);
        }
        return NULL;
    }

    if (NULL == thread_ctx || NULL == thread_ctx->stream) {
        stream = websocket_stream_ref(ctx->stream);
        if (NULL != thread_ctx) {
            thread_ctx->stream = stream;
        }
    } else if (thread_ctx->stream != ctx->stream) {
        websocket_stream_deref(thread_ctx->stream);
        thread_ctx->stream = websocket_stream_ref(ctx->stream);
    }
    pthread_rwlock_unlock(&ctx->lock);

    if (NULL == thread_ctx) {
        thread_ctx = thread_ctx_new();
        if (NULL == thread_ctx) {
            websocket_stream_deref(stream);
            return NULL;
        }
        thread_ctx->stream = stream;
        pthread_setspecific(ctx->thread_key, thread_ctx);
    }

    return thread_ctx;
}

// NOTE: Do not call log functions otherwise recursion will happen.
static inline void websocket_ctx_del_thread_ctx(websocket_ctx_t *ctx,
                                                thread_ctx_t *   thread_ctx)
{
    pthread_rwlock_wrlock(&ctx->lock);
    if (thread_ctx->stream == ctx->stream) {
        websocket_stream_deref(ctx->stream);
        ctx->stream = NULL;
    }
    pthread_rwlock_unlock(&ctx->lock);

    websocket_stream_deref(thread_ctx->stream);
    thread_ctx->stream = NULL;
    thread_ctx_free(thread_ctx);
    pthread_setspecific(ctx->thread_key, NULL);
}

// NOTE: Do not call log functions otherwise recursion will happen.
static inline int websocket_ctx_send(websocket_ctx_t *ctx, const void *buf,
                                     size_t size)
{
    int           rv         = 0;
    thread_ctx_t *thread_ctx = NULL;

    thread_ctx = websocket_ctx_get_thread_ctx(ctx);
    if (NULL == thread_ctx) {
        return -1;
    }

    nng_aio *aio = thread_ctx->aio;

    nng_iov iov = {
        .iov_buf = (void *) buf,
        .iov_len = size,
    };
    nng_aio_set_iov(aio, 1, &iov);
    nng_stream_send(thread_ctx->stream->s, aio);
    nng_aio_wait(aio);

    rv = nng_aio_result(aio);
    if (0 != rv) {
        PRINT("stream send fail: %s\n", nng_strerror(rv));
        websocket_ctx_del_thread_ctx(ctx, thread_ctx);
    }

    return rv;
}

// WebSocket for logging.
// Unfortunately, it has to be a global variable due to limitations of zlog.
static websocket_ctx_t g_log_ws_ctx_;

// NOTE: Do not call log functions otherwise recursion will happen.
static int zlog_output_websocket(zlog_msg_t *msg)
{
    int rv = 0;

    rv = websocket_ctx_send(&g_log_ws_ctx_, msg->buf, msg->len);

    return rv;
}

// clean up for main thread
void log_websocket_clean()
{
    thread_ctx_t *thread_ctx = pthread_getspecific(g_log_ws_ctx_.thread_key);
    thread_ctx_free(thread_ctx);
}

int log_websocket_start(const char *url)
{
    int rv = 0;

    rv = websocket_ctx_init(&g_log_ws_ctx_, url);
    if (0 != rv) {
        nlog_error("log websocket ctx init fail");
        return rv;
    }

    // NOTE: hereafter, do not call log functions.
    rv = zlog_set_record("ws", zlog_output_websocket);
    if (0 != rv) {
        PRINT("set websocket zlog record fail\n");
        websocket_ctx_fini(&g_log_ws_ctx_);
        return rv;
    }

    rv = atexit(log_websocket_clean);
    if (0 != rv) {
        PRINT("atexit log websocket clean fail\n");
        websocket_ctx_fini(&g_log_ws_ctx_);
    }

    return rv;
}

void log_websocket_stop()
{
    websocket_ctx_fini(&g_log_ws_ctx_);
}
