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
#include <stdlib.h>

#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_rw.h"

#include "handle.h"
#include "utils/http.h"
#include "utils/time.h"

#include "rw_handle.h"

#include "otel/otel_manager.h"

void handle_read(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_read_req_t, neu_json_decode_read_req, {
            int                  ret    = 0;
            neu_reqresp_head_t   header = { 0 };
            neu_req_read_group_t cmd    = { 0 };
            int                  err_type;
            header.ctx             = aio;
            header.type            = NEU_REQ_READ_GROUP;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            if (req->node && strlen(req->node) >= NEU_NODE_NAME_LEN) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            if (req->group && strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                err_type = NEU_ERR_GROUP_NAME_TOO_LONG;
                goto error;
            }

            if (req->name && strlen(req->name) >= NEU_TAG_NAME_LEN) {
                err_type = NEU_ERR_TAG_NAME_TOO_LONG;
                goto error;
            }

            cmd.driver = req->node;
            cmd.group  = req->group;
            cmd.name   = req->name;
            cmd.desc   = req->desc;
            cmd.sync   = req->sync;
            req->node  = NULL;
            req->group = NULL;
            ret        = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                neu_req_read_group_fini(&cmd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
            goto success;

        error:
            NEU_JSON_RESPONSE_ERROR(
                err_type, { neu_http_response(aio, err_type, result_error); });

        success:;
        })
}

void handle_read_paginate(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_read_paginate_req_t, neu_json_decode_read_paginate_req, {
            int                           ret    = 0;
            neu_reqresp_head_t            header = { 0 };
            neu_req_read_group_paginate_t cmd    = { 0 };
            int                           err_type;
            header.ctx             = aio;
            header.type            = NEU_REQ_READ_GROUP_PAGINATE;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            if (req->node && strlen(req->node) >= NEU_NODE_NAME_LEN) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            if (req->group && strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                err_type = NEU_ERR_GROUP_NAME_TOO_LONG;
                goto error;
            }

            if (req->name && strlen(req->name) >= NEU_TAG_NAME_LEN) {
                err_type = NEU_ERR_TAG_NAME_TOO_LONG;
                goto error;
            }

            cmd.driver       = req->node;
            cmd.group        = req->group;
            cmd.name         = req->name;
            cmd.desc         = req->desc;
            cmd.sync         = req->sync;
            cmd.current_page = req->current_page;
            cmd.page_size    = req->page_size;
            cmd.is_error     = req->is_error;
            req->node        = NULL;
            req->group       = NULL;
            ret              = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                neu_req_read_group_paginate_fini(&cmd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
            goto success;

        error:
            NEU_JSON_RESPONSE_ERROR(
                err_type, { neu_http_response(aio, err_type, result_error); });

        success:;
        })
}

void handle_test_read_tag(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_test_read_tag_req_t, neu_json_decode_test_read_tag_req, {
            int                     ret    = 0;
            neu_reqresp_head_t      header = { 0 };
            neu_req_test_read_tag_t cmd    = { 0 };
            int                     err_type;
            header.ctx             = aio;
            header.type            = NEU_REQ_TEST_READ_TAG;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            if (req->driver && strlen(req->driver) >= NEU_NODE_NAME_LEN) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }
            if (req->group && strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                err_type = NEU_ERR_GROUP_NAME_TOO_LONG;
                goto error;
            }
            if (req->tag && strlen(req->tag) >= NEU_TAG_NAME_LEN) {
                err_type = NEU_ERR_TAG_NAME_TOO_LONG;
                goto error;
            }
            if (req->address && strlen(req->address) >= NEU_TAG_ADDRESS_LEN) {
                err_type = NEU_ERR_TAG_ADDRESS_TOO_LONG;
                goto error;
            }

            strcpy(cmd.driver, req->driver);
            strcpy(cmd.group, req->group);
            strcpy(cmd.tag, req->tag);
            strcpy(cmd.address, req->address);
            cmd.attribute = req->attribute;
            cmd.type      = req->type;
            cmd.precision = req->precision;
            cmd.decimal   = req->decimal;
            cmd.bias      = req->bias;

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
            goto success;

        error:
            NEU_JSON_RESPONSE_ERROR(
                err_type, { neu_http_response(aio, err_type, result_error); });

        success:;
        })
}

void handle_write(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_write_req_t, neu_json_decode_write_req, {
            neu_reqresp_head_t  header = { 0 };
            neu_req_write_tag_t cmd    = { 0 };
            int                 err_type;

            nng_http_req *nng_req = nng_aio_get_input(aio, 0);
            nlog_notice("<%p> req %s %s", aio, nng_http_req_get_method(nng_req),
                        nng_http_req_get_uri(nng_req));

            header.ctx             = aio;
            header.type            = NEU_REQ_WRITE_TAG;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_SPEC;

            bool               trace_flag = false;
            neu_otel_trace_ctx trace      = NULL;
            neu_otel_scope_ctx scope      = NULL;

            if (neu_otel_control_is_started()) {
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
                        trace_flag = true;
                        trace      = neu_otel_create_trace(trace_id, header.ctx,
                                                      flags, trace_state);
                        scope      = neu_otel_add_span(trace);
                        neu_otel_scope_set_span_name(
                            scope, nng_http_req_get_uri(nng_req));
                        char new_span_id[36] = { 0 };
                        neu_otel_new_span_id(new_span_id);
                        neu_otel_scope_set_span_id(scope, new_span_id);
                        if (strlen(span_id) == 16) {
                            neu_otel_scope_set_parent_span_id(scope, span_id);
                        }
                        neu_otel_scope_set_span_flags(scope, flags);
                        neu_otel_scope_set_span_start_time(scope,
                                                           neu_time_ns());

                        neu_otel_scope_add_span_attr_int(
                            scope, "thread id", (int64_t) pthread_self());

                        neu_otel_scope_add_span_attr_string(
                            scope, "HTTP method",
                            nng_http_req_get_method(nng_req));

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

            if (req->t == NEU_JSON_STR &&
                strlen(req->value.val_str) >= NEU_VALUE_SIZE) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_STRING_TOO_LONG, {
                    neu_http_response(aio, NEU_ERR_STRING_TOO_LONG,
                                      result_error);
                });
                if (neu_otel_control_is_started() && trace_flag) {
                    neu_otel_scope_set_status_code2(
                        scope, NEU_OTEL_STATUS_ERROR, NEU_ERR_STRING_TOO_LONG);
                    neu_otel_scope_set_span_end_time(scope, neu_time_ns());
                    neu_otel_trace_set_final(trace);
                }
                return;
            }

            if (req->node && strlen(req->node) >= NEU_NODE_NAME_LEN) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            if (req->group && strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                err_type = NEU_ERR_GROUP_NAME_TOO_LONG;
                goto error;
            }

            if (req->tag && strlen(req->tag) >= NEU_TAG_NAME_LEN) {
                err_type = NEU_ERR_TAG_NAME_TOO_LONG;
                goto error;
            }

            cmd.driver = req->node;
            cmd.group  = req->group;
            cmd.tag    = req->tag;

            req->node  = NULL; // ownership moved
            req->group = NULL; // ownership moved
            req->tag   = NULL; // ownership moved

            switch (req->t) {
            case NEU_JSON_INT:
                cmd.value.type      = NEU_TYPE_INT64;
                cmd.value.value.i64 = req->value.val_int;
                break;
            case NEU_JSON_STR:
                cmd.value.type = NEU_TYPE_STRING;
                strcpy(cmd.value.value.str, req->value.val_str);
                break;
            case NEU_JSON_DOUBLE:
                cmd.value.type      = NEU_TYPE_DOUBLE;
                cmd.value.value.d64 = req->value.val_double;
                break;
            case NEU_JSON_BOOL:
                cmd.value.type          = NEU_TYPE_BOOL;
                cmd.value.value.boolean = req->value.val_bool;
                break;
            case NEU_JSON_ARRAY_INT64:
                cmd.value.type              = NEU_TYPE_ARRAY_INT64;
                cmd.value.value.i64s.length = req->value.val_array_int64.length;
                cmd.value.value.i64s.i64s =
                    calloc(req->value.val_array_int64.length, sizeof(int64_t));
                for (size_t i = 0; i < req->value.val_array_int64.length; i++) {
                    cmd.value.value.i64s.i64s[i] =
                        req->value.val_array_int64.i64s[i];
                }
                break;
            case NEU_JSON_ARRAY_DOUBLE:
                cmd.value.type = NEU_TYPE_ARRAY_DOUBLE;
                cmd.value.value.f64s.length =
                    req->value.val_array_double.length;
                cmd.value.value.f64s.f64s =
                    calloc(req->value.val_array_double.length, sizeof(double));
                for (size_t i = 0; i < req->value.val_array_double.length;
                     i++) {
                    cmd.value.value.f64s.f64s[i] =
                        req->value.val_array_double.f64s[i];
                }
                break;
            case NEU_JSON_ARRAY_BOOL:
                cmd.value.type               = NEU_TYPE_ARRAY_BOOL;
                cmd.value.value.bools.length = req->value.val_array_bool.length;
                cmd.value.value.bools.bools =
                    calloc(req->value.val_array_bool.length, sizeof(bool));
                for (size_t i = 0; i < req->value.val_array_bool.length; i++) {
                    cmd.value.value.bools.bools[i] =
                        req->value.val_array_bool.bools[i];
                }
                break;
            default:
                assert(false);
                break;
            }

            int ret = 0;
            if (neu_otel_control_is_started() && trace_flag) {
                ret = neu_plugin_op(plugin, header, &cmd);

                neu_otel_scope_set_span_end_time(scope, neu_time_ns());
            } else {
                ret = neu_plugin_op(plugin, header, &cmd);
            }

            if (ret != 0) {
                neu_req_write_tag_fini(&cmd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
                if (neu_otel_control_is_started() && trace_flag) {
                    neu_otel_scope_set_status_code2(
                        scope, NEU_OTEL_STATUS_ERROR, NEU_ERR_IS_BUSY);
                    neu_otel_trace_set_final(trace);
                }
            }
            goto success;

        error:
            NEU_JSON_RESPONSE_ERROR(
                err_type, { neu_http_response(aio, err_type, result_error); });
            if (neu_otel_control_is_started() && trace_flag) {
                neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_ERROR,
                                                err_type);
                neu_otel_trace_set_final(trace);
            }

        success:;
        })
}

void handle_write_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_write_tags_req_t, neu_json_decode_write_tags_req, {
            neu_reqresp_head_t   header = { 0 };
            neu_req_write_tags_t cmd    = { 0 };
            int                  err_type;

            nng_http_req *nng_req = nng_aio_get_input(aio, 0);
            nlog_notice("<%p> req %s %s", aio, nng_http_req_get_method(nng_req),
                        nng_http_req_get_uri(nng_req));
            header.ctx             = aio;
            header.type            = NEU_REQ_WRITE_TAGS;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_SPEC;

            bool               trace_flag = false;
            neu_otel_trace_ctx trace      = NULL;
            neu_otel_scope_ctx scope      = NULL;

            if (neu_otel_control_is_started()) {
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
                        trace_flag = true;
                        trace      = neu_otel_create_trace(trace_id, header.ctx,
                                                      flags, trace_state);
                        scope      = neu_otel_add_span(trace);
                        neu_otel_scope_set_span_name(
                            scope, nng_http_req_get_uri(nng_req));
                        char new_span_id[36] = { 0 };
                        neu_otel_new_span_id(new_span_id);
                        neu_otel_scope_set_span_id(scope, new_span_id);
                        if (strlen(span_id) == 16) {
                            neu_otel_scope_set_parent_span_id(scope, span_id);
                        }
                        neu_otel_scope_set_span_flags(scope, flags);
                        neu_otel_scope_set_span_start_time(scope,
                                                           neu_time_ns());

                        neu_otel_scope_add_span_attr_int(
                            scope, "thread id", (int64_t) pthread_self());

                        neu_otel_scope_add_span_attr_string(
                            scope, "HTTP method",
                            nng_http_req_get_method(nng_req));

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

            for (int i = 0; i < req->n_tag; i++) {
                if (req->tags[i].t == NEU_JSON_STR) {
                    if (strlen(req->tags[i].value.val_str) >= NEU_VALUE_SIZE) {
                        NEU_JSON_RESPONSE_ERROR(NEU_ERR_STRING_TOO_LONG, {
                            neu_http_response(aio, NEU_ERR_STRING_TOO_LONG,
                                              result_error);
                        });
                        if (neu_otel_control_is_started() && trace_flag) {
                            neu_otel_scope_add_span_attr_int(
                                scope, "error type", NEU_ERR_STRING_TOO_LONG);
                            neu_otel_scope_set_span_end_time(scope,
                                                             neu_time_ns());
                            neu_otel_trace_set_final(trace);
                        }
                        return;
                    }
                }
            }

            if (strlen(req->node) >= NEU_NODE_NAME_LEN) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            if (strlen(req->node) >= NEU_GROUP_NAME_LEN) {
                err_type = NEU_ERR_GROUP_NAME_TOO_LONG;
                goto error;
            }

            cmd.driver = req->node;
            cmd.group  = req->group;
            cmd.n_tag  = req->n_tag;
            cmd.tags   = calloc(cmd.n_tag, sizeof(neu_resp_tag_value_t));
            req->node  = NULL; // ownership moved
            req->group = NULL; // ownership moved

            for (int i = 0; i < cmd.n_tag; i++) {
                strcpy(cmd.tags[i].tag, req->tags[i].tag);
                switch (req->tags[i].t) {
                case NEU_JSON_INT:
                    cmd.tags[i].value.type      = NEU_TYPE_INT64;
                    cmd.tags[i].value.value.i64 = req->tags[i].value.val_int;
                    break;
                case NEU_JSON_STR:
                    cmd.tags[i].value.type = NEU_TYPE_STRING;
                    strcpy(cmd.tags[i].value.value.str,
                           req->tags[i].value.val_str);
                    break;
                case NEU_JSON_DOUBLE:
                    cmd.tags[i].value.type      = NEU_TYPE_DOUBLE;
                    cmd.tags[i].value.value.d64 = req->tags[i].value.val_double;
                    break;
                case NEU_JSON_BOOL:
                    cmd.tags[i].value.type = NEU_TYPE_BOOL;
                    cmd.tags[i].value.value.boolean =
                        req->tags[i].value.val_bool;
                    break;
                case NEU_JSON_ARRAY_INT64:
                    cmd.tags[i].value.type = NEU_TYPE_ARRAY_INT64;
                    cmd.tags[i].value.value.i64s.length =
                        req->tags[i].value.val_array_int64.length;
                    cmd.tags[i].value.value.i64s.i64s =
                        calloc(req->tags[i].value.val_array_int64.length,
                               sizeof(int64_t));
                    memcpy(cmd.tags[i].value.value.i64s.i64s,
                           req->tags[i].value.val_array_int64.i64s,
                           sizeof(int64_t) *
                               req->tags[i].value.val_array_int64.length);
                    break;
                case NEU_JSON_ARRAY_DOUBLE:
                    cmd.tags[i].value.type = NEU_TYPE_ARRAY_DOUBLE;
                    cmd.tags[i].value.value.f64s.length =
                        req->tags[i].value.val_array_double.length;
                    cmd.tags[i].value.value.f64s.f64s =
                        calloc(req->tags[i].value.val_array_double.length,
                               sizeof(double));
                    memcpy(cmd.tags[i].value.value.f64s.f64s,
                           req->tags[i].value.val_array_double.f64s,
                           sizeof(double) *
                               req->tags[i].value.val_array_double.length);
                    break;
                case NEU_JSON_ARRAY_BOOL:
                    cmd.tags[i].value.type = NEU_TYPE_ARRAY_BOOL;
                    cmd.tags[i].value.value.bools.length =
                        req->tags[i].value.val_array_bool.length;
                    cmd.tags[i].value.value.bools.bools = calloc(
                        req->tags[i].value.val_array_bool.length, sizeof(bool));
                    memcpy(cmd.tags[i].value.value.bools.bools,
                           req->tags[i].value.val_array_bool.bools,
                           sizeof(bool) *
                               req->tags[i].value.val_array_bool.length);
                    break;
                default:
                    assert(false);
                    break;
                }
            }

            int ret = 0;

            if (neu_otel_control_is_started() && trace_flag) {
                ret = neu_plugin_op(plugin, header, &cmd);
                neu_otel_scope_set_span_end_time(scope, neu_time_ns());
            } else {
                ret = neu_plugin_op(plugin, header, &cmd);
            }

            if (ret != 0) {
                neu_req_write_tags_fini(&cmd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
                if (neu_otel_control_is_started() && trace_flag) {
                    neu_otel_scope_set_status_code2(
                        scope, NEU_OTEL_STATUS_ERROR, NEU_ERR_IS_BUSY);
                    neu_otel_trace_set_final(trace);
                }
            }
            goto success;

        error:
            NEU_JSON_RESPONSE_ERROR(
                err_type, { neu_http_response(aio, err_type, result_error); });
            if (neu_otel_control_is_started() && trace_flag) {
                neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_ERROR,
                                                err_type);
                neu_otel_trace_set_final(trace);
            }

        success:;
        })
}

static void trans(neu_json_write_gtags_req_t *req, neu_req_write_gtags_t *cmd)
{
    cmd->driver  = req->node;
    req->node    = NULL; // ownership moved
    cmd->n_group = req->n_group;
    cmd->groups  = calloc(cmd->n_group, sizeof(neu_req_gtag_group_t));
    for (int i = 0; i < cmd->n_group; i++) {
        cmd->groups[i].group = req->groups[i].group;
        req->groups[i].group = NULL; // ownership moved
        cmd->groups[i].n_tag = req->groups[i].n_tag;
        cmd->groups[i].tags =
            calloc(cmd->groups[i].n_tag, sizeof(neu_resp_tag_value_t));

        for (int k = 0; k < cmd->groups[i].n_tag; k++) {
            strcpy(cmd->groups[i].tags[k].tag, req->groups[i].tags[k].tag);

            switch (req->groups[i].tags[k].t) {
            case NEU_JSON_INT:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_INT64;
                cmd->groups[i].tags[k].value.value.u64 =
                    req->groups[i].tags[k].value.val_int;
                break;
            case NEU_JSON_STR:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_STRING;
                strcpy(cmd->groups[i].tags[k].value.value.str,
                       req->groups[i].tags[k].value.val_str);
                break;
            case NEU_JSON_DOUBLE:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_DOUBLE;
                cmd->groups[i].tags[k].value.value.d64 =
                    req->groups[i].tags[k].value.val_double;
                break;
            case NEU_JSON_BOOL:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_BOOL;
                cmd->groups[i].tags[k].value.value.boolean =
                    req->groups[i].tags[k].value.val_bool;
                break;
            case NEU_JSON_ARRAY_INT64:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_ARRAY_INT64;
                cmd->groups[i].tags[k].value.value.i64s.length =
                    req->groups[i].tags[k].value.val_array_int64.length;

                cmd->groups[i].tags[k].value.value.i64s.i64s =
                    calloc(sizeof(int64_t),
                           req->groups[i].tags[k].value.val_array_int64.length);

                memcpy(cmd->groups[i].tags[k].value.value.i64s.i64s,
                       req->groups[i].tags[k].value.val_array_int64.i64s,
                       sizeof(int64_t) *
                           req->groups[i].tags[k].value.val_array_int64.length);

                break;
            case NEU_JSON_ARRAY_DOUBLE:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_ARRAY_DOUBLE;
                cmd->groups[i].tags[k].value.value.f64s.length =
                    req->groups[i].tags[k].value.val_array_double.length;

                cmd->groups[i].tags[k].value.value.f64s.f64s = calloc(
                    sizeof(double),
                    req->groups[i].tags[k].value.val_array_double.length);
                memcpy(
                    cmd->groups[i].tags[k].value.value.f64s.f64s,
                    req->groups[i].tags[k].value.val_array_double.f64s,
                    sizeof(double) *
                        req->groups[i].tags[k].value.val_array_double.length);

                break;
            case NEU_JSON_ARRAY_BOOL:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_ARRAY_BOOL;
                cmd->groups[i].tags[k].value.value.bools.length =
                    req->groups[i].tags[k].value.val_array_bool.length;

                cmd->groups[i].tags[k].value.value.bools.bools =
                    calloc(sizeof(bool),
                           req->groups[i].tags[k].value.val_array_bool.length);
                memcpy(cmd->groups[i].tags[k].value.value.bools.bools,
                       req->groups[i].tags[k].value.val_array_bool.bools,
                       sizeof(bool) *
                           req->groups[i].tags[k].value.val_array_bool.length);

                break;
            default:
                assert(false);
                break;
            }
        }
    }
}

void handle_write_gtags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_write_gtags_req_t, neu_json_decode_write_gtags_req, {
            neu_reqresp_head_t    header = { 0 };
            neu_req_write_gtags_t cmd    = { 0 };

            nng_http_req *nng_req = nng_aio_get_input(aio, 0);
            nlog_notice("<%p> req %s %s", aio, nng_http_req_get_method(nng_req),
                        nng_http_req_get_uri(nng_req));
            header.ctx             = aio;
            header.type            = NEU_REQ_WRITE_GTAGS;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_SPEC;

            bool               trace_flag = false;
            neu_otel_trace_ctx trace      = NULL;
            neu_otel_scope_ctx scope      = NULL;

            if (neu_otel_control_is_started()) {
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
                        trace_flag = true;
                        trace      = neu_otel_create_trace(trace_id, header.ctx,
                                                      flags, trace_state);
                        scope      = neu_otel_add_span(trace);
                        neu_otel_scope_set_span_name(
                            scope, nng_http_req_get_uri(nng_req));
                        char new_span_id[36] = { 0 };
                        neu_otel_new_span_id(new_span_id);
                        neu_otel_scope_set_span_id(scope, new_span_id);
                        if (strlen(span_id) == 16) {
                            neu_otel_scope_set_parent_span_id(scope, span_id);
                        }
                        neu_otel_scope_set_span_flags(scope, flags);
                        neu_otel_scope_set_span_start_time(scope,
                                                           neu_time_ns());

                        neu_otel_scope_add_span_attr_int(
                            scope, "thread id", (int64_t) pthread_self());

                        neu_otel_scope_add_span_attr_string(
                            scope, "HTTP method",
                            nng_http_req_get_method(nng_req));

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

            trans(req, &cmd);

            int ret = 0;

            if (neu_otel_control_is_started() && trace_flag) {
                ret = neu_plugin_op(plugin, header, &cmd);
                neu_otel_scope_set_span_end_time(scope, neu_time_ns());
            } else {
                ret = neu_plugin_op(plugin, header, &cmd);
            }

            if (ret != 0) {
                neu_req_write_gtags_fini(&cmd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
                if (neu_otel_control_is_started() && trace_flag) {
                    neu_otel_scope_set_status_code2(
                        scope, NEU_OTEL_STATUS_ERROR, NEU_ERR_IS_BUSY);
                    neu_otel_trace_set_final(trace);
                }
            }
        })
}

void handle_read_resp(nng_aio *aio, neu_resp_read_group_t *resp)
{
    neu_json_read_resp_t api_res = { 0 };
    char *               result  = NULL;
    int                  index   = 0;

    api_res.n_tag = utarray_len(resp->tags);
    api_res.tags  = calloc(api_res.n_tag, sizeof(neu_json_read_resp_tag_t));

    utarray_foreach(resp->tags, neu_resp_tag_value_meta_t *, tag_value)
    {
        neu_tag_value_to_json(tag_value, &api_res.tags[index]);
        index += 1;
    }

    neu_json_encode_by_fn(&api_res, neu_json_encode_read_resp, &result);
    for (int i = 0; i < api_res.n_tag; i++) {
        if (api_res.tags[i].n_meta > 0) {
            free(api_res.tags[i].metas);
        }
    }
    neu_http_ok(aio, result);
    free(api_res.tags);
    free(result);
}

void handle_read_paginate_resp(nng_aio *                       aio,
                               neu_resp_read_group_paginate_t *resp)
{
    neu_json_read_paginate_resp_t api_res    = { 0 };
    char *                        result     = NULL;
    unsigned int                  total_tags = resp->total_count;

    if (resp->is_error) {
        UT_array *filtered_tags;
        utarray_new(filtered_tags, neu_resp_tag_value_meta_paginate_icd());

        utarray_foreach(resp->tags, neu_resp_tag_value_meta_paginate_t *,
                        tag_value)
        {
            if (tag_value->value.type == NEU_TYPE_ERROR) {
                neu_resp_tag_value_meta_paginate_t new_tag_value;
                memcpy(&new_tag_value, tag_value,
                       sizeof(neu_resp_tag_value_meta_paginate_t));

                new_tag_value.datatag.name = strdup(tag_value->datatag.name);
                new_tag_value.datatag.address =
                    strdup(tag_value->datatag.address);
                new_tag_value.datatag.description =
                    strdup(tag_value->datatag.description);

                utarray_push_back(filtered_tags, &new_tag_value);
            }
        }

        for (unsigned int i = 0; i < utarray_len(resp->tags); ++i) {
            neu_resp_tag_value_meta_paginate_t *orig_tag_value;
            orig_tag_value =
                (neu_resp_tag_value_meta_paginate_t *) utarray_eltptr(
                    resp->tags, i);
            if (orig_tag_value) {
                free(orig_tag_value->datatag.name);
                free(orig_tag_value->datatag.address);
                free(orig_tag_value->datatag.description);
            }
        }

        utarray_free(resp->tags);
        resp->tags = filtered_tags;
        total_tags = utarray_len(resp->tags);

        if (resp->current_page > 0 && resp->page_size > 0) {
            unsigned int start_index =
                (resp->current_page - 1) * resp->page_size;
            unsigned int end_index = start_index + resp->page_size;

            if (start_index >= total_tags) {
                api_res.n_tag = 0;
                api_res.tags =
                    calloc(1, sizeof(neu_json_read_paginate_resp_tag_t));
            } else {
                if (end_index > total_tags) {
                    end_index = total_tags;
                }

                api_res.n_tag = end_index - start_index;
                api_res.tags  = calloc(
                    api_res.n_tag, sizeof(neu_json_read_paginate_resp_tag_t));

                for (unsigned int i = start_index; i < end_index; i++) {
                    neu_resp_tag_value_meta_paginate_t *tag_value =
                        (neu_resp_tag_value_meta_paginate_t *) utarray_eltptr(
                            resp->tags, i);
                    neu_tag_value_to_json_paginate(
                        tag_value, &api_res.tags[i - start_index]);
                }
            }
        } else {
            api_res.n_tag = total_tags;
            api_res.tags  = calloc(api_res.n_tag,
                                  sizeof(neu_json_read_paginate_resp_tag_t));
            int index     = 0;

            utarray_foreach(resp->tags, neu_resp_tag_value_meta_paginate_t *,
                            tag_value)
            {
                neu_tag_value_to_json_paginate(tag_value,
                                               &api_res.tags[index++]);
            }
        }
    } else {

        if (resp->current_page > 0 && resp->page_size > 0) {
            api_res.n_tag = utarray_len(resp->tags);
            api_res.tags  = calloc(api_res.n_tag,
                                  sizeof(neu_json_read_paginate_resp_tag_t));
            int index     = 0;
            utarray_foreach(resp->tags, neu_resp_tag_value_meta_paginate_t *,
                            tag_value)
            {
                neu_tag_value_to_json_paginate(tag_value,
                                               &api_res.tags[index++]);
            }
        } else {
            api_res.n_tag = total_tags;
            api_res.tags  = calloc(api_res.n_tag,
                                  sizeof(neu_json_read_paginate_resp_tag_t));
            int index     = 0;

            utarray_foreach(resp->tags, neu_resp_tag_value_meta_paginate_t *,
                            tag_value)
            {
                neu_tag_value_to_json_paginate(tag_value,
                                               &api_res.tags[index++]);
            }
        }
    }

    api_res.meta.current_page = resp->current_page;
    api_res.meta.page_size    = resp->page_size;
    api_res.meta.total        = total_tags;

    neu_json_encode_by_fn(&api_res, neu_json_encode_read_paginate_resp,
                          &result);

    for (unsigned int i = 0; i < utarray_len(resp->tags); ++i) {
        neu_resp_tag_value_meta_paginate_t *final_tag_value;
        final_tag_value = (neu_resp_tag_value_meta_paginate_t *) utarray_eltptr(
            resp->tags, i);
        if (final_tag_value) {
            free(final_tag_value->datatag.name);
            free(final_tag_value->datatag.address);
            free(final_tag_value->datatag.description);
        }
    }

    for (int i = 0; i < api_res.n_tag; i++) {
        if (api_res.tags[i].n_meta > 0) {
            free(api_res.tags[i].metas);
        }
    }
    neu_http_ok(aio, result);
    free(api_res.tags);
    free(result);
}

void handle_test_read_tag_resp(nng_aio *aio, neu_resp_test_read_tag_t *resp)
{
    char *result = NULL;
    neu_json_encode_by_fn(resp, neu_json_encode_test_read_tag_resp, &result);
    neu_http_ok(aio, result);
    free(result);
}