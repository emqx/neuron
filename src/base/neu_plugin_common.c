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
#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "errcodes.h"
#include "otel/otel_manager.h"
#include "plugin.h"
#include "utils/http.h"
#include "utils/time.h"

#define NEU_PLUGIN_MAGIC_NUMBER 0x43474d50 // a string "PMGC"

void neu_plugin_common_init(neu_plugin_common_t *common)
{
    common->magic      = NEU_PLUGIN_MAGIC_NUMBER;
    common->link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
}

bool neu_plugin_common_check(neu_plugin_t *plugin)
{
    return neu_plugin_to_plugin_common(plugin)->magic == NEU_PLUGIN_MAGIC_NUMBER
        ? true
        : false;
}

int neu_plugin_op(neu_plugin_t *plugin, neu_reqresp_head_t head, void *data)
{
    neu_plugin_common_t *plugin_common = neu_plugin_to_plugin_common(plugin);
    int                  ret           = 0;

    neu_otel_trace_ctx trace = NULL;
    neu_otel_scope_ctx scope = NULL;

    if (head.otel_trace_type == NEU_OTEL_TRACE_TYPE_REST_COMM) {
        nng_aio *     aio         = (nng_aio *) head.ctx;
        nng_http_req *nng_req     = nng_aio_get_input(aio, 0);
        const char *  http_method = nng_http_req_get_method(nng_req);

        if (otel_flag && otel_control_flag) {
            const char *trace_parent =
                nng_http_req_get_header(nng_req, "traceparent");
            const char *trace_state =
                nng_http_req_get_header(nng_req, "tracestate");
            if (trace_parent) {
                char     trace_id[64] = { 0 };
                char     span_id[32]  = { 0 };
                uint32_t flags        = 0;
                neu_otel_split_traceparent(trace_parent, trace_id, span_id,
                                           &flags);
                if (strlen(trace_id) == 32) {
                    trace = neu_otel_create_trace(trace_id, head.ctx, flags,
                                                  trace_state);
                    scope = neu_otel_add_span(trace);
                    neu_otel_scope_set_span_name(scope,
                                                 nng_http_req_get_uri(nng_req));
                    char new_span_id[36] = { 0 };
                    neu_otel_new_span_id(new_span_id);
                    neu_otel_scope_set_span_id(scope, new_span_id);
                    if (strlen(span_id) == 16) {
                        neu_otel_scope_set_parent_span_id(scope, span_id);
                    }
                    neu_otel_scope_set_span_flags(scope, flags);
                    neu_otel_scope_set_span_start_time(scope, neu_time_ms());

                    neu_otel_scope_add_span_attr_int(scope, "thread id",
                                                     (int64_t) pthread_self());

                    neu_otel_scope_add_span_attr_string(scope, "HTTP method",
                                                        http_method);

                    if (strcmp(http_method, "POST") == 0) {
                        char * req_data      = NULL;
                        size_t req_data_size = 0;

                        if (neu_http_get_body((aio), (void **) &req_data,
                                              &req_data_size) == 0) {
                            neu_otel_scope_add_span_attr_string(
                                scope, "HTTP body", req_data);
                        }

                        free(req_data);
                    }
                }
            }
        }
    }

    ret = plugin_common->adapter_callbacks->command(plugin_common->adapter,
                                                    head, data);

    if (otel_flag && otel_control_flag && trace) {
        neu_otel_scope_set_span_end_time(scope, neu_time_ms());

        if (ret != 0) {
            neu_otel_scope_add_span_attr_int(scope, "error type",
                                             NEU_ERR_IS_BUSY);
            neu_otel_trace_set_final(trace);
        }
    }

    return ret;
}
