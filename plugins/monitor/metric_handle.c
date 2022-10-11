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
#include "restful/handle.h"
#include "restful/http.h"

#define METRIC_GLOBAL_TMPL                                                 \
    "# HELP core_dumped Whether there is any core dump\n"                  \
    "# TYPE core_dumped counter\n"                                         \
    "core_dumped %d\n"                                                     \
    "# HELP uptime_seconds Uptime in seconds\n"                            \
    "# TYPE uptime_seconds counter\n"                                      \
    "uptime_seconds %" PRIu64 "\n"                                         \
    "# HELP north_nodes Number of north nodes\n"                           \
    "# TYPE north_nodes counter\n"                                         \
    "north_nodes %zu\n"                                                    \
    "# HELP north_running_nodes Number of north nodes in running state\n"  \
    "# TYPE north_running_nodes counter\n"                                 \
    "north_running_nodes %zu\n"                                            \
    "# HELP north_disconnected_nodes Number of north nodes disconnected\n" \
    "# TYPE north_disconnected_nodes counter\n"                            \
    "north_disconnected_nodes %zu\n"                                       \
    "# HELP south_nodes Number of south nodes\n"                           \
    "# TYPE south_nodes counter\n"                                         \
    "south_nodes %zu\n"                                                    \
    "# HELP south_running_nodes Number of south nodes in running state\n"  \
    "# TYPE south_running_nodes counter\n"                                 \
    "south_running_nodes %zu\n"                                            \
    "# HELP south_disconnected_nodes Number of south nodes disconnected\n" \
    "# TYPE south_disconnected_nodes counter\n"                            \
    "south_disconnected_nodes %zu\n"

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
    }

    nng_http_res_set_status(res, status);

    nng_http_req *nng_req = nng_aio_get_input(aio, 0);
    nlog_notice("%s %s [%d]", nng_http_req_get_method(nng_req),
                nng_http_req_get_uri(nng_req), status);

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);

    return 0;
}

void handle_get_metric(nng_aio *aio)
{
    char *result = NULL;

    neu_asprintf(&result, METRIC_GLOBAL_TMPL, 0, 0, 0, 0, 0, 0, 0, 0);
    if (NULL == result) {
        nng_aio_finish(aio, NNG_EINTERNAL);
        return;
    }

    response(aio, result, NNG_HTTP_STATUS_OK);
    free(result);
}
