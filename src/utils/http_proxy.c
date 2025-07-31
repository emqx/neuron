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
#include <pthread.h>

#include "errcodes.h"
#include "utils/http.h"
#include "utils/http_handler.h"
#include "utils/log.h"
#include "utils/utlist.h"
#include "json/neu_json_fn.h"

struct http_proxy_ctx;

/**
 * @brief 处理代理请求的回调函数
 * @param aio NNG异步I/O对象
 */
static void handle_proxy(nng_aio *aio);

typedef struct {
    pthread_mutex_t        mtx;
    size_t                 src_uri_len; // source uri length
    size_t                 dst_uri_len; // destination uri length
    size_t                 uri_buf_len; // request uri buffer length
    char *                 uri_buf;     // request uri buffer
    nng_http_client *      client;      // client to destination
    struct http_proxy_ctx *free_list;   // context free list
} http_proxy_t;

struct http_proxy_ctx {
    http_proxy_t *         proxy;
    nng_aio *              aio;
    nng_aio *              user_aio; // aio to write back response
    nng_http_res *         res;      // http response from the destination
    struct http_proxy_ctx *prev;
    struct http_proxy_ctx *next;
};

/**
 * @brief 释放HTTP代理上下文
 * @param ctx 要释放的HTTP代理上下文
 */
static void http_proxy_ctx_free(struct http_proxy_ctx *ctx)
{
    if (NULL == ctx) {
        return;
    }
    nng_aio_free(ctx->aio);
}

/**
 * @brief 分配并初始化HTTP代理
 * @param proxy 输出参数，指向分配的HTTP代理
 * @param src_url 源URL
 * @param dst_url 目标URL
 * @return 成功返回0，失败返回负数
 */
static int http_proxy_alloc(http_proxy_t **proxy, const char *src_url,
                            const char *dst_url)
{
    int      ret = 0;
    nng_url *url = NULL;

    http_proxy_t *p = calloc(1, sizeof(http_proxy_t));
    if (NULL == p) {
        nlog_error("alloc http proxy fail");
        return -1;
    }

    if (0 != pthread_mutex_init(&p->mtx, NULL)) {
        nlog_error("init http proxy mutex fail");
        ret = -1;
        goto err_mutex_init;
    }

    ret = nng_url_parse(&url, dst_url);
    if (0 != ret) {
        nlog_error("parse url: `%s` fail", dst_url);
        goto err_url_parse;
    }

    p->dst_uri_len = strlen(url->u_requri);
    p->uri_buf_len = p->dst_uri_len + 1;
    p->uri_buf     = calloc(p->uri_buf_len, sizeof(char));
    if (NULL == p->uri_buf) {
        nlog_error("alloc uri buffer for `%s` fail", dst_url);
        ret = -1;
        goto err_uri_buf;
    }
    // copy destination uri to head of buffer
    strcpy(p->uri_buf, url->u_requri);

    ret = nng_http_client_alloc(&p->client, url);
    if (0 != ret) {
        nlog_error("alloc http client for `%s` fail", dst_url);
        goto err_http_client;
    }

    p->src_uri_len = strlen(src_url);
    *proxy         = p;
    nng_url_free(url);
    return ret;

err_http_client:
    free(p->uri_buf);
err_uri_buf:
    nng_url_free(url);
err_url_parse:
    pthread_mutex_destroy(&p->mtx);
err_mutex_init:
    free(p);
    return ret;
}

/**
 * @brief 释放HTTP代理资源
 * @param proxy 要释放的HTTP代理
 */
static void http_proxy_free(http_proxy_t *proxy)
{
    if (NULL == proxy) {
        return;
    }

    if (proxy->client) {
        nng_http_client_free(proxy->client);
    }

    free(proxy->uri_buf);

    struct http_proxy_ctx *el = NULL, *tmp = NULL;
    DL_FOREACH_SAFE(proxy->free_list, el, tmp)
    {
        DL_DELETE(proxy->free_list, el);
        nng_aio_free(el->aio);
        nng_aio_free(el->user_aio);
        free(el);
    }

    pthread_mutex_destroy(&proxy->mtx);
    free(proxy);
}

/**
 * @brief 设置HTTP请求的URI，替换源URI为目标URI
 * @param proxy HTTP代理
 * @param req HTTP请求
 * @return 成功返回0，失败返回-1
 */
static inline int http_proxy_set_uri(http_proxy_t *proxy, nng_http_req *req)
{
    const char *uri     = nng_http_req_get_uri(req);
    size_t      len     = strlen(uri);
    size_t      buf_len = len - proxy->src_uri_len + proxy->dst_uri_len + 1;

    // TODO: use per context buffer if a single lock cause too much contention
    pthread_mutex_lock(&proxy->mtx);
    if (buf_len > proxy->uri_buf_len) {
        proxy->uri_buf_len = buf_len;
        proxy->uri_buf     = realloc(proxy->uri_buf, buf_len);
        if (NULL == proxy->uri_buf) {
            nlog_error("realloc uri buffer fail");
            pthread_mutex_unlock(&proxy->mtx);
            return -1;
        }
    }

    // buffer large enough guaranteed
    sprintf(proxy->uri_buf + proxy->dst_uri_len, "%s",
            uri + proxy->src_uri_len);

    if (0 != nng_http_req_set_uri(req, proxy->uri_buf)) {
        nlog_error("proxy fail set req uri: `%s`", proxy->uri_buf);
        pthread_mutex_unlock(&proxy->mtx);
        return -1;
    }
    pthread_mutex_unlock(&proxy->mtx);

    return 0;
}

/**
 * @brief HTTP代理回调函数，处理从目标服务器返回的响应
 * @param arg 回调参数，HTTP代理上下文
 */
static void http_proxy_cb(void *arg)
{
    int                    rv  = 0;
    struct http_proxy_ctx *ctx = arg;

    nng_aio_set_output(ctx->user_aio, 0, ctx->res);

    if (0 != (rv = nng_aio_result(ctx->aio))) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            if (0 !=
                    (rv = nng_http_res_set_status(
                         ctx->res, NNG_HTTP_STATUS_SERVICE_UNAVAILABLE)) ||
                0 !=
                    (rv = nng_http_res_set_header(ctx->res, "Content-Type",
                                                  "application/json")) ||
                0 !=
                    (rv = nng_http_res_copy_data(ctx->res, result_error,
                                                 strlen(result_error)))) {
                nng_aio_set_output(ctx->user_aio, 0, NULL);
                nng_http_res_free(ctx->res);
            }
        });
    }

    nng_aio_finish(ctx->user_aio, rv);

    ctx->user_aio = NULL;
    ctx->res      = NULL;

    pthread_mutex_lock(&ctx->proxy->mtx);
    DL_APPEND(ctx->proxy->free_list, ctx); // put to free list
    pthread_mutex_unlock(&ctx->proxy->mtx);
}

/**
 * @brief 获取HTTP代理上下文，优先从空闲列表获取，无可用时创建新的
 * @param proxy HTTP代理
 * @return 成功返回HTTP代理上下文指针，失败返回NULL
 */
static struct http_proxy_ctx *http_proxy_get_ctx(http_proxy_t *proxy)
{
    pthread_mutex_lock(&proxy->mtx);
    struct http_proxy_ctx *ctx = proxy->free_list;
    if (NULL != ctx) {
        DL_DELETE(proxy->free_list, ctx);
        pthread_mutex_unlock(&proxy->mtx);
        return ctx;
    }
    pthread_mutex_unlock(&proxy->mtx);

    ctx = calloc(1, sizeof(*ctx));
    if (NULL == ctx) {
        nlog_error("alloc proxy ctx fail");
        return NULL;
    }

    if (0 != nng_aio_alloc(&ctx->aio, http_proxy_cb, ctx)) {
        nlog_error("alloc proxy ctx aio fail");
        free(ctx);
        return NULL;
    }

    ctx->proxy = proxy;
    return ctx;
}

/**
 * @brief 创建HTTP代理处理器
 * @param http_handler HTTP处理器配置
 * @param handler 输出参数，指向创建的HTTP处理器
 * @return 成功返回0，失败返回错误码
 */
int neu_http_proxy_handler(const struct neu_http_handler *http_handler,
                           nng_http_handler **            handler)
{
    int           ret   = 0;
    const char *  dst   = http_handler->value.dst_url;
    nng_url *     url   = NULL;
    http_proxy_t *proxy = NULL;

    ret = http_proxy_alloc(&proxy, http_handler->url,
                           http_handler->value.dst_url);
    if (0 != ret) {
        return ret;
    }

    ret = nng_http_handler_alloc(handler, http_handler->url, handle_proxy);
    if (0 != ret) {
        nlog_error("alloc proxy handler for `%s` fail", dst);
        goto error;
    }

    ret = nng_http_handler_set_tree(*handler);
    if (0 != ret) {
        nlog_error("proxy handler for `%s` set tree fail", dst);
        goto error;
    }

    ret = nng_http_handler_set_method(*handler, NULL);
    if (0 != ret) {
        nlog_error("proxy handler for `%s` set all method fail", dst);
        goto error;
    }

    ret = nng_http_handler_set_data(*handler, proxy,
                                    (void (*)(void *)) http_proxy_free);
    if (0 != ret) {
        nlog_error("proxy handler for `%s` set data fail", dst);
        goto error;
    }

    return ret;

error:
    if (NULL != url) {
        nng_url_free(url);
    }
    if (NULL != *handler) {
        nng_http_handler_free(*handler);
    }
    if (NULL != proxy) {
        http_proxy_free(proxy);
    }
    return ret;
}

/**
 * @brief 处理HTTP代理请求
 * @param aio NNG异步I/O对象
 */
static void handle_proxy(nng_aio *aio)
{
    nng_http_res *    res     = NULL;
    nng_http_req *    req     = nng_aio_get_input(aio, 0);
    nng_http_handler *handler = nng_aio_get_input(aio, 1);
    http_proxy_t *    proxy   = nng_http_handler_get_data(handler);

    // fix the uri
    if (0 != http_proxy_set_uri(proxy, req)) {
        goto error;
    }

    struct http_proxy_ctx *ctx = http_proxy_get_ctx(proxy);
    if (NULL == ctx) {
        nlog_error("get proxy ctx fail");
        goto error;
    }

    if (0 != nng_http_res_alloc(&res)) {
        nlog_error("alloc proxy resp fail");
        http_proxy_ctx_free(ctx);
        goto error;
    }

    ctx->user_aio = aio;
    ctx->res      = res;

    nng_http_client_transact(proxy->client, req, ctx->res, ctx->aio);
    return;

error:
    NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
        neu_http_response(aio, error_code.error, result_error);
    });
}
