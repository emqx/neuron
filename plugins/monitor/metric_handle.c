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

#include "utils/asprintf.h"
#include "utils/log.h"

#include "metric_handle.h"
#include "monitor.h"
#include "restful/handle.h"
#include "restful/http.h"

// clang-format off
#define METRIC_GLOBAL_TMPL                                                       \
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

#define METRIC_LAST_RTT_MS_HELP                                         \
    "# HELP last_rtt_ms Last request round trip time in milliseconds\n" \
    "# TYPE last_rtt_ms gauge\n"
#define METRIC_LAST_RTT_MS_TMPL "last_rtt_ms{node=\"%s\"} %" PRIu64 "\n"

#define METRIC_TAG_READS_TOTAL_HELP                                       \
    "# HELP tag_reads_total Total number of tag reads including errors\n" \
    "# TYPE tag_reads_total counter\n"
#define METRIC_TAG_READS_TOTAL_TMPL "tag_reads_total{node=\"%s\"} %" PRIu64 "\n"

#define METRIC_TAG_READ_ERRORS_TOTAL_HELP                            \
    "# HELP tag_read_errors_total Total number of tag read errors\n" \
    "# TYPE tag_read_errors_total counter\n"
#define METRIC_TAG_READ_ERRORS_TOTAL_TMPL \
    "tag_read_errors_total{node=\"%s\"} %" PRIu64 "\n"

#define METRIC_SEND_BYTES_HELP                       \
    "# HELP send_bytes Total number of bytes sent\n" \
    "# TYPE send_bytes counter\n"
#define METRIC_SEND_BYTES_TMPL "send_bytes{node=\"%s\"} %" PRIu64 "\n"

#define METRIC_RECV_BYTES_HELP                           \
    "# HELP recv_bytes Total number of bytes received\n" \
    "# TYPE recv_bytes counter\n"
#define METRIC_RECV_BYTES_TMPL "recv_bytes{node=\"%s\"} %" PRIu64 "\n"

#define METRIC_DRIVER_TMPL            \
    METRIC_LAST_RTT_MS_HELP           \
    METRIC_LAST_RTT_MS_TMPL           \
    METRIC_TAG_READS_TOTAL_HELP       \
    METRIC_TAG_READS_TOTAL_TMPL       \
    METRIC_TAG_READ_ERRORS_TOTAL_HELP \
    METRIC_TAG_READ_ERRORS_TOTAL_TMPL \
    METRIC_SEND_BYTES_HELP            \
    METRIC_SEND_BYTES_TMPL            \
    METRIC_RECV_BYTES_HELP            \
    METRIC_RECV_BYTES_TMPL

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
    nlog_notice("%s %s [%d]", nng_http_req_get_method(nng_req),
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

static inline void gen_global_metrics(FILE *               stream,
                                      const neu_metrics_t *metrics)
{
    fprintf(stream, METRIC_GLOBAL_TMPL, metrics->core_dumped,
            metrics->uptime_seconds, metrics->north_nodes,
            metrics->north_running_nodes, metrics->north_disconnected_nodes,
            metrics->south_nodes, metrics->south_running_nodes,
            metrics->south_disconnected_nodes);
}

static inline void gen_driver_metrics(FILE *                    stream,
                                      const neu_node_metrics_t *node_metrics)
{
    fprintf(stream, METRIC_DRIVER_TMPL, node_metrics->name,
            node_metrics->driver.last_rtt_ms, node_metrics->name,
            node_metrics->driver.tag_reads_total, node_metrics->name,
            node_metrics->driver.tag_errors_total, node_metrics->name,
            node_metrics->driver.send_bytes, node_metrics->name,
            node_metrics->driver.recv_bytes);
}

#define GEN_DRIVER_METRIC(metric, field)                                    \
    do {                                                                    \
        fprintf(stream, "%s", metric##_HELP);                               \
        neu_node_metrics_t *el = NULL, *tmp = NULL;                         \
        HASH_ITER(hh, metrics->node_metrics, el, tmp)                       \
        {                                                                   \
            if (NEU_NA_TYPE_DRIVER == el->type) {                           \
                fprintf(stream, metric##_TMPL, el->name, el->driver.field); \
            }                                                               \
        }                                                                   \
    } while (0)

static void gen_all_driver_metrics(FILE *stream, const neu_metrics_t *metrics)
{
    GEN_DRIVER_METRIC(METRIC_LAST_RTT_MS, last_rtt_ms);
    GEN_DRIVER_METRIC(METRIC_TAG_READS_TOTAL, tag_reads_total);
    GEN_DRIVER_METRIC(METRIC_TAG_READ_ERRORS_TOTAL, tag_errors_total);
    GEN_DRIVER_METRIC(METRIC_SEND_BYTES, send_bytes);
    GEN_DRIVER_METRIC(METRIC_RECV_BYTES, recv_bytes);
}

void handle_get_metric(nng_aio *aio)
{
    int           status = NNG_HTTP_STATUS_OK;
    char *        result = NULL;
    size_t        len    = 0;
    FILE *        stream = NULL;
    neu_plugin_t *plugin = neu_monitor_get_plugin();

    neu_metrics_category_e cat           = NEU_METRICS_CATEGORY_ALL;
    size_t                 cat_param_len = 0;
    const char *cat_param = http_get_param(aio, "category", &cat_param_len);
    if (NULL != cat_param &&
        !parse_metrics_catgory(cat_param, cat_param_len, &cat)) {
        plog_info(plugin, "invalid metrics category: %.*s", (int) cat_param_len,
                  cat_param);
        status = NNG_HTTP_STATUS_BAD_REQUEST;
        goto end;
    }

    neu_node_metrics_t *node_metrics                 = NULL;
    char                node_name[NEU_NODE_NAME_LEN] = { 0 };
    ssize_t rv = http_get_param_str(aio, "node", node_name, sizeof(node_name));
    if (-1 == rv || rv >= NEU_NODE_NAME_LEN) {
        status = NNG_HTTP_STATUS_BAD_REQUEST;
        goto end;
    } else if (-2 != rv) {
        HASH_FIND_STR(plugin->metrics.node_metrics, node_name, node_metrics);
        if ((NEU_METRICS_CATEGORY_GLOBAL == cat) || NULL == node_metrics ||
            (NEU_METRICS_CATEGORY_APP == cat &&
             NEU_NA_TYPE_APP != node_metrics->type) ||
            (NEU_METRICS_CATEGORY_DRIVER == cat &&
             NEU_NA_TYPE_DRIVER != node_metrics->type)) {
            status = NNG_HTTP_STATUS_NOT_FOUND;
            goto end;
        }
    }

    stream = open_memstream(&result, &len);
    if (NULL == stream) {
        status = NNG_HTTP_STATUS_INTERNAL_SERVER_ERROR;
        goto end;
    }

    pthread_mutex_lock(&plugin->mutex);
    if (plugin->metrics_updating) {
        // a metrics request to core is in progress
        // in the assumption that this case is rare, we just return unavailable
        pthread_mutex_unlock(&plugin->mutex);
        status = NNG_HTTP_STATUS_SERVICE_UNAVAILABLE;
        goto end;
    }
    switch (cat) {
    case NEU_METRICS_CATEGORY_GLOBAL:
        gen_global_metrics(stream, &plugin->metrics);
        break;
    case NEU_METRICS_CATEGORY_DRIVER:
        if (NULL != node_metrics) {
            gen_driver_metrics(stream, node_metrics);
        } else {
            gen_all_driver_metrics(stream, &plugin->metrics);
        }
        break;
    case NEU_METRICS_CATEGORY_APP:
    case NEU_METRICS_CATEGORY_ALL:
        break;
    }
    pthread_mutex_unlock(&plugin->mutex);

end:
    if (NULL != stream) {
        fclose(stream);
    }
    response(aio, NNG_HTTP_STATUS_OK == status ? result : NULL, status);
    free(result);
}
