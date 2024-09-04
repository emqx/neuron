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
#include <string.h>
#include <time.h>

#include "define.h"
#include "event/event.h"
#include "metrics.h"
#include "plugin.h"
#include "utils/http.h"
#include "utils/utarray.h"
#include "utils/uthash.h"

#include "otel_manager.h"
#include "trace.pb-c.h"

#define SPAN_ID_LENGTH 16
#define SPAN_ID_CHARSET "0123456789abcdef"

bool   otel_flag               = false;
char   otel_collector_url[128] = { 0 };
bool   otel_control_flag       = false;
bool   otel_data_flag          = false;
double otel_data_sample_rate   = 0.0;

typedef struct {
    Opentelemetry__Proto__Trace__V1__TracesData trace_data;
    uint8_t                                     trace_id[64];
    uint32_t                                    flags;
    bool                                        final;
    size_t                                      span_num;
    UT_array *                                  scopes;
    pthread_mutex_t                             mutex;
} trace_ctx_t;

typedef struct {
    int                                    span_index;
    Opentelemetry__Proto__Trace__V1__Span *span;
    trace_ctx_t *                          trace_ctx;
} trace_scope_t;

typedef struct {
    void *         key;
    trace_ctx_t *  ctx;
    UT_hash_handle hh;
} trace_ctx_table_ele_t;

typedef struct {
    char key[128];
    char value[256];
} trace_kv_t;

trace_ctx_table_ele_t *traces_table = NULL;

pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

neu_events_t *     otel_event = NULL;
neu_event_timer_t *otel_timer = NULL;

static int hex_char_to_int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int hex_string_to_binary(const char *   hex_string,
                                unsigned char *binary_array, int max_length)
{
    int length = strlen(hex_string);
    if (length % 2 != 0 || length <= 0)
        return -1;

    int binary_length = length / 2;
    if (binary_length > max_length)
        return -1;

    for (int i = 0; i < binary_length; i++) {
        int high_nibble = hex_char_to_int(hex_string[2 * i]);
        int low_nibble  = hex_char_to_int(hex_string[2 * i + 1]);
        if (high_nibble == -1 || low_nibble == -1)
            return -1;

        binary_array[i] = (high_nibble << 4) | low_nibble;
    }

    return binary_length;
}

static int parse_tracestate(const char *tracestate, trace_kv_t *kvs,
                            int kvs_len, int *count)
{
    if (!tracestate) {
        return -1;
    }
    const char *delim = ",";
    char *      token;
    char *      saveptr;
    char *      tracestate_copy = strdup(tracestate);
    if (!tracestate_copy) {
        return -1;
    }

    *count = 0;
    token  = strtok_r(tracestate_copy, delim, &saveptr);

    while (token != NULL && *count < kvs_len) {
        char *equal_sign = strchr(token, '=');
        if (equal_sign == NULL) {
            free(tracestate_copy);
            return -1;
        }

        *equal_sign = '\0';
        strncpy(kvs[*count].key, token, 127);
        kvs[*count].key[127] = '\0';

        strncpy(kvs[*count].value, equal_sign + 1, 255);
        kvs[*count].value[255] = '\0';

        (*count)++;
        token = strtok_r(NULL, delim, &saveptr);
    }

    free(tracestate_copy);
    return 1;
}

void neu_otel_free_span(Opentelemetry__Proto__Trace__V1__Span *span)
{
    for (size_t i = 0; i < span->n_attributes; i++) {
        if (span->attributes[i]->value->value_case ==
            OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE) {
            free(span->attributes[i]->value->string_value);
        }
        free(span->attributes[i]->key);
        free(span->attributes[i]->value);
        free(span->attributes[i]);
    }

    free(span->attributes);
    free(span->name);
    free(span->trace_id.data);
    free(span->parent_span_id.data);
    free(span->span_id.data);
    free(span);
}

neu_otel_trace_ctx neu_otel_create_trace(const char *trace_id, void *req_ctx,
                                         uint32_t flags, const char *tracestate)
{
    trace_ctx_t *ctx = calloc(1, sizeof(trace_ctx_t));
    opentelemetry__proto__trace__v1__traces_data__init(&ctx->trace_data);
    strncpy((char *) ctx->trace_id, trace_id, 64);

    ctx->trace_data.n_resource_spans = 1;
    ctx->trace_data.resource_spans =
        calloc(1, sizeof(Opentelemetry__Proto__Trace__V1__ResourceSpans *));
    ctx->trace_data.resource_spans[0] =
        calloc(1, sizeof(Opentelemetry__Proto__Trace__V1__ResourceSpans));
    opentelemetry__proto__trace__v1__resource_spans__init(
        ctx->trace_data.resource_spans[0]);

    ctx->trace_data.resource_spans[0]->resource =
        calloc(1, sizeof(Opentelemetry__Proto__Resource__V1__Resource));
    opentelemetry__proto__resource__v1__resource__init(
        ctx->trace_data.resource_spans[0]->resource);

    trace_kv_t tracestate_kvs[64] = { 0 };
    int        count              = 0;
    if (parse_tracestate(tracestate, tracestate_kvs, 64, &count)) {
        ctx->trace_data.resource_spans[0]->resource->n_attributes = 7 + count;
        ctx->trace_data.resource_spans[0]->resource->attributes   = calloc(
            7 + count, sizeof(Opentelemetry__Proto__Common__V1__KeyValue *));
        for (int i = 7; i < count + 7; i++) {
            ctx->trace_data.resource_spans[0]->resource->attributes[i] =
                calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));

            opentelemetry__proto__common__v1__key_value__init(
                ctx->trace_data.resource_spans[0]->resource->attributes[i]);

            ctx->trace_data.resource_spans[0]->resource->attributes[i]->key =
                strdup(tracestate_kvs[i - 7].key);

            ctx->trace_data.resource_spans[0]->resource->attributes[i]->value =
                calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
            opentelemetry__proto__common__v1__any_value__init(
                ctx->trace_data.resource_spans[0]
                    ->resource->attributes[i]
                    ->value);
            ctx->trace_data.resource_spans[0]
                ->resource->attributes[i]
                ->value->value_case =
                OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
            ctx->trace_data.resource_spans[0]
                ->resource->attributes[i]
                ->value->string_value = strdup(tracestate_kvs[i - 7].value);
        }
    } else {
        ctx->trace_data.resource_spans[0]->resource->n_attributes = 7;
        ctx->trace_data.resource_spans[0]->resource->attributes =
            calloc(7, sizeof(Opentelemetry__Proto__Common__V1__KeyValue *));
    }

    // 0
    ctx->trace_data.resource_spans[0]->resource->attributes[0] =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));

    opentelemetry__proto__common__v1__key_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[0]);

    ctx->trace_data.resource_spans[0]->resource->attributes[0]->key =
        strdup("app.name");

    ctx->trace_data.resource_spans[0]->resource->attributes[0]->value =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[0]->value);
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[0]
        ->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[0]
        ->value->string_value = strdup("neuron");

    // 1
    ctx->trace_data.resource_spans[0]->resource->attributes[1] =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));

    opentelemetry__proto__common__v1__key_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[1]);

    ctx->trace_data.resource_spans[0]->resource->attributes[1]->key =
        strdup("app.version");

    ctx->trace_data.resource_spans[0]->resource->attributes[1]->value =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[1]->value);
    char *version = calloc(1, 24);
    sprintf(version, "%d.%d.%d", NEU_VERSION_MAJOR, NEU_VERSION_MINOR,
            NEU_VERSION_FIX);
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[1]
        ->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[1]
        ->value->string_value = version;

    // 2
    ctx->trace_data.resource_spans[0]->resource->attributes[2] =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));

    opentelemetry__proto__common__v1__key_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[2]);

    ctx->trace_data.resource_spans[0]->resource->attributes[2]->key =
        strdup("distro");

    ctx->trace_data.resource_spans[0]->resource->attributes[2]->value =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[2]->value);
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[2]
        ->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[2]
        ->value->string_value = strdup(neu_get_global_metrics()->distro);

    // 3
    ctx->trace_data.resource_spans[0]->resource->attributes[3] =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));

    opentelemetry__proto__common__v1__key_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[3]);

    ctx->trace_data.resource_spans[0]->resource->attributes[3]->key =
        strdup("kernel");

    ctx->trace_data.resource_spans[0]->resource->attributes[3]->value =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[3]->value);
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[3]
        ->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[3]
        ->value->string_value = strdup(neu_get_global_metrics()->kernel);

    // 4
    ctx->trace_data.resource_spans[0]->resource->attributes[4] =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));

    opentelemetry__proto__common__v1__key_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[4]);

    ctx->trace_data.resource_spans[0]->resource->attributes[4]->key =
        strdup("machine");

    ctx->trace_data.resource_spans[0]->resource->attributes[4]->value =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[4]->value);
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[4]
        ->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[4]
        ->value->string_value = strdup(neu_get_global_metrics()->machine);

    // 5
    ctx->trace_data.resource_spans[0]->resource->attributes[5] =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));

    opentelemetry__proto__common__v1__key_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[5]);

    ctx->trace_data.resource_spans[0]->resource->attributes[5]->key =
        strdup("clib");

    ctx->trace_data.resource_spans[0]->resource->attributes[5]->value =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[5]->value);
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[5]
        ->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[5]
        ->value->string_value = strdup(neu_get_global_metrics()->clib);

    // 6
    ctx->trace_data.resource_spans[0]->resource->attributes[6] =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));

    opentelemetry__proto__common__v1__key_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[6]);

    ctx->trace_data.resource_spans[0]->resource->attributes[6]->key =
        strdup("clib_version");

    ctx->trace_data.resource_spans[0]->resource->attributes[6]->value =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(
        ctx->trace_data.resource_spans[0]->resource->attributes[6]->value);
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[6]
        ->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
    ctx->trace_data.resource_spans[0]
        ->resource->attributes[6]
        ->value->string_value = strdup(neu_get_global_metrics()->clib_version);

    //
    ctx->trace_data.resource_spans[0]->n_scope_spans = 1;
    ctx->trace_data.resource_spans[0]->scope_spans =
        calloc(1, sizeof(Opentelemetry__Proto__Trace__V1__ScopeSpans *));
    ctx->trace_data.resource_spans[0]->scope_spans[0] =
        calloc(1, sizeof(Opentelemetry__Proto__Trace__V1__ScopeSpans));
    opentelemetry__proto__trace__v1__scope_spans__init(
        ctx->trace_data.resource_spans[0]->scope_spans[0]);

    ctx->flags = flags;

    trace_ctx_table_ele_t *ele = calloc(1, sizeof(trace_ctx_table_ele_t));
    ele->key                   = req_ctx;
    ele->ctx                   = ctx;

    utarray_new(ctx->scopes, &ut_ptr_icd);

    pthread_mutex_lock(&table_mutex);

    HASH_ADD(hh, traces_table, key, sizeof(req_ctx), ele);

    pthread_mutex_unlock(&table_mutex);

    return (neu_otel_trace_ctx) ctx;
}

neu_otel_trace_ctx neu_otel_find_trace(void *req_ctx)
{
    trace_ctx_table_ele_t *find = NULL;

    pthread_mutex_lock(&table_mutex);

    HASH_FIND(hh, traces_table, &req_ctx, sizeof(req_ctx), find);

    pthread_mutex_unlock(&table_mutex);

    if (find) {
        return (void *) find->ctx;
    } else {
        return NULL;
    }
}

void neu_otel_trace_set_final(neu_otel_trace_ctx ctx)
{
    trace_ctx_t *trace_ctx = (trace_ctx_t *) ctx;
    trace_ctx->final       = true;
}

neu_otel_trace_ctx neu_otel_find_trace_by_id(const char *trace_id)
{
    trace_ctx_table_ele_t *el = NULL, *tmp = NULL;

    pthread_mutex_lock(&table_mutex);

    HASH_ITER(hh, traces_table, el, tmp)
    {
        if (strcmp((char *) el->ctx->trace_id, trace_id) == 0) {
            pthread_mutex_unlock(&table_mutex);
            return (void *) el->ctx;
        }
    }

    pthread_mutex_unlock(&table_mutex);

    return NULL;
}

void neu_otel_free_trace(neu_otel_trace_ctx ctx)
{
    trace_ctx_t *trace_ctx = (trace_ctx_t *) ctx;
    for (size_t i = 0;
         i < trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->n_spans;
         i++) {
        neu_otel_free_span(
            trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->spans[i]);
    }
    free(trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->spans);
    free(trace_ctx->trace_data.resource_spans[0]->scope_spans[0]);
    free(trace_ctx->trace_data.resource_spans[0]->scope_spans);
    for (size_t i = 0;
         i < trace_ctx->trace_data.resource_spans[0]->resource->n_attributes;
         i++) {
        if (trace_ctx->trace_data.resource_spans[0]
                ->resource->attributes[i]
                ->value->value_case ==
            OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE) {
            free(trace_ctx->trace_data.resource_spans[0]
                     ->resource->attributes[i]
                     ->value->string_value);
        }
        free(trace_ctx->trace_data.resource_spans[0]
                 ->resource->attributes[i]
                 ->value);
        free(trace_ctx->trace_data.resource_spans[0]
                 ->resource->attributes[i]
                 ->key);
        free(trace_ctx->trace_data.resource_spans[0]->resource->attributes[i]);
    }
    free(trace_ctx->trace_data.resource_spans[0]->resource->attributes);
    free(trace_ctx->trace_data.resource_spans[0]->resource);
    free(trace_ctx->trace_data.resource_spans[0]);
    free(trace_ctx->trace_data.resource_spans);
    utarray_foreach(trace_ctx->scopes, void **, e) { free(*e); }
    utarray_free(trace_ctx->scopes);
    free(ctx);
}

neu_otel_scope_ctx neu_otel_add_span(neu_otel_trace_ctx ctx)
{
    trace_scope_t *scope     = calloc(1, sizeof(trace_scope_t));
    trace_ctx_t *  trace_ctx = (trace_ctx_t *) ctx;
    pthread_mutex_lock(&trace_ctx->mutex);
    scope->trace_ctx = ctx;
    if (trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->n_spans == 0) {
        trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->spans =
            calloc(1, sizeof(Opentelemetry__Proto__Trace__V1__Span *));
        trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->spans[0] =
            calloc(1, sizeof(Opentelemetry__Proto__Trace__V1__Span));

        trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->n_spans = 1;
        scope->span =
            trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->spans[0];
        scope->span_index = 0;
    } else {
        Opentelemetry__Proto__Trace__V1__Span **t_spans =
            trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->spans;
        trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->spans =
            calloc(1 +
                       trace_ctx->trace_data.resource_spans[0]
                           ->scope_spans[0]
                           ->n_spans,
                   sizeof(Opentelemetry__Proto__Trace__V1__Span *));
        for (size_t i = 0; i <
             trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->n_spans;
             i++) {
            trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->spans[i] =
                t_spans[i];
        }

        free(t_spans);

        trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->spans
            [trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->n_spans] =
            calloc(1, sizeof(Opentelemetry__Proto__Trace__V1__Span));
        scope->span = trace_ctx->trace_data.resource_spans[0]
                          ->scope_spans[0]
                          ->spans[trace_ctx->trace_data.resource_spans[0]
                                      ->scope_spans[0]
                                      ->n_spans];
        trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->n_spans += 1;

        scope->span_index =
            trace_ctx->trace_data.resource_spans[0]->scope_spans[0]->n_spans -
            1;
    }

    opentelemetry__proto__trace__v1__span__init(scope->span);
    scope->span->kind =
        OPENTELEMETRY__PROTO__TRACE__V1__SPAN__SPAN_KIND__SPAN_KIND_SERVER;
    uint8_t *t_id = calloc(1, 16);
    hex_string_to_binary((char *) trace_ctx->trace_id, t_id, 16);
    scope->span->trace_id.data = t_id;
    scope->span->trace_id.len  = 16;
    scope->span->flags         = trace_ctx->flags;
    utarray_push_back(trace_ctx->scopes, &scope);
    pthread_mutex_unlock(&trace_ctx->mutex);
    return scope;
}

void neu_otel_scope_set_parent_span_id(neu_otel_scope_ctx ctx,
                                       const char *       parent_span_id)
{
    trace_scope_t *scope   = (trace_scope_t *) ctx;
    uint8_t *      p_sp_id = calloc(1, 8);
    if (hex_string_to_binary(parent_span_id, p_sp_id, 8) > 0) {
        scope->span->parent_span_id.len  = 8;
        scope->span->parent_span_id.data = p_sp_id;
    } else {
        scope->span->parent_span_id.len  = 0;
        scope->span->parent_span_id.data = NULL;
        free(p_sp_id);
    }
}

void neu_otel_scope_set_parent_span_id2(neu_otel_scope_ctx ctx,
                                        uint8_t *parent_span_id, int len)
{
    trace_scope_t *scope = (trace_scope_t *) ctx;

    uint8_t *p_sp_id = calloc(1, 8);
    memcpy(p_sp_id, parent_span_id, len);
    scope->span->parent_span_id.len  = len;
    scope->span->parent_span_id.data = p_sp_id;
}

void neu_otel_scope_set_span_name(neu_otel_scope_ctx ctx, const char *span_name)
{
    trace_scope_t *scope = (trace_scope_t *) ctx;
    scope->span->name    = strdup(span_name);
}

void neu_otel_scope_set_span_id(neu_otel_scope_ctx ctx, const char *span_id)
{
    trace_scope_t *scope = (trace_scope_t *) ctx;
    uint8_t *      sp_id = calloc(1, 8);
    hex_string_to_binary(span_id, sp_id, 8);
    scope->span->span_id.data = sp_id;
    scope->span->span_id.len  = 8;
}

void neu_otel_scope_set_span_flags(neu_otel_scope_ctx ctx, uint32_t flags)
{
    trace_scope_t *scope = (trace_scope_t *) ctx;
    scope->span->flags   = flags;
}

void neu_otel_scope_add_span_attr_int(neu_otel_scope_ctx ctx, const char *key,
                                      int64_t val)
{
    trace_scope_t *scope = (trace_scope_t *) ctx;

    Opentelemetry__Proto__Common__V1__KeyValue **t_kvs =
        scope->span->attributes;

    scope->span->attributes =
        calloc(scope->span->n_attributes + 1,
               sizeof(Opentelemetry__Proto__Common__V1__KeyValue *));

    for (size_t i = 0; i < scope->span->n_attributes; i++) {
        scope->span->attributes[i] = t_kvs[i];
    }

    Opentelemetry__Proto__Common__V1__KeyValue *kv =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));
    opentelemetry__proto__common__v1__key_value__init(kv);
    kv->key   = strdup(key);
    kv->value = calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(kv->value);
    kv->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_INT_VALUE;
    kv->value->int_value                               = val;
    scope->span->attributes[scope->span->n_attributes] = kv;

    scope->span->n_attributes += 1;
    free(t_kvs);
}
void neu_otel_scope_add_span_attr_double(neu_otel_scope_ctx ctx,
                                         const char *key, double val)
{
    trace_scope_t *scope = (trace_scope_t *) ctx;

    Opentelemetry__Proto__Common__V1__KeyValue **t_kvs =
        scope->span->attributes;

    scope->span->attributes =
        calloc(scope->span->n_attributes + 1,
               sizeof(Opentelemetry__Proto__Common__V1__KeyValue *));

    for (size_t i = 0; i < scope->span->n_attributes; i++) {
        scope->span->attributes[i] = t_kvs[i];
    }

    Opentelemetry__Proto__Common__V1__KeyValue *kv =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));
    opentelemetry__proto__common__v1__key_value__init(kv);
    kv->key   = strdup(key);
    kv->value = calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(kv->value);
    kv->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_DOUBLE_VALUE;
    kv->value->double_value                            = val;
    scope->span->attributes[scope->span->n_attributes] = kv;

    scope->span->n_attributes += 1;
    free(t_kvs);
}

void neu_otel_scope_add_span_attr_string(neu_otel_scope_ctx ctx,
                                         const char *key, const char *val)
{
    trace_scope_t *scope = (trace_scope_t *) ctx;

    Opentelemetry__Proto__Common__V1__KeyValue **t_kvs =
        scope->span->attributes;

    scope->span->attributes =
        calloc(scope->span->n_attributes + 1,
               sizeof(Opentelemetry__Proto__Common__V1__KeyValue *));

    for (size_t i = 0; i < scope->span->n_attributes; i++) {
        scope->span->attributes[i] = t_kvs[i];
    }

    Opentelemetry__Proto__Common__V1__KeyValue *kv =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));
    opentelemetry__proto__common__v1__key_value__init(kv);
    kv->key   = strdup(key);
    kv->value = calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(kv->value);
    kv->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_STRING_VALUE;
    kv->value->string_value                            = strdup(val);
    scope->span->attributes[scope->span->n_attributes] = kv;

    scope->span->n_attributes += 1;
    free(t_kvs);
}

void neu_otel_scope_add_span_attr_bool(neu_otel_scope_ctx ctx, const char *key,
                                       bool val)
{
    trace_scope_t *scope = (trace_scope_t *) ctx;

    Opentelemetry__Proto__Common__V1__KeyValue **t_kvs =
        scope->span->attributes;

    scope->span->attributes =
        calloc(scope->span->n_attributes + 1,
               sizeof(Opentelemetry__Proto__Common__V1__KeyValue *));

    for (size_t i = 0; i < scope->span->n_attributes; i++) {
        scope->span->attributes[i] = t_kvs[i];
    }

    Opentelemetry__Proto__Common__V1__KeyValue *kv =
        calloc(1, sizeof(Opentelemetry__Proto__Common__V1__KeyValue));
    opentelemetry__proto__common__v1__key_value__init(kv);
    kv->key   = strdup(key);
    kv->value = calloc(1, sizeof(Opentelemetry__Proto__Common__V1__AnyValue));
    opentelemetry__proto__common__v1__any_value__init(kv->value);
    kv->value->value_case =
        OPENTELEMETRY__PROTO__COMMON__V1__ANY_VALUE__VALUE_BOOL_VALUE;
    kv->value->bool_value                              = val;
    scope->span->attributes[scope->span->n_attributes] = kv;

    scope->span->n_attributes += 1;
    free(t_kvs);
}

void neu_otel_scope_set_span_start_time(neu_otel_scope_ctx ctx, int64_t ms)
{
    trace_scope_t *scope              = (trace_scope_t *) ctx;
    scope->span->start_time_unix_nano = ((uint64_t) ms) * 1000000;
}
void neu_otel_scope_set_span_end_time(neu_otel_scope_ctx ctx, int64_t ms)

{
    trace_scope_t *scope            = (trace_scope_t *) ctx;
    scope->span->end_time_unix_nano = ((uint64_t) ms) * 1000000;
    trace_ctx_t *trace_ctx          = (trace_ctx_t *) scope->trace_ctx;
    trace_ctx->span_num += 1;
}

uint8_t *neu_otel_scope_get_pre_span_id(neu_otel_scope_ctx ctx)
{
    trace_scope_t *scope     = (trace_scope_t *) ctx;
    trace_ctx_t *  trace_ctx = (trace_ctx_t *) scope->trace_ctx;
    if (scope->span_index <= 0) {
        return NULL;
    }
    return trace_ctx->trace_data.resource_spans[0]
        ->scope_spans[0]
        ->spans[scope->span_index - 1]
        ->span_id.data;
}

int neu_otel_trace_pack_size(neu_otel_trace_ctx ctx)
{
    trace_ctx_t *trace_ctx = (trace_ctx_t *) ctx;
    return opentelemetry__proto__trace__v1__traces_data__get_packed_size(
        &trace_ctx->trace_data);
}

int neu_otel_trace_pack(neu_otel_trace_ctx ctx, uint8_t *out)
{
    trace_ctx_t *trace_ctx = (trace_ctx_t *) ctx;
    return opentelemetry__proto__trace__v1__traces_data__pack(
        &trace_ctx->trace_data, out);
}

void neu_otel_new_span_id(char *id)
{
    static uint64_t counter   = 0;
    uint64_t        timestamp = time(NULL);
    uint64_t        random    = rand();

    uint64_t combined = (timestamp << 24) | (counter << 12) | random;
    counter++;

    for (int i = SPAN_ID_LENGTH - 1; i >= 0; i--) {
        id[i] = SPAN_ID_CHARSET[combined % 16];
        combined /= 16;
    }
    id[SPAN_ID_LENGTH] = '\0';
}

void neu_otel_split_traceparent(const char *in, char *trace_id, char *span_id,
                                uint32_t *flags)
{
    const char *delimiter = "-";
    char *      token;
    char *      saveptr;
    char *      copy        = strdup(in);
    char        t_flags[32] = { 0 };

    token = strtok_r(copy, delimiter, &saveptr);

    token = strtok_r(NULL, delimiter, &saveptr);
    if (token != NULL) {
        strcpy(trace_id, token);
    } else {
        strcpy(trace_id, "");
    }

    token = strtok_r(NULL, delimiter, &saveptr);
    if (token != NULL) {
        strcpy(span_id, token);
    } else {
        strcpy(span_id, "");
    }

    token = strtok_r(NULL, delimiter, &saveptr);
    if (token != NULL) {
        strcpy(t_flags, token);
        sscanf(t_flags, "%x", flags);
    } else {
        strcpy(t_flags, "");
        *flags = 0;
    }

    free(copy);
}

static int otel_timer_cb(void *data)
{
    (void) data;

    trace_ctx_table_ele_t *el = NULL, *tmp = NULL;

    pthread_mutex_lock(&table_mutex);

    HASH_ITER(hh, traces_table, el, tmp)
    {
        if (el->ctx->final &&
            el->ctx->span_num ==
                el->ctx->trace_data.resource_spans[0]
                    ->scope_spans[0]
                    ->n_spans) {
            int data_size = neu_otel_trace_pack_size(el->ctx);

            uint8_t *data_buf = calloc(1, data_size);
            neu_otel_trace_pack(el->ctx, data_buf);
            int status = neu_http_post_otel_trace(data_buf, data_size);
            free(data_buf);
            if (status == 200 || status == 400) {
                HASH_DEL(traces_table, el);
                neu_otel_free_trace(el->ctx);
                free(el);
            }
        }
    }

    pthread_mutex_unlock(&table_mutex);

    return 0;
}

void neu_otel_start()
{
    if (otel_event == NULL) {
        otel_event = neu_event_new();
    }

    if (otel_timer == NULL) {
        neu_event_timer_param_t param = { 0 };

        param.usr_data = NULL;

        param.second      = 0;
        param.millisecond = 100;
        param.cb          = otel_timer_cb;
        param.type        = NEU_EVENT_TIMER_BLOCK;

        otel_timer = neu_event_add_timer(otel_event, param);
    }
}

void neu_otel_stop()
{
    if (otel_timer) {
        neu_event_del_timer(otel_event, otel_timer);
        otel_timer = NULL;
    }

    if (otel_event) {
        neu_event_close(otel_event);
        otel_event = NULL;
    }
    trace_ctx_table_ele_t *el = NULL, *tmp = NULL;

    pthread_mutex_lock(&table_mutex);

    HASH_ITER(hh, traces_table, el, tmp)
    {

        HASH_DEL(traces_table, el);
        neu_otel_free_trace(el->ctx);
        free(el);
    }

    pthread_mutex_unlock(&table_mutex);
}
