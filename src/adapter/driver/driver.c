/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
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
#include <float.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>

#define EPSILON 1e-9

#include "event/event.h"
#include "utils/http.h"
#include "utils/log.h"
#include "utils/time.h"
#include "utils/utextend.h"

#include "adapter.h"
#include "adapter/adapter_internal.h"
#include "adapter/storage.h"
#include "base/group.h"
#include "cache.h"
#include "driver_internal.h"
#include "errcodes.h"
#include "tag.h"

#include "core/node_manager.h"
#include "otel/otel_manager.h"

typedef struct to_be_write_tag {
    bool           single;
    neu_datatag_t *tag;
    neu_value_u    value;
    UT_array *     tvs;
    void *         req;
} to_be_write_tag_t;

typedef struct {
    char               app[NEU_NODE_NAME_LEN];
    struct sockaddr_un addr;
} sub_app_t;

typedef struct group {
    char *name;

    int64_t         timestamp;
    neu_group_t *   group;
    UT_array *      wt_tags;
    pthread_mutex_t wt_mtx;

    neu_event_timer_t *report;
    neu_event_timer_t *read;
    neu_event_timer_t *write;

    UT_array *      apps; // sub_app_t array
    pthread_mutex_t apps_mtx;

    neu_plugin_group_t    grp;
    neu_adapter_driver_t *driver;

    UT_hash_handle hh;
} group_t;

struct neu_adapter_driver {
    neu_adapter_t adapter;

    neu_driver_cache_t *cache;
    neu_events_t *      driver_events;

    size_t        tag_cnt;
    struct group *groups;
};

static void report_to_app(neu_adapter_driver_t *driver, group_t *group,
                          struct sockaddr_un dst);
static int  report_callback(void *usr_data);
static int  read_callback(void *usr_data);
static int  write_callback(void *usr_data);
static void read_group(int64_t timestamp, int64_t timeout,
                       neu_tag_cache_type_e cache_type,
                       neu_driver_cache_t *cache, const char *group,
                       UT_array *tags, UT_array *tag_values);
static void read_group_paginate(int64_t timestamp, int64_t timeout,
                                neu_tag_cache_type_e cache_type,
                                neu_driver_cache_t *cache, const char *group,
                                UT_array *tags, UT_array *tag_values);
static void read_report_group(int64_t timestamp, int64_t timeout,
                              neu_tag_cache_type_e cache_type,
                              neu_driver_cache_t *cache, const char *group,
                              UT_array *tags, UT_array *tag_values);
static void update_with_trace(neu_adapter_t *adapter, const char *group,
                              const char *tag, neu_dvalue_t value,
                              neu_tag_meta_t *metas, int n_meta,
                              void *trace_ctx);
static void update(neu_adapter_t *adapter, const char *group, const char *tag,
                   neu_dvalue_t value);
static void update_im(neu_adapter_t *adapter, const char *group,
                      const char *tag, neu_dvalue_t value,
                      neu_tag_meta_t *metas, int n_meta);
static void update_with_meta(neu_adapter_t *adapter, const char *group,
                             const char *tag, neu_dvalue_t value,
                             neu_tag_meta_t *metas, int n_meta);
static void write_response(neu_adapter_t *adapter, void *r, neu_error error);
static group_t *   find_group(neu_adapter_driver_t *driver, const char *name);
static void        store_write_tag(group_t *group, to_be_write_tag_t *tag);
static inline void start_group_timer(neu_adapter_driver_t *driver,
                                     group_t *             grp);
static inline void stop_group_timer(neu_adapter_driver_t *driver, group_t *grp);

static void format_tag_value(neu_dvalue_t *value)
{
    double scale = pow(10, 5);

    int negative = 1;

    if (value->value.d64 < 0) {
        value->value.d64 *= -1;
        negative = -1;
    }

    int64_t integer_part = (int64_t) value->value.d64;
    double  decimal_part = value->value.d64 - integer_part;
    decimal_part *= scale;
    decimal_part = round(decimal_part);
    char str[6]  = { 0 };
    snprintf(str, sizeof(str), "%05" PRId64 "", (int64_t) decimal_part);
    int i = 0, flag = 0;
    for (; i < 4; i++) {
        if (str[i] == '0' && str[i + 1] == '0') {
            flag = 1;
            break;
        } else if (str[i] == '9' && str[i + 1] == '9') {
            flag = 2;
            break;
        }
    }
    if (flag != 0 && i != 0) {
        decimal_part     = round(decimal_part / pow(10, 5 - i));
        value->value.d64 = (double) integer_part + decimal_part / pow(10, i);
    } else {
        value->value.d64 = (double) integer_part + decimal_part / scale;
    }

    value->value.d64 *= negative;
}

static void write_response(neu_adapter_t *adapter, void *r, neu_error error)
{
    neu_reqresp_head_t *req    = (neu_reqresp_head_t *) r;
    neu_resp_error_t    nerror = { .error = error };

    neu_otel_trace_ctx trace = NULL;
    neu_otel_scope_ctx scope = NULL;
    if (neu_otel_control_is_started()) {
        trace = neu_otel_find_trace(req->ctx);
        if (trace) {
            scope                = neu_otel_add_span(trace);
            char new_span_id[36] = { 0 };
            neu_otel_new_span_id(new_span_id);
            neu_otel_scope_set_span_id(scope, new_span_id);
            uint8_t *p_sp_id = neu_otel_scope_get_pre_span_id(scope);
            if (p_sp_id) {
                neu_otel_scope_set_parent_span_id2(scope, p_sp_id, 8);
            }
            neu_otel_scope_add_span_attr_int(scope, "thread id",
                                             (int64_t) pthread_self());
            neu_otel_scope_set_span_start_time(scope, neu_time_ns());
        }
    }

    if (NEU_REQ_WRITE_TAG == req->type) {
        neu_req_write_tag_fini((neu_req_write_tag_t *) &req[1]);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_span_name(scope, "driver write tag response");
        }
    } else if (NEU_REQ_WRITE_TAGS == req->type) {
        neu_req_write_tags_fini((neu_req_write_tags_t *) &req[1]);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_span_name(scope, "driver write tags response");
        }
    } else if (NEU_REQ_WRITE_GTAGS == req->type) {
        neu_req_write_gtags_fini((neu_req_write_gtags_t *) &req[1]);
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_span_name(scope, "driver write gtags response");
        }
    }

    req->type = NEU_RESP_ERROR;

    nlog_notice("write tag response start <%p>", req->ctx);

    adapter->cb_funs.response(adapter, req, &nerror);

    if (neu_otel_control_is_started() && trace) {
        if (error == NEU_ERR_SUCCESS) {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK, error);
        } else {
            neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_ERROR,
                                            error);
        }

        neu_otel_scope_set_span_end_time(scope, neu_time_ns());
    }
}

static void update_with_meta(neu_adapter_t *adapter, const char *group,
                             const char *tag, neu_dvalue_t value,
                             neu_tag_meta_t *metas, int n_meta)
{
    neu_adapter_driver_t *         driver = (neu_adapter_driver_t *) adapter;
    neu_adapter_update_metric_cb_t update_metric =
        driver->adapter.cb_funs.update_metric;

    if (value.type == NEU_TYPE_ERROR) {
        update_metric(&driver->adapter, NEU_METRIC_GROUP_LAST_ERROR_CODE,
                      value.value.i32, group);
        update_metric(&driver->adapter, NEU_METRIC_GROUP_LAST_ERROR_TS,
                      global_timestamp, group);
    }

    if (value.type == NEU_TYPE_ERROR && tag == NULL) {
        group_t *g = find_group(driver, group);
        if (g != NULL) {
            UT_array *tags      = neu_group_get_read_tag(g->group);
            uint64_t  err_count = 0;

            utarray_foreach(tags, neu_datatag_t *, t)
            {
                neu_driver_cache_update(driver->cache, group, t->name,
                                        global_timestamp, value, NULL, 0);
                ++err_count;
            }
            update_metric(&driver->adapter, NEU_METRIC_TAG_READS_TOTAL,
                          err_count, NULL);
            update_metric(&driver->adapter, NEU_METRIC_TAG_READ_ERRORS_TOTAL,
                          err_count, NULL);
            utarray_free(tags);
        }
    } else {
        neu_driver_cache_update(driver->cache, group, tag, global_timestamp,
                                value, metas, n_meta);
        update_metric(&driver->adapter, NEU_METRIC_TAG_READS_TOTAL, 1, NULL);
        update_metric(&driver->adapter, NEU_METRIC_TAG_READ_ERRORS_TOTAL,
                      NEU_TYPE_ERROR == value.type, NULL);
    }
    nlog_debug(
        "update driver: %s, group: %s, tag: %s, type: %s, timestamp: %" PRId64
        " n_meta: %d",
        driver->adapter.name, group, tag, neu_type_string(value.type),
        global_timestamp, n_meta);
}

static void update_with_trace(neu_adapter_t *adapter, const char *group,
                              const char *tag, neu_dvalue_t value,
                              neu_tag_meta_t *metas, int n_meta,
                              void *trace_ctx)
{
    update_with_meta(adapter, group, tag, value, metas, n_meta);
    if (trace_ctx) {
        neu_adapter_driver_t *driver = (neu_adapter_driver_t *) adapter;
        neu_driver_cache_update_trace(driver->cache, group, trace_ctx);
    }
}

static void update_im(neu_adapter_t *adapter, const char *group,
                      const char *tag, neu_dvalue_t value,
                      neu_tag_meta_t *metas, int n_meta)
{
    neu_adapter_driver_t *driver = (neu_adapter_driver_t *) adapter;

    if (tag == NULL) {
        nlog_warn("update_im tag is null");
        return;
    }

    neu_driver_cache_update_change(driver->cache, group, tag, global_timestamp,
                                   value, metas, n_meta, true);
    driver->adapter.cb_funs.update_metric(&driver->adapter,
                                          NEU_METRIC_TAG_READS_TOTAL, 1, NULL);
    if (value.type == NEU_TYPE_ERROR) {
        return;
    }

    UT_array *tags = neu_adapter_driver_get_ptag(driver, group, tag);
    if (tags == NULL) {
        return;
    }
    neu_datatag_t *first = utarray_front(tags);

    if (first == NULL) {
        utarray_free(tags);
        nlog_debug("update immediately, driver: %s, "
                   "group: %s, tag: %s, type: %s, "
                   "timestamp: %" PRId64,
                   driver->adapter.name, group, tag,
                   neu_type_string(value.type), global_timestamp);
        return;
    }

    nlog_debug("update and report immediately, driver: %s, "
               "group: %s, tag: %s, type: %s, "
               "timestamp: %" PRId64,
               driver->adapter.name, group, tag, neu_type_string(value.type),
               global_timestamp);

    neu_reqresp_head_t header = {
        .type = NEU_REQRESP_TRANS_DATA,
    };
    neu_reqresp_trans_data_t *data =
        calloc(1, sizeof(neu_reqresp_trans_data_t));

    data->driver = strdup(driver->adapter.name);
    data->group  = strdup(group);
    utarray_new(data->tags, neu_resp_tag_value_meta_icd());

    read_report_group(global_timestamp, 0,
                      neu_adapter_get_tag_cache_type(&driver->adapter),
                      driver->cache, group, tags, data->tags);

    if (utarray_len(data->tags) > 0) {
        group_t *find = NULL;
        HASH_FIND_STR(driver->groups, group, find);
        if (find != NULL) {
            pthread_mutex_lock(&find->apps_mtx);

            if (utarray_len(find->apps) > 0) {
                data->ctx = calloc(1, sizeof(neu_reqresp_trans_data_ctx_t));
                data->ctx->index = utarray_len(find->apps);
                pthread_mutex_init(&data->ctx->mtx, NULL);

                utarray_foreach(find->apps, sub_app_t *, app)
                {
                    if (driver->adapter.cb_funs.responseto(
                            &driver->adapter, &header, data, app->addr) != 0) {
                        neu_trans_data_free(data);
                    }
                }
            } else {
                utarray_foreach(data->tags, neu_resp_tag_value_meta_t *,
                                tag_value)
                {
                    if (tag_value->value.type == NEU_TYPE_PTR) {
                        free(tag_value->value.value.ptr.ptr);
                    } else {
                        neu_free_dvalue(&tag_value->value);
                    }
                }
                utarray_free(data->tags);
                free(data->group);
                free(data->driver);
            }

            pthread_mutex_unlock(&find->apps_mtx);
        } else {
            utarray_foreach(data->tags, neu_resp_tag_value_meta_t *, tag_value)
            {
                if (tag_value->value.type == NEU_TYPE_PTR) {
                    free(tag_value->value.value.ptr.ptr);
                } else {
                    neu_free_dvalue(&tag_value->value);
                }
            }
            utarray_free(data->tags);
            free(data->group);
            free(data->driver);
        }
    } else {
        utarray_free(data->tags);
        free(data->group);
        free(data->driver);
    }

    utarray_free(tags);
    free(data);
}

static void update(neu_adapter_t *adapter, const char *group, const char *tag,
                   neu_dvalue_t value)
{
    update_with_meta(adapter, group, tag, value, NULL, 0);
}

static void scan_tags_response(neu_adapter_t *adapter, void *r,
                               neu_resp_scan_tags_t *resp_scan)
{
    neu_reqresp_head_t *req = (neu_reqresp_head_t *) r;
    req->type               = NEU_RESP_SCAN_TAGS;
    nlog_notice("scan tags response <%p>", req->ctx);
    adapter->cb_funs.response(adapter, req, resp_scan);
}

static void test_read_tag_response(neu_adapter_t *adapter, void *r,
                                   neu_json_type_e t, neu_type_e type,
                                   neu_json_value_u value, int64_t error)
{
    neu_reqresp_head_t *     req    = (neu_reqresp_head_t *) r;
    neu_resp_test_read_tag_t dvalue = {
        .t     = t,
        .type  = type,
        .value = value,
        .error = error,
    };
    req->type = NEU_RESP_TEST_READ_TAG;
    nlog_notice("test reading tag response <%p>", req->ctx);

    adapter->cb_funs.response(adapter, req, &dvalue);
}

neu_adapter_driver_t *neu_adapter_driver_create()
{
    neu_adapter_driver_t *driver = calloc(1, sizeof(neu_adapter_driver_t));

    driver->cache                                     = neu_driver_cache_new();
    driver->driver_events                             = neu_event_new();
    driver->adapter.cb_funs.driver.update             = update;
    driver->adapter.cb_funs.driver.write_response     = write_response;
    driver->adapter.cb_funs.driver.update_im          = update_im;
    driver->adapter.cb_funs.driver.update_with_trace  = update_with_trace;
    driver->adapter.cb_funs.driver.update_with_meta   = update_with_meta;
    driver->adapter.cb_funs.driver.scan_tags_response = scan_tags_response;
    driver->adapter.cb_funs.driver.test_read_tag_response =
        test_read_tag_response;

    return driver;
}

void neu_adapter_driver_destroy(neu_adapter_driver_t *driver)
{
    neu_event_close(driver->driver_events);
    neu_driver_cache_destroy(driver->cache);
}

int neu_adapter_driver_init(neu_adapter_driver_t *driver)
{
    (void) driver;

    return 0;
}

int neu_adapter_driver_uninit(neu_adapter_driver_t *driver)
{
    group_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, driver->groups, el, tmp)
    {
        HASH_DEL(driver->groups, el);

        neu_adapter_driver_try_del_tag(driver, neu_group_tag_size(el->group));
        stop_group_timer(driver, el);
        if (el->grp.group_free != NULL) {
            el->grp.group_free(&el->grp);
        }
        free(el->grp.group_name);
        if (el->grp.context) {
            free(el->grp.context);
        }
        free(el->name);
        utarray_free(el->grp.tags);

        utarray_foreach(el->wt_tags, to_be_write_tag_t *, tag)
        {
            neu_tag_free(tag->tag);
        }

        utarray_free(el->wt_tags);
        utarray_free(el->apps);
        neu_group_destroy(el->group);
        free(el);
    }

    return 0;
}

static inline void start_group_timer(neu_adapter_driver_t *driver, group_t *grp)
{
    uint32_t interval = neu_group_get_interval(grp->group);

    neu_event_timer_param_t param = {
        .second      = interval / 1000,
        .millisecond = interval % 1000,
        .usr_data    = (void *) grp,
        .type        = NEU_EVENT_TIMER_NOBLOCK,
    };

    param.type = driver->adapter.module->timer_type;
    param.cb   = read_callback;
    grp->read  = neu_event_add_timer(driver->driver_events, param);

    struct timespec t1 = {
        .tv_sec  = 0,
        .tv_nsec = 1000 * 1000 * 20,
    };
    struct timespec t2 = { 0 };
    nanosleep(&t1, &t2);

    param.type  = NEU_EVENT_TIMER_NOBLOCK;
    param.cb    = report_callback;
    grp->report = neu_adapter_add_timer((neu_adapter_t *) driver, param);

    param.second      = 0;
    param.millisecond = 3;
    param.cb          = write_callback;
    grp->write        = neu_event_add_timer(driver->driver_events, param);
}

void neu_adapter_driver_start_group_timer(neu_adapter_driver_t *driver)
{
    group_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, driver->groups, el, tmp)
    {
        start_group_timer(driver, el);
        neu_adapter_update_group_metric(
            &driver->adapter, neu_group_get_name(el->group),
            NEU_METRIC_GROUP_TAGS_TOTAL, neu_group_tag_size(el->group));
    }

    driver->adapter.cb_funs.update_metric(
        &driver->adapter, NEU_METRIC_TAGS_TOTAL, driver->tag_cnt, NULL);
}

static inline void stop_group_timer(neu_adapter_driver_t *driver, group_t *grp)
{
    if (grp->report) {
        neu_adapter_del_timer((neu_adapter_t *) driver, grp->report);
        grp->report = NULL;
    }
    if (grp->read) {
        neu_event_del_timer(driver->driver_events, grp->read);
        grp->read = NULL;
    }
    if (grp->write) {
        neu_event_del_timer(driver->driver_events, grp->write);
        grp->write = NULL;
    }
}

void neu_adapter_driver_stop_group_timer(neu_adapter_driver_t *driver)
{
    group_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, driver->groups, el, tmp) { stop_group_timer(driver, el); }
}

void neu_adapter_driver_read_group(neu_adapter_driver_t *driver,
                                   neu_reqresp_head_t *  req)
{
    neu_req_read_group_t *cmd = (neu_req_read_group_t *) &req[1];
    group_t *             g   = find_group(driver, cmd->group);
    if (g == NULL) {
        neu_resp_error_t error = { .error = NEU_ERR_GROUP_NOT_EXIST };
        req->type              = NEU_RESP_ERROR;
        neu_req_read_group_fini(cmd);
        driver->adapter.cb_funs.response(&driver->adapter, req, &error);
        return;
    }

    neu_resp_read_group_t resp  = { 0 };
    neu_group_t *         group = g->group;
    UT_array *tags = neu_group_query_read_tag(group, cmd->name, cmd->desc,
                                              cmd->n_tag, cmd->tags);

    utarray_new(resp.tags, neu_resp_tag_value_meta_icd());

    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        utarray_foreach(tags, neu_datatag_t *, tag)
        {
            neu_resp_tag_value_meta_t tag_value = { 0 };
            strcpy(tag_value.tag, tag->name);
            tag_value.value.type      = NEU_TYPE_ERROR;
            tag_value.value.value.i32 = NEU_ERR_PLUGIN_NOT_RUNNING;

            utarray_push_back(resp.tags, &tag_value);
        }
    } else if (cmd->sync) {
        if (NULL == driver->adapter.module->intf_funs->driver.group_sync) {
            // plugin does not support sync read
            utarray_foreach(tags, neu_datatag_t *, tag)
            {
                neu_resp_tag_value_meta_t tag_value = { 0 };
                strcpy(tag_value.tag, tag->name);
                tag_value.value.type = NEU_TYPE_ERROR;
                tag_value.value.value.i32 =
                    NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC;

                utarray_push_back(resp.tags, &tag_value);
            }
        } else {
            // sync read to update cache
            stop_group_timer(driver, g);
            driver->adapter.module->intf_funs->driver.group_sync(
                driver->adapter.plugin, &g->grp);
            // fetch updated data from cache
            read_group(global_timestamp,
                       neu_group_get_interval(group) *
                           NEU_DRIVER_TAG_CACHE_EXPIRE_TIME,
                       neu_adapter_get_tag_cache_type(&driver->adapter),
                       driver->cache, cmd->group, tags, resp.tags);
            start_group_timer(driver, g);
        }
    } else {
        read_group(global_timestamp,
                   neu_group_get_interval(group) *
                       NEU_DRIVER_TAG_CACHE_EXPIRE_TIME,
                   neu_adapter_get_tag_cache_type(&driver->adapter),
                   driver->cache, cmd->group, tags, resp.tags);
    }

    resp.driver = cmd->driver;
    resp.group  = cmd->group;
    cmd->driver = NULL; // ownership moved
    cmd->group  = NULL; // ownership moved

    utarray_free(tags);
    neu_req_read_group_fini(cmd);

    req->type = NEU_RESP_READ_GROUP;
    driver->adapter.cb_funs.response(&driver->adapter, req, &resp);
}

void neu_adapter_driver_read_group_paginate(neu_adapter_driver_t *driver,
                                            neu_reqresp_head_t *  req)
{
    neu_req_read_group_paginate_t *cmd =
        (neu_req_read_group_paginate_t *) &req[1];
    group_t *g = find_group(driver, cmd->group);
    if (g == NULL) {
        neu_resp_error_t error = { .error = NEU_ERR_GROUP_NOT_EXIST };
        req->type              = NEU_RESP_ERROR;
        neu_req_read_group_paginate_fini(cmd);
        driver->adapter.cb_funs.response(&driver->adapter, req, &error);
        return;
    }

    neu_resp_read_group_paginate_t resp  = { 0 };
    neu_group_t *                  group = g->group;
    UT_array *                     tags;

    if (cmd->is_error != true && cmd->current_page > 0 && cmd->page_size > 0) {
        tags = neu_group_query_read_tag_paginate(
            group, cmd->name, cmd->desc, cmd->current_page, cmd->page_size,
            &resp.total_count);
    } else {
        tags = neu_group_query_read_tag(group, cmd->name, cmd->desc, 0, NULL);
        resp.total_count = utarray_len(tags);
    }

    utarray_new(resp.tags, neu_resp_tag_value_meta_paginate_icd());

    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        utarray_foreach(tags, neu_datatag_t *, tag)
        {
            neu_resp_tag_value_meta_paginate_t tag_value = { 0 };
            strcpy(tag_value.tag, tag->name);
            tag_value.value.type      = NEU_TYPE_ERROR;
            tag_value.value.value.i32 = NEU_ERR_PLUGIN_NOT_RUNNING;

            tag_value.datatag.name        = strdup(tag->name);
            tag_value.datatag.address     = strdup(tag->address);
            tag_value.datatag.attribute   = tag->attribute;
            tag_value.datatag.type        = tag->type;
            tag_value.datatag.precision   = tag->precision;
            tag_value.datatag.decimal     = tag->decimal;
            tag_value.datatag.bias        = tag->bias;
            tag_value.datatag.description = strdup(tag->description);
            tag_value.datatag.option      = tag->option;
            memcpy(tag_value.datatag.meta, tag->meta, NEU_TAG_META_LENGTH);

            utarray_push_back(resp.tags, &tag_value);
        }
    } else if (cmd->sync) {
        if (NULL == driver->adapter.module->intf_funs->driver.group_sync) {
            // plugin does not support sync read
            utarray_foreach(tags, neu_datatag_t *, tag)
            {
                neu_resp_tag_value_meta_paginate_t tag_value = { 0 };
                strcpy(tag_value.tag, tag->name);
                tag_value.value.type = NEU_TYPE_ERROR;
                tag_value.value.value.i32 =
                    NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC;

                tag_value.datatag.name        = strdup(tag->name);
                tag_value.datatag.address     = strdup(tag->address);
                tag_value.datatag.attribute   = tag->attribute;
                tag_value.datatag.type        = tag->type;
                tag_value.datatag.precision   = tag->precision;
                tag_value.datatag.decimal     = tag->decimal;
                tag_value.datatag.bias        = tag->bias;
                tag_value.datatag.description = strdup(tag->description);
                tag_value.datatag.option      = tag->option;
                memcpy(tag_value.datatag.meta, tag->meta, NEU_TAG_META_LENGTH);

                utarray_push_back(resp.tags, &tag_value);
            }
        } else {
            // sync read to update cache
            stop_group_timer(driver, g);
            driver->adapter.module->intf_funs->driver.group_sync(
                driver->adapter.plugin, &g->grp);
            // fetch updated data from cache
            read_group_paginate(
                global_timestamp,
                neu_group_get_interval(group) *
                    NEU_DRIVER_TAG_CACHE_EXPIRE_TIME,
                neu_adapter_get_tag_cache_type(&driver->adapter), driver->cache,
                cmd->group, tags, resp.tags);
            start_group_timer(driver, g);
        }
    } else {
        read_group_paginate(global_timestamp,
                            neu_group_get_interval(group) *
                                NEU_DRIVER_TAG_CACHE_EXPIRE_TIME,
                            neu_adapter_get_tag_cache_type(&driver->adapter),
                            driver->cache, cmd->group, tags, resp.tags);
    }

    resp.driver       = cmd->driver;
    resp.group        = cmd->group;
    resp.current_page = cmd->current_page;
    resp.page_size    = cmd->page_size;
    resp.is_error     = cmd->is_error;
    cmd->driver       = NULL; // ownership moved
    cmd->group        = NULL; // ownership moved

    utarray_free(tags);
    neu_req_read_group_paginate_fini(cmd);

    req->type = NEU_RESP_READ_GROUP_PAGINATE;
    driver->adapter.cb_funs.response(&driver->adapter, req, &resp);
}

static void fix_value(neu_datatag_t *tag, neu_type_e value_type,
                      neu_dvalue_t *value)
{
    switch (tag->type) {
    case NEU_TYPE_BOOL:
    case NEU_TYPE_STRING:
    case NEU_TYPE_TIME:
    case NEU_TYPE_DATA_AND_TIME:
    case NEU_TYPE_ARRAY_INT64:
    case NEU_TYPE_ARRAY_CHAR:
    case NEU_TYPE_ARRAY_BOOL:
    case NEU_TYPE_ARRAY_STRING:
    case NEU_TYPE_CUSTOM:
        break;
    case NEU_TYPE_BIT:
        value->type     = NEU_TYPE_BIT;
        value->value.u8 = (uint8_t) value->value.u64;
        break;
    case NEU_TYPE_UINT8:
    case NEU_TYPE_INT8:
        value->type     = NEU_TYPE_UINT8;
        value->value.u8 = (uint8_t) value->value.u64;
        break;
    case NEU_TYPE_INT16:
    case NEU_TYPE_UINT16:
    case NEU_TYPE_WORD:
        value->type      = NEU_TYPE_UINT16;
        value->value.u16 = (uint16_t) value->value.u64;
        if (tag->option.value16.endian == NEU_DATATAG_ENDIAN_B16) {
            value->value.u16 = htons(value->value.u16);
        }
        break;
    case NEU_TYPE_UINT32:
    case NEU_TYPE_INT32:
    case NEU_TYPE_DWORD:
        value->type      = NEU_TYPE_UINT32;
        value->value.u32 = (uint32_t) value->value.u64;
        switch (tag->option.value32.endian) {
        case NEU_DATATAG_ENDIAN_LB32:
            neu_ntohs_p((uint16_t *) value->value.bytes.bytes);
            neu_ntohs_p((uint16_t *) (value->value.bytes.bytes + 2));
            break;
        case NEU_DATATAG_ENDIAN_BB32:
            value->value.u32 = htonl(value->value.u32);
            break;
        case NEU_DATATAG_ENDIAN_BL32:
            value->value.u32 = htonl(value->value.u32);
            neu_ntohs_p((uint16_t *) value->value.bytes.bytes);
            neu_ntohs_p((uint16_t *) (value->value.bytes.bytes + 2));
            break;
        case NEU_DATATAG_ENDIAN_LL32:
        default:
            break;
        }
        break;
    case NEU_TYPE_FLOAT:
        if (value_type != NEU_TYPE_FLOAT) {
            value->type      = NEU_TYPE_FLOAT;
            value->value.f32 = (float) value->value.d64;
        }

        switch (tag->option.value32.endian) {
        case NEU_DATATAG_ENDIAN_LB32:
            neu_ntohs_p((uint16_t *) value->value.bytes.bytes);
            neu_ntohs_p((uint16_t *) (value->value.bytes.bytes + 2));
            break;
        case NEU_DATATAG_ENDIAN_BB32:
            value->value.u32 = htonl(value->value.u32);
            break;
        case NEU_DATATAG_ENDIAN_BL32:
            value->value.u32 = htonl(value->value.u32);
            neu_ntohs_p((uint16_t *) value->value.bytes.bytes);
            neu_ntohs_p((uint16_t *) (value->value.bytes.bytes + 2));
            break;
        case NEU_DATATAG_ENDIAN_LL32:
        default:
            break;
        }

        break;
    case NEU_TYPE_DOUBLE:
    case NEU_TYPE_UINT64:
    case NEU_TYPE_INT64:
    case NEU_TYPE_LWORD:
        switch (tag->option.value64.endian) {
        case NEU_DATATAG_ENDIAN_B64:
        case NEU_DATATAG_ENDIAN_BB64:
            value->value.u64 = neu_ntohll(value->value.u64);
            break;
        case NEU_DATATAG_ENDIAN_LB64:
            value->value.u64 = neu_htonlb(value->value.u64);
            break;
        case NEU_DATATAG_ENDIAN_BL64:
            value->value.u64 = neu_htonbl(value->value.u64);
            break;
        case NEU_DATATAG_ENDIAN_L64:
        case NEU_DATATAG_ENDIAN_LL64:
        default:
            break;
        }
        break;
    case NEU_TYPE_BYTES:
        if (value->type == NEU_TYPE_ARRAY_INT64) {
            int64_t *i64s     = value->value.i64s.i64s;
            uint32_t i64s_len = value->value.i64s.length > NEU_VALUE_SIZE
                ? NEU_VALUE_SIZE
                : value->value.i64s.length;
            for (size_t i = 0; i < i64s_len; i++) {
                value->value.bytes.bytes[i] = (uint8_t) i64s[i];
            }
            for (int i = i64s_len; i < NEU_VALUE_SIZE; i++) {
                value->value.bytes.bytes[i] = 0;
            }
            value->value.bytes.length = i64s_len;
            free(i64s);
        }
        break;
    case NEU_TYPE_ARRAY_INT8:
        if (value->type == NEU_TYPE_ARRAY_INT64) {
            int64_t *i64s        = value->value.i64s.i64s;
            value->value.i8s.i8s = calloc(value->value.i64s.length, 1);
            for (size_t i = 0; i < value->value.i64s.length; i++) {
                value->value.i8s.i8s[i] = (int8_t) i64s[i];
            }
            free(i64s);
        }
        break;
    case NEU_TYPE_ARRAY_UINT8:
        if (value->type == NEU_TYPE_ARRAY_INT64) {
            int64_t *i64s        = value->value.i64s.i64s;
            value->value.u8s.u8s = calloc(value->value.i64s.length, 1);
            for (size_t i = 0; i < value->value.i64s.length; i++) {
                value->value.u8s.u8s[i] = (uint8_t) i64s[i];
            }
            free(i64s);
        }
        break;
    case NEU_TYPE_ARRAY_INT16:
        if (value->type == NEU_TYPE_ARRAY_INT64) {
            int64_t *i64s          = value->value.i64s.i64s;
            value->value.i16s.i16s = calloc(value->value.i64s.length, 2);
            for (size_t i = 0; i < value->value.i64s.length; i++) {
                value->value.i16s.i16s[i] = (int16_t) i64s[i];
            }
            free(i64s);
        }
        break;
    case NEU_TYPE_ARRAY_UINT16:
        if (value->type == NEU_TYPE_ARRAY_INT64) {
            int64_t *i64s          = value->value.i64s.i64s;
            value->value.u16s.u16s = calloc(value->value.i64s.length, 2);
            for (size_t i = 0; i < value->value.i64s.length; i++) {
                value->value.u16s.u16s[i] = (uint16_t) i64s[i];
            }
            free(i64s);
        }
        break;
    case NEU_TYPE_ARRAY_INT32:
        if (value->type == NEU_TYPE_ARRAY_INT64) {
            int64_t *i64s          = value->value.i64s.i64s;
            value->value.i32s.i32s = calloc(value->value.i64s.length, 4);
            for (size_t i = 0; i < value->value.i64s.length; i++) {
                value->value.i32s.i32s[i] = (int32_t) i64s[i];
            }
            free(i64s);
        }
        break;
    case NEU_TYPE_ARRAY_UINT32:
        if (value->type == NEU_TYPE_ARRAY_INT64) {
            int64_t *i64s          = value->value.i64s.i64s;
            value->value.u32s.u32s = calloc(value->value.i64s.length, 4);
            for (size_t i = 0; i < value->value.i64s.length; i++) {
                value->value.u32s.u32s[i] = (uint32_t) i64s[i];
            }
            free(i64s);
        }
        break;
    case NEU_TYPE_ARRAY_UINT64:
        if (value->type == NEU_TYPE_ARRAY_INT64) {
            int64_t *i64s          = value->value.i64s.i64s;
            value->value.u64s.u64s = calloc(value->value.i64s.length, 8);
            for (size_t i = 0; i < value->value.i64s.length; i++) {
                value->value.u64s.u64s[i] = (uint64_t) i64s[i];
            }
            free(i64s);
        }
        break;
    case NEU_TYPE_ARRAY_FLOAT:
        if (value->type == NEU_TYPE_ARRAY_INT64) {
            int64_t *i64s          = value->value.i64s.i64s;
            value->value.f32s.f32s = calloc(value->value.i64s.length, 4);
            for (size_t i = 0; i < value->value.i64s.length; i++) {
                value->value.f32s.f32s[i] = (float) i64s[i];
            }
            free(i64s);
        }
        if (value->type == NEU_TYPE_ARRAY_DOUBLE) {
            double *f64s           = value->value.f64s.f64s;
            value->value.f32s.f32s = calloc(value->value.f64s.length, 4);
            for (size_t i = 0; i < value->value.f64s.length; i++) {
                value->value.f32s.f32s[i] = (float) f64s[i];
            }
            free(f64s);
        }
        break;
    case NEU_TYPE_ARRAY_DOUBLE:
        if (value->type == NEU_TYPE_ARRAY_INT64) {
            int64_t *i64s          = value->value.i64s.i64s;
            value->value.f64s.f64s = calloc(value->value.i64s.length, 8);
            for (size_t i = 0; i < value->value.i64s.length; i++) {
                value->value.f64s.f64s[i] = (double) i64s[i];
            }
            free(i64s);
        }
        break;
    default:
        assert(false);
        break;
    }
}

static void cal_decimal(neu_type_e tag_type, neu_type_e value_type,
                        neu_value_u *value, double decimal)
{
    switch (tag_type) {
    case NEU_TYPE_INT8:
        if (value_type == NEU_TYPE_DOUBLE) {
            value->i8 = (int8_t) round(value->d64 / decimal);
        } else {
            value->i8 = (int8_t) round(value->i64 / decimal);
        }
        break;
    case NEU_TYPE_INT16:
        if (value_type == NEU_TYPE_DOUBLE) {
            value->i16 = (int16_t) round(value->d64 / decimal);
        } else {
            value->i16 = (int16_t) round(value->i64 / decimal);
        }
        break;
    case NEU_TYPE_INT32:
        if (value_type == NEU_TYPE_DOUBLE) {
            value->i32 = (int32_t) round(value->d64 / decimal);
        } else {
            value->i32 = (int32_t) round(value->i64 / decimal);
        }
        break;
    case NEU_TYPE_INT64:
        if (value_type == NEU_TYPE_DOUBLE) {
            value->i64 = (int64_t) round(value->d64 / decimal);
        } else {
            value->i64 = (int64_t) round(value->i64 / decimal);
        }
        break;
    case NEU_TYPE_UINT8:
        if (value_type == NEU_TYPE_DOUBLE) {
            value->u8 = (uint8_t) round(value->d64 / decimal);
        } else {
            value->u8 = (uint8_t) round(value->u64 / decimal);
        }
        break;
    case NEU_TYPE_UINT16:
    case NEU_TYPE_WORD:
        if (value_type == NEU_TYPE_DOUBLE) {
            value->u16 = (uint16_t) round(value->d64 / decimal);
        } else {
            value->u16 = (uint16_t) round(value->u64 / decimal);
        }
        break;
    case NEU_TYPE_UINT32:
    case NEU_TYPE_LWORD:
        if (value_type == NEU_TYPE_DOUBLE) {
            value->u32 = (uint32_t) round(value->d64 / decimal);
        } else {
            value->u32 = (uint32_t) round(value->u64 / decimal);
        }
        break;
    case NEU_TYPE_UINT64:
    case NEU_TYPE_DWORD:
        if (value_type == NEU_TYPE_DOUBLE) {
            value->u64 = (uint64_t) round(value->d64 / decimal);
        } else {
            value->u64 = (uint64_t) round(value->u64 / decimal);
        }
        break;
    case NEU_TYPE_FLOAT:
    case NEU_TYPE_DOUBLE:
        if (value_type == NEU_TYPE_INT64) {
            value->d64 = (double) (value->d64 / decimal);
        } else if (value_type == NEU_TYPE_DOUBLE) {
            value->d64 = value->d64 / decimal;
        } else if (value_type == NEU_TYPE_FLOAT) {
            value->f32 = value->f32 / decimal;
        }
        break;
    default:
        break;
    }
}

int almost_equal(double a, double b, double epsilon)
{
    return fabs(a - b) < epsilon;
}

int check_value_decimal(neu_type_e write_type, double result, int64_t value_min,
                        int64_t value_max)
{
    if (write_type != NEU_TYPE_INT64 && write_type != NEU_TYPE_DOUBLE) {
        return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
    } else if (result < value_min || result > value_max) {
        return NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE;
    } else if (almost_equal(ceil(result), result, EPSILON) ||
               almost_equal(floor(result), result, EPSILON)) {
        return NEU_ERR_SUCCESS;
    } else {
        return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
    }
}

int check_value(neu_type_e write_type, int64_t value, int64_t value_min,
                int64_t value_max)
{
    if (write_type != NEU_TYPE_INT64) {
        return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
    } else if (value >= value_min && value <= value_max) {
        return NEU_ERR_SUCCESS;
    } else {
        return NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE;
    }
}

int is_value_in_range(neu_type_e tag_type, int64_t value, double value_d,
                      neu_type_e write_type, double decimal)
{
    if (decimal != 0) {
        double result;

        if (write_type == NEU_TYPE_INT64) {
            result = value / decimal;
        } else {
            result = value_d / decimal;
        }

        switch (tag_type) {
        case NEU_TYPE_INT8:
            return check_value_decimal(write_type, result, INT8_MIN, INT8_MAX);
        case NEU_TYPE_UINT8:
            return check_value_decimal(write_type, result, 0, UINT8_MAX);
        case NEU_TYPE_INT16:
            return check_value_decimal(write_type, result, INT16_MIN,
                                       INT16_MAX);
        case NEU_TYPE_UINT16:
            return check_value_decimal(write_type, result, 0, UINT16_MAX);
        case NEU_TYPE_INT32:
            return check_value_decimal(write_type, result, INT32_MIN,
                                       INT32_MAX);
        case NEU_TYPE_UINT32:
            return check_value_decimal(write_type, result, 0, UINT32_MAX);
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
            if (write_type != NEU_TYPE_INT64 && write_type != NEU_TYPE_DOUBLE) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            } else if (almost_equal(ceil(result), result, EPSILON) ||
                       almost_equal(floor(result), result, EPSILON)) {
                return NEU_ERR_SUCCESS;
            } else {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            }
        case NEU_TYPE_FLOAT:
            if (write_type != NEU_TYPE_DOUBLE && write_type != NEU_TYPE_INT64) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            }
            if (result < (int64_t) -FLT_MAX || result > (int64_t) FLT_MAX) {
                return NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE;
            } else {
                return NEU_ERR_SUCCESS;
            }
        case NEU_TYPE_DOUBLE:
            if (write_type != NEU_TYPE_DOUBLE && write_type != NEU_TYPE_INT64) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            }
            return NEU_ERR_SUCCESS;
        case NEU_TYPE_STRING:
        case NEU_TYPE_BOOL:
        case NEU_TYPE_BIT:
            return NEU_ERR_TAG_DECIMAL_INVALID;
        default:
            return NEU_ERR_SUCCESS;
        }
    } else {
        switch (tag_type) {
        case NEU_TYPE_BOOL:
            if (write_type == NEU_TYPE_BOOL) {
                return NEU_ERR_SUCCESS;
            } else {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            }
        case NEU_TYPE_BIT:
            if (write_type != NEU_TYPE_INT64) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            } else if (value != 1 && value != 0) {
                return NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE;
            } else {
                return NEU_ERR_SUCCESS;
            }
        case NEU_TYPE_INT8:
            return check_value(write_type, value, INT8_MIN, INT8_MAX);
        case NEU_TYPE_UINT8:
            return check_value(write_type, value, 0, UINT8_MAX);
        case NEU_TYPE_INT16:
            return check_value(write_type, value, INT16_MIN, INT16_MAX);
        case NEU_TYPE_UINT16:
            return check_value(write_type, value, 0, UINT16_MAX);
        case NEU_TYPE_INT32:
            return check_value(write_type, value, INT32_MIN, INT32_MAX);
        case NEU_TYPE_UINT32:
            return check_value(write_type, value, 0, UINT32_MAX);
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
            if (write_type == NEU_TYPE_INT64) {
                return NEU_ERR_SUCCESS;
            } else {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            }
        case NEU_TYPE_FLOAT:
            if (write_type != NEU_TYPE_DOUBLE && write_type != NEU_TYPE_INT64) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            }
            if (value < (int64_t) -FLT_MAX || value > (int64_t) FLT_MAX) {
                return NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE;
            } else {
                return NEU_ERR_SUCCESS;
            }
        case NEU_TYPE_DOUBLE:
            if (write_type != NEU_TYPE_DOUBLE && write_type != NEU_TYPE_INT64) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            }
            return NEU_ERR_SUCCESS;
        case NEU_TYPE_STRING:
            if (write_type != NEU_TYPE_STRING) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            } else {
                return NEU_ERR_SUCCESS;
            }
        case NEU_TYPE_CUSTOM:
            if (write_type != NEU_TYPE_CUSTOM) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            } else {
                return NEU_ERR_SUCCESS;
            }
        case NEU_TYPE_ARRAY_UINT8:
        case NEU_TYPE_ARRAY_INT8:
        case NEU_TYPE_ARRAY_UINT16:
        case NEU_TYPE_ARRAY_INT16:
        case NEU_TYPE_ARRAY_UINT32:
        case NEU_TYPE_ARRAY_INT32:
        case NEU_TYPE_ARRAY_UINT64:
        case NEU_TYPE_ARRAY_INT64:
            if (write_type != NEU_TYPE_ARRAY_INT64) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            } else {
                return NEU_ERR_SUCCESS;
            }
        case NEU_TYPE_ARRAY_FLOAT:
        case NEU_TYPE_ARRAY_DOUBLE:
            if (write_type != NEU_TYPE_ARRAY_DOUBLE &&
                write_type != NEU_TYPE_ARRAY_INT64) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            } else {
                return NEU_ERR_SUCCESS;
            }
        case NEU_TYPE_ARRAY_STRING:
            if (write_type != NEU_TYPE_ARRAY_STRING) {
                return NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH;
            } else {
                return NEU_ERR_SUCCESS;
            }
        default:
            return NEU_ERR_SUCCESS;
        }
    }
}

int neu_adapter_driver_write_tags(neu_adapter_driver_t *driver,
                                  neu_reqresp_head_t *  req)
{
    neu_req_write_tags_t *cmd = (neu_req_write_tags_t *) &req[1];

    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        driver->adapter.cb_funs.driver.write_response(
            &driver->adapter, req, NEU_ERR_PLUGIN_NOT_RUNNING);
        return NEU_ERR_PLUGIN_NOT_RUNNING;
    }

    if (driver->adapter.module->intf_funs->driver.write_tags == NULL) {
        driver->adapter.cb_funs.driver.write_response(
            &driver->adapter, req, NEU_ERR_PLUGIN_NOT_SUPPORT_WRITE_TAGS);
        return NEU_ERR_PLUGIN_NOT_SUPPORT_WRITE_TAGS;
    }

    group_t *g = find_group(driver, cmd->group);

    if (g == NULL) {
        driver->adapter.cb_funs.driver.write_response(&driver->adapter, req,
                                                      NEU_ERR_GROUP_NOT_EXIST);
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    UT_array *tags = NULL;
    UT_icd    icd  = { sizeof(neu_plugin_tag_value_t), NULL, NULL, NULL };
    utarray_new(tags, &icd);
    int value_err = NEU_ERR_SUCCESS;
    int value_check;

    for (int i = 0; i < cmd->n_tag; i++) {
        neu_datatag_t *tag = neu_group_find_tag(g->group, cmd->tags[i].tag);

        if (tag != NULL) {
            value_check =
                is_value_in_range(tag->type, cmd->tags[i].value.value.i64,
                                  cmd->tags[i].value.value.d64,
                                  cmd->tags[i].value.type, tag->decimal);
        } else {
            value_check = NEU_ERR_TAG_NOT_EXIST;
        }

        if (tag != NULL && neu_tag_attribute_test(tag, NEU_ATTRIBUTE_WRITE) &&
            value_check == NEU_ERR_SUCCESS) {
            if (tag->type == NEU_TYPE_FLOAT || tag->type == NEU_TYPE_DOUBLE) {
                if (cmd->tags[i].value.type == NEU_TYPE_INT64) {
                    cmd->tags[i].value.value.d64 =
                        (double) cmd->tags[i].value.value.i64;
                }
            }

            if (tag->decimal != 0) {
                cal_decimal(tag->type, cmd->tags[i].value.type,
                            &cmd->tags[i].value.value, tag->decimal);
            }
            fix_value(tag, cmd->tags[i].value.type, &cmd->tags[i].value);

            neu_plugin_tag_value_t tv = {
                .tag   = neu_tag_dup(tag),
                .value = cmd->tags[i].value.value,
                .type  = cmd->tags[i].value.type,
            };
            utarray_push_back(tags, &tv);
        } else {
            value_err = value_check;
        }
        if (tag != NULL) {
            neu_tag_free(tag);
        }
    }

    if (value_err != NEU_ERR_SUCCESS) {
        driver->adapter.cb_funs.driver.write_response(&driver->adapter, req,
                                                      value_err);
        utarray_foreach(tags, neu_plugin_tag_value_t *, tv)
        {
            neu_tag_free(tv->tag);
            neu_dvalue_t dvalue = { .type = tv->type, .value = tv->value };
            neu_free_dvalue(&dvalue);
        }
        utarray_free(tags);
        return value_err;
    }

    if (utarray_len(tags) != (unsigned int) cmd->n_tag) {
        driver->adapter.cb_funs.driver.write_response(&driver->adapter, req,
                                                      NEU_ERR_TAG_NOT_EXIST);
        utarray_foreach(tags, neu_plugin_tag_value_t *, tv)
        {
            neu_tag_free(tv->tag);
            neu_dvalue_t dvalue = { .type = tv->type, .value = tv->value };
            neu_free_dvalue(&dvalue);
        }
        utarray_free(tags);
        return NEU_ERR_TAG_NOT_EXIST;
    }

    to_be_write_tag_t wtag = { 0 };
    wtag.single            = false;
    wtag.req               = (void *) req;
    wtag.tvs               = tags;

    store_write_tag(g, &wtag);

    return NEU_ERR_SUCCESS;
}

int neu_adapter_driver_write_gtags(neu_adapter_driver_t *driver,
                                   neu_reqresp_head_t *  req)
{
    neu_req_write_gtags_t *cmd     = (neu_req_write_gtags_t *) &req[1];
    group_t *              first_g = NULL;

    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        driver->adapter.cb_funs.driver.write_response(
            &driver->adapter, req, NEU_ERR_PLUGIN_NOT_RUNNING);
        return NEU_ERR_PLUGIN_NOT_RUNNING;
    }

    if (driver->adapter.module->intf_funs->driver.write_tags == NULL) {
        driver->adapter.cb_funs.driver.write_response(
            &driver->adapter, req, NEU_ERR_PLUGIN_NOT_SUPPORT_WRITE_TAGS);
        return NEU_ERR_PLUGIN_NOT_SUPPORT_WRITE_TAGS;
    }

    for (int i = 0; i < cmd->n_group; i++) {
        group_t *g = find_group(driver, cmd->groups[i].group);

        if (g == NULL) {
            driver->adapter.cb_funs.driver.write_response(
                &driver->adapter, req, NEU_ERR_GROUP_NOT_EXIST);
            return NEU_ERR_GROUP_NOT_EXIST;
        }

        if (first_g == NULL) {
            first_g = g;
        }
    }

    UT_array *tags = NULL;
    UT_icd    icd  = { sizeof(neu_plugin_tag_value_t), NULL, NULL, NULL };
    utarray_new(tags, &icd);
    int value_err = NEU_ERR_SUCCESS;
    int value_check;

    for (int i = 0; i < cmd->n_group; i++) {
        group_t *g = find_group(driver, cmd->groups[i].group);

        for (int k = 0; k < cmd->groups[i].n_tag; k++) {
            neu_datatag_t *tag =
                neu_group_find_tag(g->group, cmd->groups[i].tags[k].tag);

            if (tag != NULL) {
                value_check = is_value_in_range(
                    tag->type, cmd->groups[i].tags[k].value.value.i64,
                    cmd->groups[i].tags[k].value.value.d64,
                    cmd->groups[i].tags[k].value.type, tag->decimal);
            } else {
                value_check = NEU_ERR_TAG_NOT_EXIST;
            }

            if (tag != NULL &&
                neu_tag_attribute_test(tag, NEU_ATTRIBUTE_WRITE) &&
                value_check == NEU_ERR_SUCCESS) {
                if (tag->type == NEU_TYPE_FLOAT ||
                    tag->type == NEU_TYPE_DOUBLE) {
                    if (cmd->groups[i].tags[k].value.type == NEU_TYPE_INT64) {
                        cmd->groups[i].tags[k].value.value.d64 =
                            (double) cmd->groups[i].tags[k].value.value.i64;
                    }
                }

                if (tag->decimal != 0) {
                    cal_decimal(tag->type, cmd->groups[i].tags[k].value.type,
                                &cmd->groups[i].tags[k].value.value,
                                tag->decimal);
                }
                fix_value(tag, cmd->groups[i].tags[k].value.type,
                          &cmd->groups[i].tags[k].value);

                neu_plugin_tag_value_t tv = {
                    .tag   = neu_tag_dup(tag),
                    .value = cmd->groups[i].tags[k].value.value,
                };
                utarray_push_back(tags, &tv);
            } else {
                value_err = value_check;
            }
            if (tag != NULL) {
                neu_tag_free(tag);
            }
        }
    }

    if (value_err != NEU_ERR_SUCCESS) {
        driver->adapter.cb_funs.driver.write_response(&driver->adapter, req,
                                                      value_err);
        utarray_foreach(tags, neu_plugin_tag_value_t *, tv)
        {
            neu_tag_free(tv->tag);
        }
        utarray_free(tags);
        return value_err;
    }

    uint32_t n_tag = 0;
    for (int i = 0; i < cmd->n_group; i++) {
        n_tag += cmd->groups[i].n_tag;
    }

    if (utarray_len(tags) != (unsigned int) n_tag) {
        driver->adapter.cb_funs.driver.write_response(&driver->adapter, req,
                                                      NEU_ERR_TAG_NOT_EXIST);
        utarray_foreach(tags, neu_plugin_tag_value_t *, tv)
        {
            neu_tag_free(tv->tag);
        }
        utarray_free(tags);
        return NEU_ERR_TAG_NOT_EXIST;
    }

    to_be_write_tag_t wtag = { 0 };
    wtag.single            = false;
    wtag.req               = (void *) req;
    wtag.tvs               = tags;

    store_write_tag(first_g, &wtag);

    return NEU_ERR_SUCCESS;
}

int neu_adapter_driver_write_tag(neu_adapter_driver_t *driver,
                                 neu_reqresp_head_t *  req)
{
    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        driver->adapter.cb_funs.driver.write_response(
            &driver->adapter, req, NEU_ERR_PLUGIN_NOT_RUNNING);
        return NEU_ERR_PLUGIN_NOT_RUNNING;
    }

    neu_req_write_tag_t *cmd = (neu_req_write_tag_t *) &req[1];
    group_t *            g   = find_group(driver, cmd->group);

    if (g == NULL) {
        driver->adapter.cb_funs.driver.write_response(&driver->adapter, req,
                                                      NEU_ERR_GROUP_NOT_EXIST);
        return NEU_ERR_GROUP_NOT_EXIST;
    }
    neu_datatag_t *tag = neu_group_find_tag(g->group, cmd->tag);

    if (tag == NULL) {
        driver->adapter.cb_funs.driver.write_response(&driver->adapter, req,
                                                      NEU_ERR_TAG_NOT_EXIST);
        return NEU_ERR_TAG_NOT_EXIST;
    } else {
        if ((tag->attribute & NEU_ATTRIBUTE_WRITE) != NEU_ATTRIBUTE_WRITE) {
            driver->adapter.cb_funs.driver.write_response(
                &driver->adapter, req, NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE);
            neu_tag_free(tag);
            return NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE;
        }

        int value_check = is_value_in_range(tag->type, cmd->value.value.i64,
                                            cmd->value.value.d64,
                                            cmd->value.type, tag->decimal);

        if (value_check != NEU_ERR_SUCCESS) {
            driver->adapter.cb_funs.driver.write_response(&driver->adapter, req,
                                                          value_check);
            neu_tag_free(tag);
            return value_check;
        }

        if (tag->type == NEU_TYPE_FLOAT || tag->type == NEU_TYPE_DOUBLE) {
            if (cmd->value.type == NEU_TYPE_INT64) {
                cmd->value.value.d64 = (double) cmd->value.value.i64;
            }
        }

        if (tag->decimal != 0) {
            cal_decimal(tag->type, cmd->value.type, &cmd->value.value,
                        tag->decimal);
        }

        fix_value(tag, cmd->value.type, &cmd->value);

        to_be_write_tag_t wtag = { 0 };
        wtag.single            = true;
        wtag.req               = (void *) req;
        wtag.value             = cmd->value.value;
        wtag.tag               = neu_tag_dup(tag);

        store_write_tag(g, &wtag);

        neu_tag_free(tag);
        return NEU_ERR_SUCCESS;
    }
}

#define REGISTER_GROUP_METRIC(adapter, group, name, init)                  \
    neu_adapter_register_group_metric((adapter), group, name, name##_HELP, \
                                      name##_TYPE, (init))

int neu_adapter_driver_add_group(neu_adapter_driver_t *driver, const char *name,
                                 uint32_t interval, void *context)
{
    UT_icd   icd     = { sizeof(to_be_write_tag_t), NULL, NULL, NULL };
    UT_icd   sub_icd = { sizeof(sub_app_t), NULL, NULL, NULL };
    group_t *find    = NULL;
    int      ret     = NEU_ERR_GROUP_EXIST;

    HASH_FIND_STR(driver->groups, name, find);
    if (find == NULL) {
        find = calloc(1, sizeof(group_t));

        pthread_mutex_init(&find->wt_mtx, NULL);
        pthread_mutex_init(&find->apps_mtx, NULL);

        utarray_new(find->wt_tags, &icd);
        utarray_new(find->apps, &sub_icd);

        find->driver         = driver;
        find->name           = strdup(name);
        find->group          = neu_group_new(name, interval);
        find->grp.group_name = strdup(name);
        find->grp.interval   = interval;
        find->grp.context    = context;
        find->grp.tags       = neu_group_get_tag(find->group);

        if (NEU_NODE_RUNNING_STATE_RUNNING == driver->adapter.state) {
            start_group_timer(driver, find);
        }

        REGISTER_GROUP_METRIC(&driver->adapter, find->name,
                              NEU_METRIC_GROUP_TAGS_TOTAL,
                              neu_group_tag_size(find->group));
        REGISTER_GROUP_METRIC(&driver->adapter, find->name,
                              NEU_METRIC_GROUP_LAST_SEND_MSGS, 0);
        REGISTER_GROUP_METRIC(&driver->adapter, find->name,
                              NEU_METRIC_GROUP_LAST_TIMER_MS, 0);
        REGISTER_GROUP_METRIC(&driver->adapter, find->name,
                              NEU_METRIC_GROUP_LAST_ERROR_CODE, 0);
        REGISTER_GROUP_METRIC(&driver->adapter, find->name,
                              NEU_METRIC_GROUP_LAST_ERROR_TS, 0);

        HASH_ADD_STR(driver->groups, name, find);
        ret = NEU_ERR_SUCCESS;
    }

    return ret;
}

int neu_adapter_driver_update_group(neu_adapter_driver_t *driver,
                                    const char *name, const char *new_name,
                                    uint32_t interval)
{
    int      ret  = 0;
    group_t *find = NULL;

    HASH_FIND_STR(driver->groups, name, find);
    if (NULL == find) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    if (NULL != new_name) {
        if (0 == strlen(new_name)) {
            return NEU_ERR_EINTERNAL;
        }
        if (0 != strcmp(name, new_name)) {
            group_t *other = NULL;
            HASH_FIND_STR(driver->groups, new_name, other);
            if (NULL != other) {
                return NEU_ERR_GROUP_EXIST;
            }
        }
    } else if (neu_group_get_interval(find->group) == interval) {
        return 0; // no change
    }

    // stop the timer first to avoid race condition
    if (NEU_NODE_RUNNING_STATE_RUNNING == driver->adapter.state) {
        stop_group_timer(driver, find);
    }

    // a diminutive value should keep the interval untouched
    if (interval < NEU_GROUP_INTERVAL_LIMIT) {
        interval = neu_group_get_interval(find->group);
    }

    // update group name if a different name is provided
    if (NULL != new_name && 0 != strcmp(name, new_name)) {
        char *new_name_cp1 = strdup(new_name);
        char *new_name_cp2 = strdup(new_name);
        if (new_name_cp1 && new_name_cp2 &&
            0 == neu_group_set_name(find->group, new_name)) {
            HASH_DEL(driver->groups, find);
            free(find->name);
            find->name = new_name_cp1;
            free(find->grp.group_name);
            find->grp.group_name = new_name_cp2;
            neu_adapter_metric_update_group_name((neu_adapter_t *) driver, name,
                                                 new_name);
            HASH_ADD_STR(driver->groups, name, find);
        } else {
            free(new_name_cp1);
            free(new_name_cp2);
            ret = NEU_ERR_EINTERNAL;
        }
    }

    find->timestamp    = global_timestamp; // trigger group_change
    find->grp.interval = interval;
    neu_group_set_interval(find->group, interval);

    // restore the timers
    if (NEU_NODE_RUNNING_STATE_RUNNING == driver->adapter.state) {
        start_group_timer(driver, find);
    }

    return ret;
}

int neu_adapter_driver_del_group(neu_adapter_driver_t *driver, const char *name)
{
    group_t *find = NULL;
    int      ret  = NEU_ERR_GROUP_NOT_EXIST;

    HASH_FIND_STR(driver->groups, name, find);
    if (find != NULL) {
        HASH_DEL(driver->groups, find);

        neu_adapter_driver_try_del_tag(driver, neu_group_tag_size(find->group));

        if (NEU_NODE_RUNNING_STATE_RUNNING == driver->adapter.state) {
            stop_group_timer(driver, find);
        }

        if (find->grp.group_free != NULL) {
            find->grp.group_free(&find->grp);
        }
        free(find->grp.group_name);
        free(find->name);
        if (find->grp.context) {
            free(find->grp.context);
        }

        utarray_foreach(find->grp.tags, neu_datatag_t *, tag)
        {
            neu_driver_cache_del(driver->cache, name, tag->name);
        }

        utarray_foreach(find->wt_tags, to_be_write_tag_t *, tag)
        {
            if (tag->single) {
                neu_tag_free(tag->tag);
            } else {
                utarray_foreach(tag->tvs, neu_plugin_tag_value_t *, tv)
                {
                    neu_tag_free(tv->tag);
                }
                utarray_free(tag->tvs);
            }
        }

        driver->tag_cnt -= neu_group_tag_size(find->group);
        driver->adapter.cb_funs.update_metric(
            &driver->adapter, NEU_METRIC_TAGS_TOTAL, driver->tag_cnt, NULL);

        utarray_free(find->grp.tags);
        utarray_free(find->wt_tags);
        utarray_free(find->apps);
        neu_group_destroy(find->group);
        pthread_mutex_destroy(&find->wt_mtx);
        pthread_mutex_destroy(&find->apps_mtx);
        free(find);

        neu_adapter_del_group_metrics(&driver->adapter, name);
        ret = NEU_ERR_SUCCESS;
    }

    return ret;
}

uint16_t neu_adapter_driver_group_count(neu_adapter_driver_t *driver)
{
    u_int16_t num_groups = 0;
    if (driver && driver->groups) {
        num_groups = HASH_COUNT(driver->groups);
    }
    return num_groups;
}

uint16_t neu_adapter_driver_new_group_count(neu_adapter_driver_t *driver,
                                            neu_req_add_gtag_t *  cmd)
{
    uint16_t new_groups_count = 0;
    for (int i = 0; i < cmd->n_group; i++) {
        group_t *group_in_driver;
        HASH_FIND_STR(driver->groups, cmd->groups[i].group, group_in_driver);
        if (!group_in_driver) {
            new_groups_count++;
        }
    }
    return new_groups_count;
}

static group_t *find_group(neu_adapter_driver_t *driver, const char *name)
{
    group_t *find = NULL;

    HASH_FIND_STR(driver->groups, name, find);

    return find;
}
int neu_adapter_driver_group_exist(neu_adapter_driver_t *driver,
                                   const char *          name)
{
    group_t *find = NULL;
    int      ret  = NEU_ERR_GROUP_NOT_EXIST;

    HASH_FIND_STR(driver->groups, name, find);
    if (find != NULL) {
        ret = NEU_ERR_SUCCESS;
    }

    return ret;
}

UT_array *neu_adapter_driver_get_group(neu_adapter_driver_t *driver)
{
    group_t * el = NULL, *tmp = NULL;
    UT_array *groups = NULL;
    UT_icd    icd    = { sizeof(neu_resp_group_info_t), NULL, NULL, NULL };

    utarray_new(groups, &icd);

    HASH_ITER(hh, driver->groups, el, tmp)
    {
        neu_resp_group_info_t info = { 0 };

        info.interval  = neu_group_get_interval(el->group);
        info.tag_count = neu_group_tag_size(el->group);
        strncpy(info.name, el->name, sizeof(info.name));

        utarray_push_back(groups, &info);
    }

    return groups;
}

int neu_adapter_driver_try_add_tag(neu_adapter_driver_t *driver,
                                   const char *group, neu_datatag_t *tags,
                                   int n_tag)
{
    int ret = 0;
    if (driver->adapter.module->intf_funs->driver.add_tags != NULL) {
        ret = driver->adapter.module->intf_funs->driver.add_tags(
            driver->adapter.plugin, group, tags, n_tag);
    }
    return ret;
}

int neu_adapter_driver_load_tag(neu_adapter_driver_t *driver, const char *group,
                                neu_datatag_t *tags, int n_tag)
{
    int ret = 0;
    if (driver->adapter.module->intf_funs->driver.load_tags != NULL) {
        ret = driver->adapter.module->intf_funs->driver.load_tags(
            driver->adapter.plugin, group, tags, n_tag);
    }
    return ret;
}

int neu_adapter_driver_try_del_tag(neu_adapter_driver_t *driver, int n_tag)
{
    int ret = 0;
    if (driver->adapter.module->intf_funs->driver.del_tags != NULL) {
        ret = driver->adapter.module->intf_funs->driver.del_tags(
            driver->adapter.plugin, n_tag);
    }
    return ret;
}

int neu_adapter_driver_validate_tag(neu_adapter_driver_t *driver,
                                    const char *group, neu_datatag_t *tag)
{
    (void) group;

    if (strlen(tag->name) >= NEU_TAG_NAME_LEN) {
        return NEU_ERR_TAG_NAME_TOO_LONG;
    }

    if (strlen(tag->address) >= NEU_TAG_ADDRESS_LEN) {
        return NEU_ERR_TAG_ADDRESS_TOO_LONG;
    }

    if (strlen(tag->description) >= NEU_TAG_DESCRIPTION_LEN) {
        return NEU_ERR_TAG_DESCRIPTION_TOO_LONG;
    }

    if (tag->precision > NEU_TAG_FLOAG_PRECISION_MAX) {
        return NEU_ERR_TAG_PRECISION_INVALID;
    }

    if (tag->bias != 0) {
        switch (tag->type) {
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_DOUBLE:
            if (tag->bias < -1000 || 1000 < tag->bias ||
                neu_tag_attribute_test(tag, NEU_ATTRIBUTE_WRITE)) {
                return NEU_ERR_TAG_BIAS_INVALID;
            }
            break;
        default:
            return NEU_ERR_TAG_BIAS_INVALID;
        }
    }

    int ret = driver->adapter.module->intf_funs->driver.validate_tag(
        driver->adapter.plugin, tag);
    if (ret != NEU_ERR_SUCCESS) {
        return ret;
    }

    neu_datatag_parse_addr_option(tag, &tag->option);

    return NEU_ERR_SUCCESS;
}

int neu_adapter_driver_add_tag(neu_adapter_driver_t *driver, const char *group,
                               neu_datatag_t *tag, uint16_t interval)
{
    int      ret  = NEU_ERR_SUCCESS;
    group_t *find = NULL;

    neu_datatag_parse_addr_option(tag, &tag->option);
    driver->adapter.module->intf_funs->driver.validate_tag(
        driver->adapter.plugin, tag);

    HASH_FIND_STR(driver->groups, group, find);
    if (find == NULL) {
        neu_adapter_driver_add_group(driver, group, interval, NULL);
        adapter_storage_add_group(driver->adapter.name, group, interval, NULL);
    }
    HASH_FIND_STR(driver->groups, group, find);
    assert(find != NULL);
    ret = neu_group_add_tag(find->group, tag);

    if (ret == NEU_ERR_SUCCESS) {
        driver->tag_cnt += 1;
        driver->adapter.cb_funs.update_metric(
            &driver->adapter, NEU_METRIC_TAGS_TOTAL, driver->tag_cnt, NULL);
        neu_adapter_update_group_metric(&driver->adapter, group,
                                        NEU_METRIC_GROUP_TAGS_TOTAL,
                                        neu_group_tag_size(find->group));
    }

    return ret;
}

int neu_adapter_driver_del_tag(neu_adapter_driver_t *driver, const char *group,
                               const char *tag)
{
    int      ret  = NEU_ERR_SUCCESS;
    group_t *find = NULL;

    HASH_FIND_STR(driver->groups, group, find);
    if (find != NULL) {
        ret = neu_group_del_tag(find->group, tag);
    } else {
        ret = NEU_ERR_GROUP_NOT_EXIST;
    }

    if (ret == NEU_ERR_SUCCESS) {
        neu_adapter_driver_try_del_tag(driver, 1);
        driver->tag_cnt -= 1;
        driver->adapter.cb_funs.update_metric(
            &driver->adapter, NEU_METRIC_TAGS_TOTAL, driver->tag_cnt, NULL);
        neu_adapter_update_group_metric(&driver->adapter, group,
                                        NEU_METRIC_GROUP_TAGS_TOTAL,
                                        neu_group_tag_size(find->group));
    }

    return ret;
}

int neu_adapter_driver_update_tag(neu_adapter_driver_t *driver,
                                  const char *group, neu_datatag_t *tag)
{
    int      ret  = NEU_ERR_SUCCESS;
    group_t *find = NULL;

    if (strlen(tag->name) >= NEU_TAG_NAME_LEN) {
        return NEU_ERR_TAG_NAME_TOO_LONG;
    }

    if (strlen(tag->address) > NEU_TAG_ADDRESS_LEN) {
        return NEU_ERR_TAG_ADDRESS_TOO_LONG;
    }

    if (strlen(tag->description) > NEU_TAG_DESCRIPTION_LEN) {
        return NEU_ERR_TAG_DESCRIPTION_TOO_LONG;
    }

    if (tag->precision > NEU_TAG_FLOAG_PRECISION_MAX) {
        return NEU_ERR_TAG_PRECISION_INVALID;
    }

    ret = driver->adapter.module->intf_funs->driver.validate_tag(
        driver->adapter.plugin, tag);
    if (ret != NEU_ERR_SUCCESS) {
        return ret;
    }

    neu_datatag_parse_addr_option(tag, &tag->option);
    HASH_FIND_STR(driver->groups, group, find);
    if (find != NULL) {
        ret = neu_group_update_tag(find->group, tag);
    } else {
        ret = NEU_ERR_GROUP_NOT_EXIST;
    }

    return ret;
}

int neu_adapter_driver_get_tag(neu_adapter_driver_t *driver, const char *group,
                               UT_array **tags)
{
    int      ret  = NEU_ERR_SUCCESS;
    group_t *find = NULL;

    HASH_FIND_STR(driver->groups, group, find);
    if (find != NULL) {
        *tags = neu_group_get_tag(find->group);
    } else {
        ret = NEU_ERR_GROUP_NOT_EXIST;
    }

    return ret;
}

int neu_adapter_driver_query_tag(neu_adapter_driver_t *driver,
                                 const char *group, const char *name,
                                 UT_array **tags)
{
    int      ret  = NEU_ERR_SUCCESS;
    group_t *find = NULL;

    HASH_FIND_STR(driver->groups, group, find);
    if (find != NULL) {
        if (strlen(name) > 0) {
            *tags = neu_group_query_tag(find->group, name);
        } else {
            *tags = neu_group_get_tag(find->group);
        }
    } else {
        ret = NEU_ERR_GROUP_NOT_EXIST;
    }

    return ret;
}

void neu_adapter_driver_get_value_tag(neu_adapter_driver_t *driver,
                                      const char *group, UT_array **tags)
{
    int ret = neu_adapter_driver_get_tag(driver, group, tags);
    if (ret == NEU_ERR_SUCCESS) {
        utarray_foreach(*tags, neu_datatag_t *, tag)
        {
            if (tag->decimal != 0) {
                if (tag->type == NEU_TYPE_UINT8 || tag->type == NEU_TYPE_INT8 ||
                    tag->type == NEU_TYPE_INT16 ||
                    tag->type == NEU_TYPE_UINT16 ||
                    tag->type == NEU_TYPE_INT32 ||
                    tag->type == NEU_TYPE_UINT32 ||
                    tag->type == NEU_TYPE_INT64 ||
                    tag->type == NEU_TYPE_UINT64 ||
                    tag->type == NEU_TYPE_FLOAT) {
                    tag->type = NEU_TYPE_DOUBLE;
                }
            }
        }
    } else {
        utarray_new(*tags, neu_tag_get_icd());
    }
}

UT_array *neu_adapter_driver_get_read_tag(neu_adapter_driver_t *driver,
                                          const char *          group)
{
    group_t * find = NULL;
    UT_array *tags = NULL;

    HASH_FIND_STR(driver->groups, group, find);
    if (find != NULL) {
        tags = neu_group_get_read_tag(find->group);
    }

    return tags;
}

UT_array *neu_adapter_driver_get_ptag(neu_adapter_driver_t *driver,
                                      const char *group, const char *tag)
{
    group_t * find = NULL;
    UT_array *tags = NULL;

    HASH_FIND_STR(driver->groups, group, find);
    if (find != NULL) {
        neu_datatag_t *t = neu_group_find_tag(find->group, tag);
        if (t != NULL) {
            utarray_new(tags, neu_tag_get_icd());
            utarray_push_back(tags, t);
            neu_tag_free(t);
        }
    }

    return tags;
}

static void report_to_app(neu_adapter_driver_t *driver, group_t *group,
                          struct sockaddr_un dst)
{
    neu_reqresp_head_t header = {
        .type = NEU_REQRESP_TRANS_DATA,
    };

    UT_array *tags =
        neu_adapter_driver_get_read_tag(group->driver, group->name);

    neu_reqresp_trans_data_t *data =
        calloc(1, sizeof(neu_reqresp_trans_data_t));

    data->driver = strdup(group->driver->adapter.name);
    data->group  = strdup(group->name);
    utarray_new(data->tags, neu_resp_tag_value_meta_icd());

    read_group(global_timestamp,
               neu_group_get_interval(group->group) *
                   NEU_DRIVER_TAG_CACHE_EXPIRE_TIME,
               neu_adapter_get_tag_cache_type(&driver->adapter), driver->cache,
               group->name, tags, data->tags);

    nlog_info("report group: %s, all tags: %d, report tags: %d", group->name,
              utarray_len(tags), utarray_len(data->tags));
    if (utarray_len(data->tags) > 0) {
        pthread_mutex_lock(&group->apps_mtx);

        data->ctx        = calloc(1, sizeof(neu_reqresp_trans_data_ctx_t));
        data->ctx->index = 1;

        pthread_mutex_init(&data->ctx->mtx, NULL);

        if (driver->adapter.cb_funs.responseto(&driver->adapter, &header, data,
                                               dst) != 0) {
            neu_trans_data_free(data);
        }

        pthread_mutex_unlock(&group->apps_mtx);
    } else {
        utarray_free(data->tags);
        free(data->group);
        free(data->driver);
    }
    utarray_free(tags);
    free(data);
}

static int report_callback(void *usr_data)
{
    group_t *                group = (group_t *) usr_data;
    neu_node_running_state_e state = group->driver->adapter.state;
    if (state != NEU_NODE_RUNNING_STATE_RUNNING) {
        return 0;
    }

    neu_reqresp_head_t header = {
        .type = NEU_REQRESP_TRANS_DATA,
    };

    UT_array *tags =
        neu_adapter_driver_get_read_tag(group->driver, group->name);

    neu_reqresp_trans_data_t *data =
        calloc(1, sizeof(neu_reqresp_trans_data_t));

    data->driver = strdup(group->driver->adapter.name);
    data->group  = strdup(group->name);
    utarray_new(data->tags, neu_resp_tag_value_meta_icd());
    utarray_reserve(data->tags, utarray_len(tags));

    void *trace_ctx =
        neu_driver_cache_get_trace(group->driver->cache, group->name);

    neu_otel_trace_ctx trans_trace = NULL;
    neu_otel_scope_ctx trans_scope = NULL;
    if (neu_otel_data_is_started() && trace_ctx) {
        trans_trace = neu_otel_find_trace2(trace_ctx);
        if (trans_trace) {
            data->trace_ctx      = trace_ctx;
            char new_span_id[36] = { 0 };
            neu_otel_new_span_id(new_span_id);
            trans_scope =
                neu_otel_add_span2(trans_trace, "report cb", new_span_id);
            neu_otel_scope_add_span_attr_int(trans_scope, "thread id",
                                             (int64_t)(pthread_self()));
            neu_otel_scope_set_span_start_time(trans_scope, neu_time_ns());
        }
    }

    read_report_group(global_timestamp,
                      neu_group_get_interval(group->group) *
                          NEU_DRIVER_TAG_CACHE_EXPIRE_TIME,
                      neu_adapter_get_tag_cache_type(&group->driver->adapter),
                      group->driver->cache, group->name, tags, data->tags);

    if (utarray_len(data->tags) > 0) {
        pthread_mutex_lock(&group->apps_mtx);

        if (utarray_len(group->apps) > 0) {
            int app_num      = 0;
            data->ctx        = calloc(1, sizeof(neu_reqresp_trans_data_ctx_t));
            data->ctx->index = utarray_len(group->apps);
            pthread_mutex_init(&data->ctx->mtx, NULL);

            for (uint16_t i = 0; i < utarray_len(group->apps) - 1; i++) {
                utarray_foreach(data->tags, neu_resp_tag_value_meta_t *,
                                tag_value)
                {
                    if (tag_value->value.type == NEU_TYPE_CUSTOM) {
                        json_incref(tag_value->value.value.json);
                    }
                }
            }

            if (trans_trace) {
                neu_otel_set_internal_parent_span(trans_trace);
            }

            utarray_foreach(group->apps, sub_app_t *, app)
            {
                if (group->driver->adapter.cb_funs.responseto(
                        &group->driver->adapter, &header, data, app->addr) !=
                    0) {
                    utarray_foreach(data->tags, neu_resp_tag_value_meta_t *,
                                    tag_value)
                    {
                        if (tag_value->value.type == NEU_TYPE_CUSTOM) {
                            json_decref(tag_value->value.value.json);
                        }
                    }
                    neu_trans_data_free(data);
                    if (trans_trace) {
                        neu_otel_scope_add_span_attr_int(trans_scope, app->app,
                                                         0);
                    }
                } else {
                    app_num += 1;
                    if (trans_trace) {
                        neu_otel_scope_add_span_attr_int(trans_scope, app->app,
                                                         1);
                    }
                }
            }

            if (trans_trace) {
                neu_otel_scope_set_span_end_time(trans_scope, neu_time_ns());
                neu_otel_trace_set_expected_span_num(trans_trace, app_num);
                if (app_num == 0) {
                    neu_otel_trace_set_final(trans_trace);
                }
            }

        } else {
            utarray_foreach(data->tags, neu_resp_tag_value_meta_t *, tag_value)
            {
                if (tag_value->value.type == NEU_TYPE_PTR) {
                    free(tag_value->value.value.ptr.ptr);
                } else {
                    neu_free_dvalue(&tag_value->value);
                }
            }
            utarray_free(data->tags);
            free(data->group);
            free(data->driver);

            if (trans_trace) {
                neu_otel_scope_add_span_attr_int(trans_scope, "no sub app", 1);
                neu_otel_scope_set_span_end_time(trans_scope, neu_time_ns());
                neu_otel_trace_set_final(trans_trace);
            }
        }

        pthread_mutex_unlock(&group->apps_mtx);
    } else {
        utarray_free(data->tags);
        free(data->group);
        free(data->driver);
        if (trans_trace) {
            neu_otel_scope_add_span_attr_int(trans_scope, "no tags", 1);
            neu_otel_scope_set_span_end_time(trans_scope, neu_time_ns());
            neu_otel_trace_set_final(trans_trace);
        }
    }
    utarray_free(tags);
    free(data);
    return 0;
}

static void group_change(void *arg, int64_t timestamp, UT_array *tags,
                         uint32_t interval)
{
    group_t *group   = (group_t *) arg;
    group->timestamp = timestamp;
    (void) interval;

    if (group->grp.group_free != NULL)
        group->grp.group_free(&group->grp);

    utarray_foreach(group->grp.tags, neu_datatag_t *, tag)
    {
        neu_driver_cache_del(group->driver->cache, group->name, tag->name);
    }

    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        neu_dvalue_t value = { 0 };

        value.precision = tag->precision;
        value.type      = NEU_TYPE_ERROR;
        value.value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY;

        neu_driver_cache_add(group->driver->cache, group->name, tag->name,
                             value);
    }

    neu_plugin_group_t grp = {
        .group_name = strdup(group->name),
        .interval   = neu_group_get_interval(group->group),
        .tags       = NULL,
        .group_free = NULL,
        .user_data  = NULL,
        .context    = NULL,
    };

    grp.context = group->grp.context;
    grp.tags    = tags;
    free(group->grp.group_name);
    if (group->grp.tags != NULL) {
        utarray_free(group->grp.tags);
    }

    group->grp = grp;

    nlog_notice("group: %s changed, timestamp: %" PRIi64, group->name,
                timestamp);
}

static int write_callback(void *usr_data)
{
    group_t *                group = (group_t *) usr_data;
    neu_node_running_state_e state = group->driver->adapter.state;
    if (state != NEU_NODE_RUNNING_STATE_RUNNING) {
        return 0;
    }

    pthread_mutex_lock(&group->wt_mtx);
    utarray_foreach(group->wt_tags, to_be_write_tag_t *, wtag)
    {

        int64_t s_time = 0;
        int64_t e_time = 0;

        neu_otel_trace_ctx trace = NULL;
        neu_otel_scope_ctx scope = NULL;
        if (neu_otel_control_is_started()) {
            trace =
                neu_otel_find_trace(((neu_reqresp_head_t *) wtag->req)->ctx);
            if (trace) {
                scope = neu_otel_add_span(trace);
                if (wtag->single) {
                    neu_otel_scope_set_span_name(scope,
                                                 "driver timer cb write tag");
                } else {
                    neu_otel_scope_set_span_name(scope,
                                                 "driver timer cb write tags");
                }
                char new_span_id[36] = { 0 };
                neu_otel_new_span_id(new_span_id);
                neu_otel_scope_set_span_id(scope, new_span_id);
                uint8_t *p_sp_id = neu_otel_scope_get_pre_span_id(scope);
                if (p_sp_id) {
                    neu_otel_scope_set_parent_span_id2(scope, p_sp_id, 8);
                }
                neu_otel_scope_add_span_attr_int(scope, "thread id",
                                                 (int64_t) pthread_self());
                neu_otel_scope_add_span_attr_string(
                    scope, "plugin name",
                    group->driver->adapter.module->module_name);

                char version[64] = { 0 };
                sprintf(version, "%d.%d.%d",
                        NEU_GET_VERSION_MAJOR(
                            group->driver->adapter.module->version),
                        NEU_GET_VERSION_MINOR(
                            group->driver->adapter.module->version),
                        NEU_GET_VERSION_FIX(
                            group->driver->adapter.module->version));

                neu_otel_scope_add_span_attr_string(scope, "plugin version",
                                                    version);
            }
        }

        s_time = neu_time_ns();

        if (wtag->single) {
            group->driver->adapter.module->intf_funs->driver.write_tag(
                group->driver->adapter.plugin, (void *) wtag->req, wtag->tag,
                wtag->value);
            e_time = neu_time_ns();
            neu_tag_free(wtag->tag);
        } else {
            group->driver->adapter.module->intf_funs->driver.write_tags(
                group->driver->adapter.plugin, (void *) wtag->req, wtag->tvs);
            e_time = neu_time_ms();
            utarray_foreach(wtag->tvs, neu_plugin_tag_value_t *, tv)
            {
                neu_tag_free(tv->tag);
            }
            utarray_free(wtag->tvs);
        }
        if (neu_otel_control_is_started() && trace) {
            neu_otel_scope_set_span_start_time(scope, s_time);
            neu_otel_scope_set_span_end_time(scope, e_time);
        }
    }
    utarray_clear(group->wt_tags);
    pthread_mutex_unlock(&group->wt_mtx);

    return 0;
}

static int read_callback(void *usr_data)
{
    group_t *                group = (group_t *) usr_data;
    neu_node_running_state_e state = group->driver->adapter.state;
    if (state != NEU_NODE_RUNNING_STATE_RUNNING) {
        return 0;
    }

    if (neu_group_is_change(group->group, group->timestamp)) {
        neu_group_change_test(group->group, group->timestamp, (void *) group,
                              group_change);
    }

    if (group->grp.tags != NULL && utarray_len(group->grp.tags) > 0) {
        int64_t spend = global_timestamp;

        group->driver->adapter.module->intf_funs->driver.group_timer(
            group->driver->adapter.plugin, &group->grp);

        spend = global_timestamp - spend;
        nlog_debug("%s-%s timer: %" PRId64, group->driver->adapter.name,
                   group->name, spend);

        neu_adapter_update_group_metric(&group->driver->adapter, group->name,
                                        NEU_METRIC_GROUP_LAST_TIMER_MS, spend);
    }

    return 0;
}

static void read_report_group(int64_t timestamp, int64_t timeout,
                              neu_tag_cache_type_e cache_type,
                              neu_driver_cache_t *cache, const char *group,
                              UT_array *tags, UT_array *tag_values)
{
    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        neu_driver_cache_value_t  value     = { 0 };
        neu_resp_tag_value_meta_t tag_value = { 0 };

        if (neu_tag_attribute_test(tag, NEU_ATTRIBUTE_SUBSCRIBE)) {
            if (neu_driver_cache_meta_get_changed(cache, group, tag->name,
                                                  &value, tag_value.metas,
                                                  NEU_TAG_META_SIZE) != 0) {
                nlog_debug("tag: %s not changed", tag->name);
                continue;
            }
        } else {
            if (neu_driver_cache_meta_get(cache, group, tag->name, &value,
                                          tag_value.metas,
                                          NEU_TAG_META_SIZE) != 0) {
                strcpy(tag_value.tag, tag->name);
                tag_value.value.type      = NEU_TYPE_ERROR;
                tag_value.value.value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY;

                utarray_push_back(tag_values, &tag_value);
                continue;
            }
        }
        strcpy(tag_value.tag, tag->name);

        tag_value.datatag.bias = tag->bias;

        if (value.value.type == NEU_TYPE_ERROR) {
            tag_value.value = value.value;

            utarray_push_back(tag_values, &tag_value);
            continue;
        }

        if ((tag->type == NEU_TYPE_FLOAT && isnan(value.value.value.f32)) ||
            (tag->type == NEU_TYPE_DOUBLE && isnan(value.value.value.d64))) {
            tag_value.value.type      = NEU_TYPE_ERROR;
            tag_value.value.value.i32 = NEU_ERR_PLUGIN_TAG_VALUE_EXPIRED;
            utarray_push_back(tag_values, &tag_value);
            continue;
        }

        switch (tag->type) {
        case NEU_TYPE_UINT16:
        case NEU_TYPE_INT16:
            switch (tag->option.value16.endian) {
            case NEU_DATATAG_ENDIAN_B16:
                value.value.value.u16 = htons(value.value.value.u16);
                break;
            case NEU_DATATAG_ENDIAN_L16:
            default:
                break;
            }
            break;
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_INT32:
            switch (tag->option.value32.endian) {
            case NEU_DATATAG_ENDIAN_LB32: {
                uint16_t *v1 = (uint16_t *) value.value.value.bytes.bytes;
                uint16_t *v2 = (uint16_t *) (value.value.value.bytes.bytes + 2);

                neu_htons_p(v1);
                neu_htons_p(v2);
                break;
            }
            case NEU_DATATAG_ENDIAN_BB32:
                value.value.value.u32 = htonl(value.value.value.u32);
                break;
            case NEU_DATATAG_ENDIAN_BL32:
                value.value.value.u32 = htonl(value.value.value.u32);
                uint16_t *v1 = (uint16_t *) value.value.value.bytes.bytes;
                uint16_t *v2 = (uint16_t *) (value.value.value.bytes.bytes + 2);

                neu_htons_p(v1);
                neu_htons_p(v2);
                break;
            case NEU_DATATAG_ENDIAN_LL32:
            default:
                break;
            }
            break;
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
            switch (tag->option.value64.endian) {
            case NEU_DATATAG_ENDIAN_B64:
            case NEU_DATATAG_ENDIAN_BB64:
                value.value.value.u64 = neu_htonll(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_LB64:
                value.value.value.u64 = neu_htonlb(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_BL64:
                value.value.value.u64 = neu_htonbl(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_L64:
            case NEU_DATATAG_ENDIAN_LL64:
            default:
                break;
            }
            break;
        default:
            break;
        }

        if (cache_type != NEU_TAG_CACHE_TYPE_NEVER &&
            (timestamp - value.timestamp) > timeout && timeout > 0) {
            if (value.value.type == NEU_TYPE_PTR) {
                free(value.value.value.ptr.ptr);
            } else {
                neu_free_dvalue(&value.value);
            }
            tag_value.value.type      = NEU_TYPE_ERROR;
            tag_value.value.value.i32 = NEU_ERR_PLUGIN_TAG_VALUE_EXPIRED;
        } else {
            if (value.value.type == NEU_TYPE_PTR) {
                tag_value.value.type             = NEU_TYPE_PTR;
                tag_value.value.value.ptr.length = value.value.value.ptr.length;
                tag_value.value.value.ptr.type   = value.value.value.ptr.type;
                tag_value.value.value.ptr.ptr    = value.value.value.ptr.ptr;
            } else {
                tag_value.value = value.value;
            }

            if (tag->decimal != 0 || tag->bias != 0) {
                double decimal = tag->decimal != 0 ? tag->decimal : 1;
                double bias    = tag->bias;

                tag_value.value.type = NEU_TYPE_DOUBLE;
                switch (tag->type) {
                case NEU_TYPE_INT8:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i8 * decimal + bias;
                    break;
                case NEU_TYPE_UINT8:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u8 * decimal + bias;
                    break;
                case NEU_TYPE_INT16:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i16 * decimal + bias;
                    break;
                case NEU_TYPE_UINT16:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u16 * decimal + bias;
                    break;
                case NEU_TYPE_INT32:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i32 * decimal + bias;
                    break;
                case NEU_TYPE_UINT32:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u32 * decimal + bias;
                    break;
                case NEU_TYPE_INT64:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i64 * decimal + bias;
                    break;
                case NEU_TYPE_UINT64:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u64 * decimal + bias;
                    break;
                case NEU_TYPE_FLOAT:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.f32 * decimal + bias;
                    break;
                case NEU_TYPE_DOUBLE:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.d64 * decimal + bias;
                    break;
                default:
                    tag_value.value.type = tag->type;
                    break;
                }
            }
            if (tag->precision == 0 && tag->bias == 0 &&
                tag->type == NEU_TYPE_DOUBLE) {
                format_tag_value(&tag_value.value);
            }
        }

        utarray_push_back(tag_values, &tag_value);
    }
}

static void read_group(int64_t timestamp, int64_t timeout,
                       neu_tag_cache_type_e cache_type,
                       neu_driver_cache_t *cache, const char *group,
                       UT_array *tags, UT_array *tag_values)
{
    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        neu_resp_tag_value_meta_t tag_value = { 0 };
        neu_driver_cache_value_t  value     = { 0 };

        strcpy(tag_value.tag, tag->name);

        tag_value.datatag.bias = tag->bias;

        if (neu_driver_cache_meta_get(cache, group, tag->name, &value,
                                      tag_value.metas,
                                      NEU_TAG_META_SIZE) != 0) {
            tag_value.value.type      = NEU_TYPE_ERROR;
            tag_value.value.value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY;

            utarray_push_back(tag_values, &tag_value);
            continue;
        }

        if (value.value.type == NEU_TYPE_ERROR) {
            tag_value.value = value.value;
            utarray_push_back(tag_values, &tag_value);
            continue;
        }

        switch (tag->type) {
        case NEU_TYPE_UINT16:
        case NEU_TYPE_INT16:
            switch (tag->option.value16.endian) {
            case NEU_DATATAG_ENDIAN_B16:
                value.value.value.u16 = htons(value.value.value.u16);
                break;
            case NEU_DATATAG_ENDIAN_L16:
            default:
                break;
            }
            break;
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_INT32:
            switch (tag->option.value32.endian) {
            case NEU_DATATAG_ENDIAN_LB32: {
                uint16_t *v1 = (uint16_t *) value.value.value.bytes.bytes;
                uint16_t *v2 = (uint16_t *) (value.value.value.bytes.bytes + 2);

                neu_htons_p(v1);
                neu_htons_p(v2);
                break;
            }
            case NEU_DATATAG_ENDIAN_BB32:
                value.value.value.u32 = htonl(value.value.value.u32);
                break;
            case NEU_DATATAG_ENDIAN_BL32:
                value.value.value.u32 = htonl(value.value.value.u32);
                uint16_t *v1 = (uint16_t *) value.value.value.bytes.bytes;
                uint16_t *v2 = (uint16_t *) (value.value.value.bytes.bytes + 2);

                neu_htons_p(v1);
                neu_htons_p(v2);
                break;
            case NEU_DATATAG_ENDIAN_LL32:
            default:
                break;
            }
            break;
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
            switch (tag->option.value64.endian) {
            case NEU_DATATAG_ENDIAN_B64:
            case NEU_DATATAG_ENDIAN_BB64:
                value.value.value.u64 = neu_htonll(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_LB64:
                value.value.value.u64 = neu_htonlb(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_BL64:
                value.value.value.u64 = neu_htonbl(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_L64:
            case NEU_DATATAG_ENDIAN_LL64:
            default:
                break;
            }
            break;
        default:
            break;
        }

        if (cache_type != NEU_TAG_CACHE_TYPE_NEVER &&
            (timestamp - value.timestamp) > timeout) {
            if (value.value.type == NEU_TYPE_PTR) {
                free(value.value.value.ptr.ptr);
            } else {
                neu_free_dvalue(&value.value);
            }
            tag_value.value.type      = NEU_TYPE_ERROR;
            tag_value.value.value.i32 = NEU_ERR_PLUGIN_TAG_VALUE_EXPIRED;
        } else {
            if (value.value.type == NEU_TYPE_PTR) {
                tag_value.value.type             = NEU_TYPE_PTR;
                tag_value.value.value.ptr.length = value.value.value.ptr.length;
                tag_value.value.value.ptr.type   = value.value.value.ptr.type;
                tag_value.value.value.ptr.ptr    = value.value.value.ptr.ptr;
            } else {
                tag_value.value = value.value;
            }
            if (tag->decimal != 0 || tag->bias != 0) {
                tag_value.value.type = NEU_TYPE_DOUBLE;
                double decimal       = tag->decimal != 0 ? tag->decimal : 1;
                double bias          = tag->bias;
                switch (tag->type) {
                case NEU_TYPE_INT8:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i8 * decimal + bias;
                    break;
                case NEU_TYPE_UINT8:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u8 * decimal + bias;
                    break;
                case NEU_TYPE_INT16:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i16 * decimal + bias;
                    break;
                case NEU_TYPE_UINT16:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u16 * decimal + bias;
                    break;
                case NEU_TYPE_INT32:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i32 * decimal + bias;
                    break;
                case NEU_TYPE_UINT32:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u32 * decimal + bias;
                    break;
                case NEU_TYPE_INT64:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i64 * decimal + bias;
                    break;
                case NEU_TYPE_UINT64:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u64 * decimal + bias;
                    break;
                case NEU_TYPE_FLOAT:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.f32 * decimal + bias;
                    break;
                case NEU_TYPE_DOUBLE:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.d64 * decimal + bias;
                    break;
                default:
                    tag_value.value.type = tag->type;
                    break;
                }
            }

            if (tag->precision == 0 && tag->bias == 0 &&
                tag->type == NEU_TYPE_DOUBLE) {
                format_tag_value(&tag_value.value);
            }
        }
        utarray_push_back(tag_values, &tag_value);
    }
}

static void read_group_paginate(int64_t timestamp, int64_t timeout,
                                neu_tag_cache_type_e cache_type,
                                neu_driver_cache_t *cache, const char *group,
                                UT_array *tags, UT_array *tag_values)
{
    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        neu_resp_tag_value_meta_paginate_t tag_value = { 0 };
        neu_driver_cache_value_t           value     = { 0 };

        strcpy(tag_value.tag, tag->name);

        tag_value.datatag.name        = strdup(tag->name);
        tag_value.datatag.address     = strdup(tag->address);
        tag_value.datatag.attribute   = tag->attribute;
        tag_value.datatag.type        = tag->type;
        tag_value.datatag.precision   = tag->precision;
        tag_value.datatag.decimal     = tag->decimal;
        tag_value.datatag.bias        = tag->bias;
        tag_value.datatag.description = strdup(tag->description);
        tag_value.datatag.option      = tag->option;
        memcpy(tag_value.datatag.meta, tag->meta, NEU_TAG_META_LENGTH);

        if (tag->attribute == NEU_ATTRIBUTE_WRITE) {
            tag_value.value.type         = NEU_TYPE_STRING;
            tag_value.value.value.str[0] = 0;
            utarray_push_back(tag_values, &tag_value);
            continue;
        }

        if (neu_driver_cache_meta_get(cache, group, tag->name, &value,
                                      tag_value.metas,
                                      NEU_TAG_META_SIZE) != 0) {
            tag_value.value.type      = NEU_TYPE_ERROR;
            tag_value.value.value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY;

            utarray_push_back(tag_values, &tag_value);
            continue;
        }

        if (value.value.type == NEU_TYPE_ERROR) {
            tag_value.value = value.value;
            utarray_push_back(tag_values, &tag_value);
            continue;
        }

        switch (tag->type) {
        case NEU_TYPE_UINT16:
        case NEU_TYPE_INT16:
            switch (tag->option.value16.endian) {
            case NEU_DATATAG_ENDIAN_B16:
                value.value.value.u16 = htons(value.value.value.u16);
                break;
            case NEU_DATATAG_ENDIAN_L16:
            default:
                break;
            }
            break;
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_INT32:
            switch (tag->option.value32.endian) {
            case NEU_DATATAG_ENDIAN_LB32: {
                uint16_t *v1 = (uint16_t *) value.value.value.bytes.bytes;
                uint16_t *v2 = (uint16_t *) (value.value.value.bytes.bytes + 2);

                neu_htons_p(v1);
                neu_htons_p(v2);
                break;
            }
            case NEU_DATATAG_ENDIAN_BB32:
                value.value.value.u32 = htonl(value.value.value.u32);
                break;
            case NEU_DATATAG_ENDIAN_BL32:
                value.value.value.u32 = htonl(value.value.value.u32);
                uint16_t *v1 = (uint16_t *) value.value.value.bytes.bytes;
                uint16_t *v2 = (uint16_t *) (value.value.value.bytes.bytes + 2);

                neu_htons_p(v1);
                neu_htons_p(v2);
                break;
            case NEU_DATATAG_ENDIAN_LL32:
            default:
                break;
            }
            break;
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
            switch (tag->option.value64.endian) {
            case NEU_DATATAG_ENDIAN_B64:
            case NEU_DATATAG_ENDIAN_BB64:
                value.value.value.u64 = neu_htonll(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_LB64:
                value.value.value.u64 = neu_htonlb(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_BL64:
                value.value.value.u64 = neu_htonbl(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_L64:
            case NEU_DATATAG_ENDIAN_LL64:
            default:
                break;
            }
            break;
        default:
            break;
        }

        if (cache_type != NEU_TAG_CACHE_TYPE_NEVER &&
            (timestamp - value.timestamp) > timeout) {
            if (value.value.type == NEU_TYPE_PTR) {
                free(value.value.value.ptr.ptr);
            } else {
                neu_free_dvalue(&value.value);
            }
            tag_value.value.type      = NEU_TYPE_ERROR;
            tag_value.value.value.i32 = NEU_ERR_PLUGIN_TAG_VALUE_EXPIRED;
        } else {
            if (value.value.type == NEU_TYPE_PTR) {
                tag_value.value.type             = NEU_TYPE_PTR;
                tag_value.value.value.ptr.length = value.value.value.ptr.length;
                tag_value.value.value.ptr.type   = value.value.value.ptr.type;
                tag_value.value.value.ptr.ptr    = value.value.value.ptr.ptr;
            } else {
                tag_value.value = value.value;
            }
            if (tag->decimal != 0 || tag->bias != 0) {
                tag_value.value.type = NEU_TYPE_DOUBLE;
                double decimal       = tag->decimal != 0 ? tag->decimal : 1;
                double bias          = tag->bias;
                switch (tag->type) {
                case NEU_TYPE_INT8:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i8 * decimal + bias;
                    break;
                case NEU_TYPE_UINT8:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u8 * decimal + bias;
                    break;
                case NEU_TYPE_INT16:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i16 * decimal + bias;
                    break;
                case NEU_TYPE_UINT16:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u16 * decimal + bias;
                    break;
                case NEU_TYPE_INT32:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i32 * decimal + bias;
                    break;
                case NEU_TYPE_UINT32:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u32 * decimal + bias;
                    break;
                case NEU_TYPE_INT64:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.i64 * decimal + bias;
                    break;
                case NEU_TYPE_UINT64:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.u64 * decimal + bias;
                    break;
                case NEU_TYPE_FLOAT:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.f32 * decimal + bias;
                    break;
                case NEU_TYPE_DOUBLE:
                    tag_value.value.value.d64 =
                        (double) tag_value.value.value.d64 * decimal + bias;
                    break;
                default:
                    tag_value.value.type = tag->type;
                    break;
                }
            }

            if (tag->precision == 0 && tag->bias == 0 &&
                tag->type == NEU_TYPE_DOUBLE) {
                format_tag_value(&tag_value.value);
            }
        }
        utarray_push_back(tag_values, &tag_value);
    }
}

static void store_write_tag(group_t *group, to_be_write_tag_t *tag)
{
    pthread_mutex_lock(&group->wt_mtx);
    utarray_push_back(group->wt_tags, tag);
    pthread_mutex_unlock(&group->wt_mtx);
}

void neu_adapter_driver_subscribe(neu_adapter_driver_t *driver,
                                  neu_req_subscribe_t * req)
{
    sub_app_t sub_app = { 0 };
    group_t * find    = NULL;

    HASH_FIND_STR(driver->groups, req->group, find);
    if (find == NULL) {
        nlog_warn("%s sub group: %s not exist", driver->adapter.name,
                  req->group);
        return;
    }

    pthread_mutex_lock(&find->apps_mtx);
    utarray_foreach(find->apps, sub_app_t *, app)
    {
        if (strcmp(app->app, req->app) == 0) {
            nlog_warn("%s sub group: %s app: %s already exist",
                      driver->adapter.name, req->group, req->app);
            pthread_mutex_unlock(&find->apps_mtx);
            return;
        }
    }

    strcpy(sub_app.app, req->app);
    sub_app.addr.sun_family = AF_UNIX;
    snprintf(sub_app.addr.sun_path, sizeof(sub_app.addr.sun_path),
             "%cneuron-%" PRIu16, '\0', req->port);

    utarray_push_back(find->apps, &sub_app);
    pthread_mutex_unlock(&find->apps_mtx);

    report_to_app(driver, find, sub_app.addr);
}

void neu_adapter_driver_unsubscribe(neu_adapter_driver_t * driver,
                                    neu_req_unsubscribe_t *req)
{
    group_t *find = NULL;

    HASH_FIND_STR(driver->groups, req->group, find);
    if (find == NULL) {
        nlog_warn("%s unsub group: %s not exist", driver->adapter.name,
                  req->group);
        return;
    }

    pthread_mutex_lock(&find->apps_mtx);
    utarray_foreach(find->apps, sub_app_t *, app)
    {
        if (strcmp(app->app, req->app) == 0) {
            utarray_erase(find->apps, utarray_eltidx(find->apps, app), 1);
            pthread_mutex_unlock(&find->apps_mtx);
            return;
        }
    }
    pthread_mutex_unlock(&find->apps_mtx);
}

void neu_adapter_driver_scan_tags(neu_adapter_driver_t *driver,
                                  neu_reqresp_head_t *  req)
{
    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        neu_resp_scan_tags_t resp_scan = {
            .scan_tags = NULL,
            .error     = NEU_ERR_PLUGIN_NOT_RUNNING,
            .type      = NEU_TYPE_ERROR,
            .is_array  = false,
        };
        driver->adapter.cb_funs.driver.scan_tags_response(&driver->adapter, req,
                                                          &resp_scan);

        nlog_warn("%s not running", driver->adapter.name);
        return;
    }

    if (driver->adapter.module->intf_funs->driver.scan_tags == NULL) {
        neu_resp_scan_tags_t resp_scan = {
            .scan_tags = NULL,
            .error     = NEU_ERR_PLUGIN_NOT_SUPPORT_SCAN_TAGS,
            .type      = NEU_TYPE_ERROR,
            .is_array  = false,
        };
        driver->adapter.cb_funs.driver.scan_tags_response(&driver->adapter, req,
                                                          &resp_scan);

        nlog_warn("%s not support scan tags", driver->adapter.name);
        return;
    }

    neu_req_scan_tags_t *cmd = (neu_req_scan_tags_t *) &req[1];
    driver->adapter.module->intf_funs->driver.scan_tags(driver->adapter.plugin,
                                                        req, cmd->id, cmd->ctx);
}

void neu_adapter_driver_test_read_tag(neu_adapter_driver_t *driver,
                                      neu_reqresp_head_t *  req)
{
    neu_json_value_u error_value;
    error_value.val_int = 0;

    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        driver->adapter.cb_funs.driver.test_read_tag_response(
            &driver->adapter, req, NEU_JSON_INT, NEU_TYPE_ERROR, error_value,
            NEU_ERR_PLUGIN_NOT_RUNNING);

        nlog_warn("%s not running", driver->adapter.name);
        return;
    }

    if (driver->adapter.module->intf_funs->driver.test_read_tag == NULL) {
        driver->adapter.cb_funs.driver.test_read_tag_response(
            &driver->adapter, req, NEU_JSON_INT, NEU_TYPE_ERROR, error_value,
            NEU_ERR_PLUGIN_NOT_SUPPORT_TEST_READ_TAG);

        nlog_warn("%s not support testing reading", driver->adapter.name);
        return;
    }

    neu_req_test_read_tag_t *cmd = (neu_req_test_read_tag_t *) &req[1];
    neu_datatag_t            datatag;
    datatag.name      = cmd->tag;
    datatag.address   = cmd->address;
    datatag.attribute = cmd->attribute;
    datatag.type      = cmd->type;
    datatag.precision = cmd->precision;
    datatag.decimal   = cmd->decimal;
    datatag.bias      = cmd->bias;
    datatag.option    = cmd->option;

    group_t *g = find_group(driver, cmd->group);
    if (g != NULL && neu_group_tag_size(g->group) > 0) {
        stop_group_timer(driver, g);
    }

    driver->adapter.module->intf_funs->driver.test_read_tag(
        driver->adapter.plugin, req, datatag);

    if (g != NULL && neu_group_tag_size(g->group) > 0) {
        start_group_timer(driver, g);
    }
}

int neu_adapter_driver_cmd(neu_adapter_driver_t *driver, const char *cmd)
{
    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        return NEU_ERR_PLUGIN_NOT_RUNNING;
    }
    if (driver->adapter.module->intf_funs->driver.action == NULL) {
        return NEU_ERR_PLUGIN_NOT_SUPPORT_EXE_ACTION;
    }
    return driver->adapter.module->intf_funs->driver.action(
        driver->adapter.plugin, cmd);
}