/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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

#include <pthread.h>

#include "define.h"
#include "metrics.h"
#include "plugin.h"
#include "utils/asprintf.h"
#include "utils/http.h"
#include "utils/http_handler.h"
#include "utils/log.h"

#include "metric_handle.h"

// clang-format off
#define METRIC_GLOBAL_TMPL                                                       \
    "# HELP os_info OS distro and kernel version\n"                              \
    "# TYPE os_info gauge\n"                                                     \
    "os_info{version=\"%s\"} 0\n"                                                \
    "os_info{kernel=\"%s\"} 0\n"                                                 \
    "# HELP cpu_percent Total CPU utilisation percentage\n"                      \
    "# TYPE cpu_percent gauge\n"                                                 \
    "cpu_percent %u\n"                                                           \
    "# HELP cpu_cores Number of CPU cores\n"                                     \
    "# TYPE cpu_cores counter\n"                                                 \
    "cpu_cores %u\n"                                                             \
    "# HELP mem_total_bytes Total installed memory in bytes\n"                   \
    "# TYPE mem_total_bytes counter\n"                                           \
    "mem_total_bytes %zu\n"                                                      \
    "# HELP mem_used_bytes Used memory in bytes\n"                               \
    "# TYPE mem_used_bytes gauge\n"                                              \
    "mem_used_bytes %zu\n"                                                       \
    "# HELP mem_cache Memory buffer/cache size in bytes\n"                       \
    "# TYPE mem_cache gauge\n"                                                   \
    "mem_cache_bytes %zu\n"                                                      \
    "# HELP rss_bytes RSS (Resident Set Size) in bytes\n"                        \
    "# TYPE rss_bytes gauge\n"                                                   \
    "rss_bytes %zu\n"                                                            \
    "# HELP disk_size_gibibytes Disk size in gibibytes\n"                        \
    "# TYPE disk_size_gibibytes counter\n"                                       \
    "disk_size_gibibytes %zu\n"                                                  \
    "# HELP disk_used_gibibytes Used disk size in gibibytes\n"                   \
    "# TYPE disk_used_gibibytes gauge\n"                                         \
    "disk_used_gibibytes %zu\n"                                                  \
    "# HELP disk_avail_gibibytes Available disk size in gibibytes\n"             \
    "# TYPE disk_avail_gibibytes gauge\n"                                        \
    "disk_avail_gibibytes %zu\n"                                                 \
    "# HELP core_dumped Whether there is any core dump\n"                        \
    "# TYPE core_dumped gauge\n"                                                 \
    "core_dumped %d\n"                                                           \
    "# HELP uptime_seconds Uptime in seconds\n"                                  \
    "# TYPE uptime_seconds counter\n"                                            \
    "uptime_seconds %" PRIu64 "\n"                                               \
    "# HELP north_nodes_total Number of north nodes\n"                           \
    "# TYPE north_nodes_total gauge\n"                                           \
    "north_nodes_total %zu\n"                                                    \
    "# HELP north_running_nodes_total Number of north nodes in running state\n"  \
    "# TYPE north_running_nodes_total gauge\n"                                   \
    "north_running_nodes_total %zu\n"                                            \
    "# HELP north_disconnected_nodes_total Number of north nodes disconnected\n" \
    "# TYPE north_disconnected_nodes_total gauge\n"                              \
    "north_disconnected_nodes_total %zu\n"                                       \
    "# HELP south_nodes_total Number of south nodes\n"                           \
    "# TYPE south_nodes_total gauge\n"                                           \
    "south_nodes_total %zu\n"                                                    \
    "# HELP south_running_nodes_total Number of south nodes in running state\n"  \
    "# TYPE south_running_nodes_total gauge\n"                                   \
    "south_running_nodes_total %zu\n"                                            \
    "# HELP south_disconnected_nodes_total Number of south nodes disconnected\n" \
    "# TYPE south_disconnected_nodes_total gauge\n"                              \
    "south_disconnected_nodes_total %zu\n"
// clang-format on

static int response(nng_aio *aio, char *content, enum nng_http_status status)
{
    nng_http_res *res = NULL;

    nng_http_res_alloc(&res);

    nng_http_res_set_header(res, "Content-Type", "text/plain");
    nng_http_res_set_header(res, "Access-Control-Allow-Origin", "*");
    nng_http_res_set_header(res, "Access-Control-Allow-Methods",
                            "POST,GET,PUT,DELETE,OPTIONS");
    nng_http_res_set_header(res, "Access-Control-Allow-Headers", "*");

    if (content != NULL && strlen(content) > 0) {
        nng_http_res_copy_data(res, content, strlen(content));
    } else {
        nng_http_res_set_data(res, NULL, 0);
    }

    nng_http_res_set_status(res, status);

    nng_http_req *nng_req = nng_aio_get_input(aio, 0);
    nlog_info("%s %s [%d]", nng_http_req_get_method(nng_req),
              nng_http_req_get_uri(nng_req), status);

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);

    return 0;
}

static inline bool parse_metrics_catgory(const char *s, size_t len,
                                         neu_metrics_category_e *cat)
{
    char *domain[] = {
        [NEU_METRICS_CATEGORY_GLOBAL] = "global",
        [NEU_METRICS_CATEGORY_DRIVER] = "driver",
        [NEU_METRICS_CATEGORY_APP]    = "app",
        [NEU_METRICS_CATEGORY_ALL]    = NULL,
    };

    if (NULL == s || NULL == cat) {
        return false;
    }

    for (int i = 0; i < NEU_METRICS_CATEGORY_ALL; ++i) {
        if (strlen(domain[i]) == len && 0 == strncmp(s, domain[i], len)) {
            *cat = i;
            return true;
        }
    }

    return false;
}

static inline void gen_global_metrics(const neu_metrics_t *metrics,
                                      FILE *               stream)
{
    fprintf(stream, METRIC_GLOBAL_TMPL, metrics->distro, metrics->kernel,
            metrics->cpu_percent, metrics->cpu_cores, metrics->mem_total_bytes,
            metrics->mem_used_bytes, metrics->mem_cache_bytes,
            metrics->mem_used_bytes, metrics->disk_size_gibibytes,
            metrics->disk_used_gibibytes, metrics->disk_avail_gibibytes,
            metrics->core_dumped, metrics->uptime_seconds, metrics->north_nodes,
            metrics->north_running_nodes, metrics->north_disconnected_nodes,
            metrics->south_nodes, metrics->south_running_nodes,
            metrics->south_disconnected_nodes);
}

static inline void gen_single_node_metrics(neu_node_metrics_t *node_metrics,
                                           FILE *              stream)
{
    fprintf(stream,
            "# HELP node_type Driver(1) or APP(2)\n"
            "# TYPE node_type gauge\n"
            "node_type{node=\"%s\"} %d\n",
            node_metrics->name, node_metrics->type);

    neu_metric_entry_t *e = NULL;

    pthread_mutex_lock(&node_metrics->lock);
    HASH_LOOP(hh, node_metrics->entries, e)
    {
        if (NEU_METRIC_TYPE_ROLLING_COUNTER == e->type) {
            // force clean stale value
            e->value = neu_rolling_counter_inc(e->rcnt, global_timestamp, 0);
        }
        fprintf(stream,
                "# HELP %s %s\n# TYPE %s %s\n%s{node=\"%s\"} %" PRIu64 "\n",
                e->name, e->help, e->name, neu_metric_type_str(e->type),
                e->name, node_metrics->name, e->value);
    }

    neu_group_metrics_t *g = NULL;
    HASH_LOOP(hh, node_metrics->group_metrics, g)
    {
        HASH_LOOP(hh, g->entries, e)
        {
            if (NEU_METRIC_TYPE_ROLLING_COUNTER == e->type) {
                // force clean stale value
                e->value =
                    neu_rolling_counter_inc(e->rcnt, global_timestamp, 0);
            }
            fprintf(stream,
                    "# HELP %s %s\n# TYPE %s %s\n%s{node=\"%s\",group=\"%s\"} "
                    "%" PRIu64 "\n",
                    e->name, e->help, e->name, neu_metric_type_str(e->type),
                    e->name, node_metrics->name, g->name, e->value);
        }
    }
    pthread_mutex_unlock(&node_metrics->lock);
}

static inline bool has_entry(neu_node_metrics_t *node_metrics, const char *name)
{
    neu_metric_entry_t * e = NULL;
    neu_group_metrics_t *g = NULL;
    HASH_FIND_STR(node_metrics->entries, name, e);
    if (e) {
        return true;
    }

    HASH_LOOP(hh, node_metrics->group_metrics, g)
    {
        HASH_FIND_STR(g->entries, name, e);
        if (e) {
            return true;
        }
    }
    return false;
}

static void gen_all_node_metrics(neu_metrics_t *metrics, int type_filter,
                                 FILE *stream)
{
    neu_metric_entry_t * e = NULL, *r = NULL;
    neu_group_metrics_t *g = NULL;
    neu_node_metrics_t * n = NULL;

    bool commented = false;
    HASH_LOOP(hh, metrics->node_metrics, n)
    {
        if (!(type_filter & n->type)) {
            continue;
        }

        if (!commented) {
            commented = true;
            fprintf(stream,
                    "# HELP node_type Driver(1) or APP(2)\n"
                    "# TYPE node_type gauge\n");
        }

        fprintf(stream, "node_type{node=\"%s\"} %d\n", n->name, n->type);
    }

    n = NULL;

    HASH_LOOP(hh, metrics->registered_metrics, r)
    {
        commented = false;
        HASH_LOOP(hh, metrics->node_metrics, n)
        {
            if (!(type_filter & n->type)) {
                continue;
            }

            if (!has_entry(n, r->name)) {
                continue;
            }

            if (!commented) {
                commented = true;
                fprintf(stream, "# HELP %s %s\n# TYPE %s %s\n", r->name,
                        r->help, r->name, neu_metric_type_str(r->type));
            }

            pthread_mutex_lock(&n->lock);
            HASH_FIND_STR(n->entries, r->name, e);
            if (e) {
                if (NEU_METRIC_TYPE_ROLLING_COUNTER == e->type) {
                    // force clean stale value
                    e->value =
                        neu_rolling_counter_inc(e->rcnt, global_timestamp, 0);
                }
                fprintf(stream, "%s{node=\"%s\"} %" PRIu64 "\n", e->name,
                        n->name, e->value);

                pthread_mutex_unlock(&n->lock);
                continue;
            }

            HASH_LOOP(hh, n->group_metrics, g)
            {
                HASH_FIND_STR(g->entries, r->name, e);
                if (e) {
                    if (NEU_METRIC_TYPE_ROLLING_COUNTER == e->type) {
                        // force clean stale value
                        e->value = neu_rolling_counter_inc(e->rcnt,
                                                           global_timestamp, 0);
                    }
                    fprintf(stream,
                            "%s{node=\"%s\",group=\"%s\"} %" PRIu64 "\n",
                            e->name, n->name, g->name, e->value);
                }
            }
            pthread_mutex_unlock(&n->lock);
        }
    }
}

struct context {
    int         filter;
    int *       status;
    FILE *      stream;
    const char *node;
};

static void gen_node_metrics(neu_metrics_t *metrics, struct context *ctx)
{
    if (ctx->node[0]) {
        neu_node_metrics_t *n = NULL;
        HASH_FIND_STR(metrics->node_metrics, ctx->node, n);
        if (NULL == n || 0 == (ctx->filter & n->type)) {
            *ctx->status = NNG_HTTP_STATUS_NOT_FOUND;
            return;
        }
        gen_single_node_metrics(n, ctx->stream);
    } else {
        gen_all_node_metrics(metrics, ctx->filter, ctx->stream);
    }
}

void handle_get_metric(nng_aio *aio)
{
    int    status = NNG_HTTP_STATUS_OK;
    char * result = NULL;
    size_t len    = 0;
    FILE * stream = NULL;

    neu_metrics_category_e cat           = NEU_METRICS_CATEGORY_ALL;
    size_t                 cat_param_len = 0;
    const char *cat_param = neu_http_get_param(aio, "category", &cat_param_len);
    if (NULL != cat_param &&
        !parse_metrics_catgory(cat_param, cat_param_len, &cat)) {
        nlog_error("invalid metrics category: %.*s", (int) cat_param_len,
                   cat_param);
        status = NNG_HTTP_STATUS_BAD_REQUEST;
        goto end;
    }

    char    node_name[NEU_NODE_NAME_LEN] = { 0 };
    ssize_t rv =
        neu_http_get_param_str(aio, "node", node_name, sizeof(node_name));
    if (-1 == rv || rv >= NEU_NODE_NAME_LEN ||
        (0 < rv && NEU_METRICS_CATEGORY_GLOBAL == cat)) {
        status = NNG_HTTP_STATUS_BAD_REQUEST;
        goto end;
    }

    stream = open_memstream(&result, &len);
    if (NULL == stream) {
        status = NNG_HTTP_STATUS_INTERNAL_SERVER_ERROR;
        goto end;
    }

    struct context ctx = {
        .status = &status,
        .stream = stream,
        .node   = node_name,
    };

    switch (cat) {
    case NEU_METRICS_CATEGORY_GLOBAL:
        neu_metrics_visist((neu_metrics_cb_t) gen_global_metrics, stream);
        break;
    case NEU_METRICS_CATEGORY_DRIVER:
        ctx.filter = NEU_NA_TYPE_DRIVER;
        neu_metrics_visist((neu_metrics_cb_t) gen_node_metrics, &ctx);
        break;
    case NEU_METRICS_CATEGORY_APP:
        ctx.filter = NEU_NA_TYPE_APP | NEU_NA_TYPE_NDRIVER;
        neu_metrics_visist((neu_metrics_cb_t) gen_node_metrics, &ctx);
        break;
    case NEU_METRICS_CATEGORY_ALL:
        ctx.filter = NEU_NA_TYPE_DRIVER | NEU_NA_TYPE_NDRIVER | NEU_NA_TYPE_APP;
        neu_metrics_visist((neu_metrics_cb_t) gen_global_metrics, stream);
        neu_metrics_visist((neu_metrics_cb_t) gen_node_metrics, &ctx);
        break;
    }

end:
    if (NULL != stream) {
        fclose(stream);
    }
    response(aio, NNG_HTTP_STATUS_OK == status ? result : NULL, status);
    free(result);
}
