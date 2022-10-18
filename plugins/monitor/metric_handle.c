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

static inline void gen_global_metrics(const neu_metrics_t *metrics,
                                      char **              result)
{
    neu_asprintf(result, METRIC_GLOBAL_TMPL, metrics->core_dumped,
                 metrics->uptime_seconds, metrics->north_nodes,
                 metrics->north_running_nodes,
                 metrics->north_disconnected_nodes, metrics->south_nodes,
                 metrics->south_running_nodes,
                 metrics->south_disconnected_nodes);
}

void handle_get_metric(nng_aio *aio)
{
    char *        result = NULL;
    neu_plugin_t *plugin = neu_monitor_get_plugin();

    neu_metrics_category_e cat           = NEU_METRICS_CATEGORY_ALL;
    size_t                 cat_param_len = 0;
    const char *cat_param = http_get_param(aio, "category", &cat_param_len);
    if (NULL != cat_param &&
        !parse_metrics_catgory(cat_param, cat_param_len, &cat)) {
        plog_info(plugin, "invalid metrics category: %.*s", (int) cat_param_len,
                  cat_param);
        response(aio, NULL, NNG_HTTP_STATUS_BAD_REQUEST);
        return;
    }

    pthread_mutex_lock(&plugin->mutex);
    if (plugin->metrics_updating) {
        // a metrics request to core is in progress
        // in the assumption that this case is rare, we just finish error
        pthread_mutex_unlock(&plugin->mutex);
        nng_aio_finish(aio, NNG_EBUSY);
        response(aio, NULL, NNG_HTTP_STATUS_SERVICE_UNAVAILABLE);
        return;
    }
    switch (cat) {
    case NEU_METRICS_CATEGORY_GLOBAL:
        gen_global_metrics(&plugin->metrics, &result);
        break;
    case NEU_METRICS_CATEGORY_DRIVER:
    case NEU_METRICS_CATEGORY_APP:
    case NEU_METRICS_CATEGORY_ALL:
        break;
    }
    pthread_mutex_unlock(&plugin->mutex);

    if (NULL == result) {
        response(aio, NULL, NNG_HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }

    response(aio, result, NNG_HTTP_STATUS_OK);
    free(result);
}
