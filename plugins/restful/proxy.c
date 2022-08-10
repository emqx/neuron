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

#include "proxy.h"
#include "errcodes.h"
#include "http.h"
#include "utils/log.h"
#include "utils/utlist.h"
#include "json/neu_json_fn.h"

struct rest_proxy_ctx;

typedef struct {
    size_t                 src_uri_len; // source uri length
    size_t                 dst_uri_len; // destination uri length
    size_t                 uri_buf_len; // request uri buffer length
    char *                 uri_buf;     // request uri buffer
    nng_http_client *      client;      // client to destination
    struct rest_proxy_ctx *free_list;   // context free list
} rest_proxy_t;

struct rest_proxy_ctx {
    rest_proxy_t *         proxy;
    nng_aio *              aio;
    nng_aio *              user_aio; // aio to write back response
    nng_http_res *         res;      // http response from the destination
    struct rest_proxy_ctx *prev;
    struct rest_proxy_ctx *next;
};

static void rest_proxy_ctx_free(struct rest_proxy_ctx *ctx)
{
    if (NULL == ctx) {
        return;
    }
    nng_aio_free(ctx->aio);
}

static int rest_proxy_alloc(rest_proxy_t **proxy, const char *src_url,
                            const char *dst_url)
{
    int      ret = 0;
    nng_url *url = NULL;

    rest_proxy_t *p = calloc(1, sizeof(rest_proxy_t));
    if (NULL == p) {
        return -1;
    }

    ret = nng_url_parse(&url, dst_url);
    if (0 != ret) {
        free(p);
        nlog_error("parse url: `%s` fail", dst_url);
        return ret;
    }

    p->dst_uri_len = strlen(url->u_requri);
    p->uri_buf_len = p->dst_uri_len + 1;
    p->uri_buf     = calloc(p->uri_buf_len, sizeof(char));
    if (NULL == p->uri_buf) {
        nng_url_free(url);
        free(p);
        nlog_error("alloc uri buffer for `%s` fail", dst_url);
        return -1;
    }
    // copy destination uri to head of buffer
    strcpy(p->uri_buf, url->u_requri);

    ret = nng_http_client_alloc(&p->client, url);
    if (0 != ret) {
        free(p->uri_buf);
        nng_url_free(url);
        free(p);
        nlog_error("alloc http client for `%s` fail", dst_url);
        return ret;
    }

    p->src_uri_len = strlen(src_url);
    *proxy         = p;
    nng_url_free(url);
    return ret;
}

static void rest_proxy_free(rest_proxy_t *proxy)
{
    if (NULL == proxy) {
        return;
    }

    if (proxy->client) {
        nng_http_client_free(proxy->client);
    }

    free(proxy->uri_buf);

    struct rest_proxy_ctx *el = NULL, *tmp = NULL;
    DL_FOREACH_SAFE(proxy->free_list, el, tmp)
    {
        DL_DELETE(proxy->free_list, el);
        nng_aio_free(el->aio);
        nng_aio_free(el->user_aio);
        free(el);
    }

    free(proxy);
}

static inline int rest_proxy_set_uri(rest_proxy_t *proxy, nng_http_req *req)
{
    const char *uri     = nng_http_req_get_uri(req);
    size_t      len     = strlen(uri);
    size_t      buf_len = len - proxy->src_uri_len + proxy->dst_uri_len + 1;

    if (buf_len > proxy->uri_buf_len) {
        proxy->uri_buf_len = buf_len;
        proxy->uri_buf     = realloc(proxy->uri_buf, buf_len);
        if (NULL == proxy->uri_buf) {
            nlog_error("realloc uri buffer fail");
            return -1;
        }
    }

    // buffer large enough guaranteed
    sprintf(proxy->uri_buf + proxy->dst_uri_len, "%s",
            uri + proxy->src_uri_len);

    if (0 != nng_http_req_set_uri(req, proxy->uri_buf)) {
        nlog_error("proxy fail set req uri: `%s`", proxy->uri_buf);
        return -1;
    }

    return 0;
}

static void rest_proxy_cb(void *arg)
{
    int                    rv  = 0;
    struct rest_proxy_ctx *ctx = arg;

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
    DL_APPEND(ctx->proxy->free_list, ctx); // put to free list
}

struct rest_proxy_ctx *rest_proxy_get_ctx(rest_proxy_t *proxy)
{
    struct rest_proxy_ctx *ctx = proxy->free_list;
    if (NULL != ctx) {
        DL_DELETE(proxy->free_list, ctx);
        return ctx;
    }

    ctx = calloc(1, sizeof(*ctx));
    if (NULL == ctx) {
        nlog_error("alloc proxy ctx fail");
        return NULL;
    }

    if (0 != nng_aio_alloc(&ctx->aio, rest_proxy_cb, ctx)) {
        nlog_error("alloc proxy ctx aio fail");
        free(ctx);
        return NULL;
    }

    ctx->proxy = proxy;
    return ctx;
}

int rest_proxy_handler(const struct neu_rest_handler *rest_handler,
                       nng_http_handler **            handler)
{
    int           ret   = 0;
    const char *  dst   = rest_handler->value.dst_url;
    nng_url *     url   = NULL;
    rest_proxy_t *proxy = NULL;

    ret = rest_proxy_alloc(&proxy, rest_handler->url,
                           rest_handler->value.dst_url);
    if (0 != ret) {
        return ret;
    }

    ret = nng_http_handler_alloc(handler, rest_handler->url, handle_proxy);
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
                                    (void (*)(void *)) rest_proxy_free);
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
        rest_proxy_free(proxy);
    }
    return ret;
}

void handle_proxy(nng_aio *aio)
{
    nng_http_res *    res     = NULL;
    nng_http_req *    req     = nng_aio_get_input(aio, 0);
    nng_http_handler *handler = nng_aio_get_input(aio, 1);
    rest_proxy_t *    proxy   = nng_http_handler_get_data(handler);

    // fix the uri
    if (0 != rest_proxy_set_uri(proxy, req)) {
        goto error;
    }

    struct rest_proxy_ctx *ctx = rest_proxy_get_ctx(proxy);
    if (NULL == ctx) {
        nlog_error("get proxy ctx fail");
        goto error;
    }

    if (0 != nng_http_res_alloc(&res)) {
        nlog_error("alloc proxy resp fail");
        rest_proxy_ctx_free(ctx);
        goto error;
    }

    ctx->user_aio = aio;
    ctx->res      = res;

    nng_http_client_transact(proxy->client, req, ctx->res, ctx->aio);
    return;

error:
    NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
        http_response(aio, error_code.error, result_error);
    });
}
