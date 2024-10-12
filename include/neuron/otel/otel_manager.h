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

#ifndef NEU_OTEL_MANAGER_H
#define NEU_OTEL_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

typedef void *neu_otel_trace_ctx;
typedef void *neu_otel_scope_ctx;

neu_otel_trace_ctx neu_otel_create_trace(const char *trace_id, void *req_ctx,
                                         uint32_t    flags,
                                         const char *tracestate);

neu_otel_trace_ctx neu_otel_find_trace(void *req_ctx);

void neu_otel_trace_set_final(neu_otel_trace_ctx ctx);

void neu_otel_trace_set_expected_span_num(neu_otel_trace_ctx ctx, uint32_t num);
uint8_t *neu_otel_get_trace_id(neu_otel_trace_ctx ctx);

neu_otel_trace_ctx neu_otel_find_trace_by_id(const char *trace_id);

void neu_otel_free_trace(neu_otel_trace_ctx ctx);

neu_otel_scope_ctx neu_otel_add_span(neu_otel_trace_ctx ctx);
neu_otel_scope_ctx neu_otel_add_span2(neu_otel_trace_ctx ctx,
                                      const char *       span_name,
                                      const char *       span_id);

void neu_otel_scope_set_parent_span_id(neu_otel_scope_ctx ctx,
                                       const char *       parent_span_id);

void neu_otel_scope_set_parent_span_id2(neu_otel_scope_ctx ctx,
                                        uint8_t *parent_span_id, int len);

void neu_otel_scope_set_span_name(neu_otel_scope_ctx ctx,
                                  const char *       span_name);

void neu_otel_scope_set_span_flags(neu_otel_scope_ctx ctx, uint32_t flags);

void neu_otel_scope_set_span_id(neu_otel_scope_ctx ctx, const char *span_id);

void neu_otel_scope_add_span_attr_int(neu_otel_scope_ctx ctx, const char *key,
                                      int64_t val);
void neu_otel_scope_add_span_attr_double(neu_otel_scope_ctx ctx,
                                         const char *key, double val);
void neu_otel_scope_add_span_attr_string(neu_otel_scope_ctx ctx,
                                         const char *key, const char *val);

void neu_otel_scope_add_span_attr_bool(neu_otel_scope_ctx ctx, const char *key,
                                       bool val);

void neu_otel_scope_set_span_start_time(neu_otel_scope_ctx ctx, int64_t ms);
void neu_otel_scope_set_span_end_time(neu_otel_scope_ctx ctx, int64_t ms);

uint8_t *neu_otel_scope_get_pre_span_id(neu_otel_scope_ctx ctx);

int neu_otel_trace_pack_size(neu_otel_trace_ctx ctx);

int neu_otel_trace_pack(neu_otel_trace_ctx ctx, uint8_t *out);

void neu_otel_new_span_id(char *id);
void neu_otel_new_trace_id(char *id);

void neu_otel_split_traceparent(const char *in, char *trace_id, char *span_id,
                                uint32_t *flags);

void        neu_otel_start();
void        neu_otel_stop();
bool        neu_otel_control_is_started();
bool        neu_otel_data_is_started();
double      neu_otel_data_sample_rate();
const char *neu_otel_collector_url();
void        neu_otel_set_config(void *config);
void *      neu_otel_get_config();
const char *neu_otel_service_name();

#endif // NEU_OTEL_MANAGER_H