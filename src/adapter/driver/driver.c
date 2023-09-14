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
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "event/event.h"
#include "utils/log.h"
#include "utils/utextend.h"

#include "adapter.h"
#include "adapter/adapter_internal.h"
#include "adapter/storage.h"
#include "base/group.h"
#include "cache.h"
#include "driver_internal.h"
#include "errcodes.h"
#include "tag.h"

typedef struct to_be_write_tag {
    bool           single;
    neu_datatag_t *tag;
    neu_value_u    value;
    UT_array *     tvs;
    void *         req;
} to_be_write_tag_t;

typedef struct group {
    char *name;

    int64_t         timestamp;
    neu_group_t *   group;
    UT_array *      static_tags;
    UT_array *      wt_tags;
    pthread_mutex_t wt_mtx;

    neu_event_timer_t *report;
    neu_event_timer_t *read;
    neu_event_timer_t *write;

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

static int  report_callback(void *usr_data);
static int  read_callback(void *usr_data);
static int  write_callback(void *usr_data);
static void read_group(int64_t timestamp, int64_t timeout,
                       neu_tag_cache_type_e cache_type,
                       neu_driver_cache_t *cache, const char *group,
                       UT_array *tags, neu_resp_tag_value_meta_t *datas);
static int  read_report_group(int64_t timestamp, int64_t timeout,
                              neu_driver_cache_t *cache, const char *group,
                              UT_array *tags, neu_resp_tag_value_meta_t *datas);
static void update(neu_adapter_t *adapter, const char *group, const char *tag,
                   neu_dvalue_t value);
static void update_im(neu_adapter_t *adapter, const char *group,
                      const char *tag, neu_dvalue_t value,
                      neu_tag_meta_t *metas, int n_meta);
static void update_with_meta(neu_adapter_t *adapter, const char *group,
                             const char *tag, neu_dvalue_t value,
                             neu_tag_meta_t *metas, int n_meta);
static void write_response(neu_adapter_t *adapter, void *r, neu_error error);
static group_t *find_group(neu_adapter_driver_t *driver, const char *name);
static void     store_write_tag(group_t *group, to_be_write_tag_t *tag);

static void write_response(neu_adapter_t *adapter, void *r, neu_error error)
{
    neu_reqresp_head_t *req    = (neu_reqresp_head_t *) r;
    neu_resp_error_t    nerror = { .error = error };

    req->type = NEU_RESP_ERROR;

    adapter->cb_funs.response(adapter, req, &nerror);
    free(req);
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
                if (neu_tag_attribute_test(t, NEU_ATTRIBUTE_STATIC)) {
                    continue;
                }
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
    nlog_info(
        "update driver: %s, group: %s, tag: %s, type: %s, timestamp: %" PRId64
        " n_meta: %d",
        driver->adapter.name, group, tag, neu_type_string(value.type),
        global_timestamp, n_meta);
}

static void update_im(neu_adapter_t *adapter, const char *group,
                      const char *tag, neu_dvalue_t value,
                      neu_tag_meta_t *metas, int n_meta)
{
    neu_adapter_driver_t *driver = (neu_adapter_driver_t *) adapter;
    neu_reqresp_head_t    header = { 0 };

    if (tag == NULL || value.type == NEU_TYPE_ERROR) {
        nlog_warn("update_im tag is null or value is error %d",
                  value.value.i32);
        return;
    }

    UT_array *tags = neu_adapter_driver_get_ptag(driver, group, tag);
    if (tags == NULL) {
        return;
    }

    neu_driver_cache_update_change(driver->cache, group, tag, global_timestamp,
                                   value, metas, n_meta, true);
    driver->adapter.cb_funs.update_metric(&driver->adapter,
                                          NEU_METRIC_TAG_READS_TOTAL, 1, NULL);
    neu_datatag_t *first = utarray_front(tags);

    if (first == NULL ||
        !neu_tag_attribute_test(first, NEU_ATTRIBUTE_SUBSCRIBE)) {
        utarray_free(tags);
        nlog_info("update immediately, driver: %s, "
                  "group: %s, tag: %s, type: %s, "
                  "timestamp: %" PRId64,
                  driver->adapter.name, group, tag, neu_type_string(value.type),
                  global_timestamp);
        return;
    }

    nlog_info("update and report immediately, driver: %s, "
              "group: %s, tag: %s, type: %s, "
              "timestamp: %" PRId64,
              driver->adapter.name, group, tag, neu_type_string(value.type),
              global_timestamp);

    header.type                    = NEU_REQRESP_TRANS_DATA;
    neu_reqresp_trans_data_t *data = calloc(
        1,
        sizeof(neu_reqresp_trans_data_t) + sizeof(neu_resp_tag_value_meta_t));

    strcpy(data->driver, driver->adapter.name);
    strcpy(data->group, group);

    data->n_tag =
        read_report_group(global_timestamp, 0, driver->cache, group, tags,
                          (neu_resp_tag_value_meta_t *) &data[1]);

    if (data->n_tag > 0) {
        driver->adapter.cb_funs.response(&driver->adapter, &header, data);
    }

    utarray_free(tags);
    free(data);
}

static void update(neu_adapter_t *adapter, const char *group, const char *tag,
                   neu_dvalue_t value)
{
    update_with_meta(adapter, group, tag, value, NULL, 0);
}

neu_adapter_driver_t *neu_adapter_driver_create()
{
    neu_adapter_driver_t *driver = calloc(1, sizeof(neu_adapter_driver_t));

    driver->cache                                   = neu_driver_cache_new();
    driver->driver_events                           = neu_event_new();
    driver->adapter.cb_funs.driver.update           = update;
    driver->adapter.cb_funs.driver.write_response   = write_response;
    driver->adapter.cb_funs.driver.update_im        = update_im;
    driver->adapter.cb_funs.driver.update_with_meta = update_with_meta;

    return driver;
}

void neu_adapter_driver_destroy(neu_adapter_driver_t *driver)
{
    neu_event_close(driver->driver_events);
    neu_driver_cache_destroy(driver->cache);
}

int neu_adapter_driver_start(neu_adapter_driver_t *driver)
{
    (void) driver;
    return 0;
}

int neu_adapter_driver_stop(neu_adapter_driver_t *driver)
{
    (void) driver;
    return 0;
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
        neu_adapter_del_timer((neu_adapter_t *) driver, el->report);
        neu_event_del_timer(driver->driver_events, el->read);
        neu_event_del_timer(driver->driver_events, el->write);
        if (el->grp.group_free != NULL) {
            el->grp.group_free(&el->grp);
        }
        free(el->grp.group_name);
        free(el->name);
        utarray_free(el->grp.tags);

        utarray_foreach(el->wt_tags, to_be_write_tag_t *, tag)
        {
            neu_tag_free(tag->tag);
        }

        utarray_free(el->static_tags);
        utarray_free(el->wt_tags);
        neu_group_destroy(el->group);
        free(el);
    }

    return 0;
}

void neu_adapter_driver_start_group_timer(neu_adapter_driver_t *driver)
{
    group_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, driver->groups, el, tmp)
    {
        uint32_t                interval = neu_group_get_interval(el->group);
        neu_event_timer_param_t param    = {
            .second      = interval / 1000,
            .millisecond = interval % 1000,
            .usr_data    = el,
            .type        = NEU_EVENT_TIMER_NOBLOCK,
        };

        param.cb   = report_callback;
        el->report = neu_adapter_add_timer((neu_adapter_t *) driver, param);

        param.type = driver->adapter.module->timer_type;
        param.cb   = read_callback;
        el->read   = neu_event_add_timer(driver->driver_events, param);
    }
}

void neu_adapter_driver_stop_group_timer(neu_adapter_driver_t *driver)
{
    group_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, driver->groups, el, tmp)
    {
        neu_adapter_del_timer((neu_adapter_t *) driver, el->report);
        el->report = NULL;
        neu_event_del_timer(driver->driver_events, el->read);
        el->read = NULL;
    }
}

void neu_adapter_driver_read_group(neu_adapter_driver_t *driver,
                                   neu_reqresp_head_t *  req)
{
    neu_req_read_group_t *cmd = (neu_req_read_group_t *) &req[1];
    group_t *             g   = find_group(driver, cmd->group);
    if (g == NULL) {
        neu_resp_error_t error = { .error = NEU_ERR_GROUP_NOT_EXIST };
        req->type              = NEU_RESP_ERROR;
        driver->adapter.cb_funs.response(&driver->adapter, req, &error);
        return;
    }

    neu_resp_read_group_t resp  = { 0 };
    neu_group_t *         group = g->group;
    UT_array *            tags  = neu_group_get_read_tag(group);

    resp.n_tag = utarray_len(tags);
    resp.tags =
        calloc(1, utarray_len(tags) * sizeof(neu_resp_tag_value_meta_t));

    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        utarray_foreach(tags, neu_datatag_t *, tag)
        {
            int index = utarray_eltidx(tags, tag);
            strcpy(resp.tags[index].tag, tag->name);
            resp.tags[index].value.type      = NEU_TYPE_ERROR;
            resp.tags[index].value.value.i32 = NEU_ERR_PLUGIN_NOT_RUNNING;
        }

    } else {
        read_group(global_timestamp,
                   neu_group_get_interval(group) *
                       NEU_DRIVER_TAG_CACHE_EXPIRE_TIME,
                   neu_adapter_get_tag_cache_type(&driver->adapter),
                   driver->cache, cmd->group, tags, resp.tags);
    }

    strcpy(resp.driver, cmd->driver);
    strcpy(resp.group, cmd->group);

    utarray_free(tags);

    req->type = NEU_RESP_READ_GROUP;
    driver->adapter.cb_funs.response(&driver->adapter, req, &resp);
}

static void fix_value(neu_datatag_t *tag, neu_type_e value_type,
                      neu_dvalue_t *value)
{
    switch (tag->type) {
    case NEU_TYPE_BOOL:
    case NEU_TYPE_STRING:
    case NEU_TYPE_BYTES:
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
            value->value.u64 = neu_ntohll(value->value.u64);
            break;
        case NEU_DATATAG_ENDIAN_L64:
        default:
            break;
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
    case NEU_TYPE_UINT8:
    case NEU_TYPE_INT16:
    case NEU_TYPE_UINT16:
    case NEU_TYPE_INT32:
    case NEU_TYPE_UINT32:
    case NEU_TYPE_INT64:
    case NEU_TYPE_UINT64:
    case NEU_TYPE_WORD:
    case NEU_TYPE_LWORD:
    case NEU_TYPE_DWORD:
        if (value_type == NEU_TYPE_INT64 || value_type == NEU_TYPE_LWORD) {
            value->u64 = value->u64 / decimal;
        } else if (value_type == NEU_TYPE_DOUBLE) {
            value->u64 = (uint64_t) round(value->d64 / decimal);
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

static void free_tags(neu_req_write_tags_t *cmd)
{
    free(cmd->tags);
}

void neu_adapter_driver_write_tags(neu_adapter_driver_t *driver,
                                   neu_reqresp_head_t *  req)
{
    neu_req_write_tags_t *cmd = (neu_req_write_tags_t *) &req[1];

    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        free_tags(cmd);
        driver->adapter.cb_funs.driver.write_response(
            &driver->adapter, req, NEU_ERR_PLUGIN_NOT_RUNNING);
        return;
    }

    if (driver->adapter.module->intf_funs->driver.write_tags == NULL) {
        free_tags(cmd);
        driver->adapter.cb_funs.driver.write_response(
            &driver->adapter, req, NEU_ERR_PLUGIN_NOT_SUPPORT_WRITE_TAGS);
        return;
    }

    group_t *g = find_group(driver, cmd->group);

    if (g == NULL) {
        neu_resp_error_t error = { .error = NEU_ERR_GROUP_NOT_EXIST };
        req->type              = NEU_RESP_ERROR;
        free_tags(cmd);
        driver->adapter.cb_funs.response(&driver->adapter, req, &error);
        free(req);
        return;
    }

    UT_array *tags = NULL;
    UT_icd    icd  = { sizeof(neu_plugin_tag_value_t), NULL, NULL, NULL };
    utarray_new(tags, &icd);
    for (int i = 0; i < cmd->n_tag; i++) {
        neu_plugin_tag_value_t tv = { 0 };

        neu_datatag_t *tag = neu_group_find_tag(g->group, cmd->tags[i].tag);
        if (tag != NULL && neu_tag_attribute_test(tag, NEU_ATTRIBUTE_WRITE) &&
            neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC) == false) {
            tv.tag = neu_tag_dup(tag);

            if (tag->type == NEU_TYPE_FLOAT || tag->type == NEU_TYPE_DOUBLE) {
                if (cmd->tags[i].value.type == NEU_TYPE_INT64) {
                    tv.value.d64 = (double) tv.value.u64;
                }
            }
            if (tag->decimal != 0) {
                cal_decimal(tag->type, cmd->tags[i].value.type,
                            &cmd->tags[i].value.value, tag->decimal);
            }
            fix_value(tag, cmd->tags[i].value.type, &cmd->tags[i].value);

            tv.value = cmd->tags[i].value.value;
            utarray_push_back(tags, &tv);
        }
        if (tag != NULL) {
            neu_tag_free(tag);
        }
    }

    if (utarray_len(tags) != (unsigned int) cmd->n_tag) {
        neu_resp_error_t error = { .error = NEU_ERR_TAG_NOT_EXIST };
        req->type              = NEU_RESP_ERROR;
        free_tags(cmd);
        driver->adapter.cb_funs.response(&driver->adapter, req, &error);
        free(req);
        utarray_foreach(tags, neu_plugin_tag_value_t *, tv)
        {
            neu_tag_free(tv->tag);
        }
        utarray_free(tags);
        return;
    }

    to_be_write_tag_t wtag = { 0 };
    wtag.single            = false;
    wtag.req               = (void *) req;
    wtag.tvs               = tags;

    store_write_tag(g, &wtag);
    free_tags(cmd);
}

void neu_adapter_driver_write_tag(neu_adapter_driver_t *driver,
                                  neu_reqresp_head_t *  req)
{
    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        driver->adapter.cb_funs.driver.write_response(
            &driver->adapter, req, NEU_ERR_PLUGIN_NOT_RUNNING);
        return;
    }

    neu_req_write_tag_t *cmd = (neu_req_write_tag_t *) &req[1];
    group_t *            g   = find_group(driver, cmd->group);

    if (g == NULL) {
        neu_resp_error_t error = { .error = NEU_ERR_GROUP_NOT_EXIST };
        req->type              = NEU_RESP_ERROR;
        driver->adapter.cb_funs.response(&driver->adapter, req, &error);
        free(req);
        return;
    }
    neu_datatag_t *tag = neu_group_find_tag(g->group, cmd->tag);

    if (tag == NULL) {
        neu_resp_error_t error = { .error = NEU_ERR_TAG_NOT_EXIST };

        req->type = NEU_RESP_ERROR;
        driver->adapter.cb_funs.response(&driver->adapter, req, &error);
        free(req);
    } else {
        if ((tag->attribute & NEU_ATTRIBUTE_WRITE) != NEU_ATTRIBUTE_WRITE) {
            driver->adapter.cb_funs.driver.write_response(
                &driver->adapter, req, NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE);
            neu_tag_free(tag);
            return;
        }
        if (tag->type == NEU_TYPE_FLOAT || tag->type == NEU_TYPE_DOUBLE) {
            if (cmd->value.type == NEU_TYPE_INT64) {
                cmd->value.value.d64 = (double) cmd->value.value.u64;
            }
        }

        if (tag->decimal != 0) {
            cal_decimal(tag->type, cmd->value.type, &cmd->value.value,
                        tag->decimal);
        }

        fix_value(tag, cmd->value.type, &cmd->value);

        if (neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC)) {
            neu_driver_cache_update(g->driver->cache, g->name, tag->name,
                                    global_timestamp, cmd->value, NULL, 0);
            neu_tag_set_static_value(tag, &cmd->value.value);
            neu_group_update_tag(g->group, tag);
            adapter_storage_update_tag_value(cmd->driver, cmd->group, tag);
            neu_resp_error_t error = { .error = NEU_ERR_SUCCESS };
            req->type              = NEU_RESP_ERROR;
            driver->adapter.cb_funs.response(&driver->adapter, req, &error);
            free(req);
        } else {
            to_be_write_tag_t wtag = { 0 };
            wtag.single            = true;
            wtag.req               = (void *) req;
            wtag.value             = cmd->value.value;
            wtag.tag               = neu_tag_dup(tag);

            store_write_tag(g, &wtag);
        }

        neu_tag_free(tag);
    }
}

#define REGISTER_GROUP_METRIC(adapter, group, name, init)                  \
    neu_adapter_register_group_metric((adapter), group, name, name##_HELP, \
                                      name##_TYPE, (init))

int neu_adapter_driver_add_group(neu_adapter_driver_t *driver, const char *name,
                                 uint32_t interval)
{
    UT_icd   icd  = { sizeof(to_be_write_tag_t), NULL, NULL, NULL };
    group_t *find = NULL;
    int      ret  = NEU_ERR_GROUP_EXIST;

    HASH_FIND_STR(driver->groups, name, find);
    if (find == NULL) {
        find = calloc(1, sizeof(group_t));

        neu_event_timer_param_t param = {
            .second      = interval / 1000,
            .millisecond = interval % 1000,
            .usr_data    = (void *) find,
            .type        = NEU_EVENT_TIMER_NOBLOCK,
        };

        pthread_mutex_init(&find->wt_mtx, NULL);

        utarray_new(find->wt_tags, &icd);

        find->driver         = driver;
        find->name           = strdup(name);
        find->group          = neu_group_new(name, interval);
        find->grp.group_name = strdup(name);
        neu_group_split_static_tags(find->group, &find->static_tags,
                                    &find->grp.tags);

        param.cb     = report_callback;
        find->report = neu_adapter_add_timer((neu_adapter_t *) driver, param);
        param.type   = driver->adapter.module->timer_type;
        param.cb     = read_callback;
        find->read   = neu_event_add_timer(driver->driver_events, param);

        param.second      = 0;
        param.millisecond = 100;
        param.cb          = write_callback;
        find->write       = neu_event_add_timer(driver->driver_events, param);

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
    }

    // stop the timer first to avoid race condition
    neu_adapter_del_timer((neu_adapter_t *) driver, find->report);
    neu_event_del_timer(driver->driver_events, find->read);

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
            find->timestamp = global_timestamp; // trigger group_change
            HASH_ADD_STR(driver->groups, name, find);
        } else {
            free(new_name_cp1);
            free(new_name_cp2);
            ret = NEU_ERR_EINTERNAL;
        }
    }

    neu_event_timer_param_t param = {
        .second      = interval / 1000,
        .millisecond = interval % 1000,
        .usr_data    = (void *) find,
        .type        = NEU_EVENT_TIMER_NOBLOCK,
    };

    neu_group_set_interval(find->group, interval);

    // restore the timers
    param.cb     = report_callback;
    find->report = neu_adapter_add_timer((neu_adapter_t *) driver, param);
    param.type   = driver->adapter.module->timer_type;
    param.cb     = read_callback;
    find->read   = neu_event_add_timer(driver->driver_events, param);

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

        neu_adapter_del_timer((neu_adapter_t *) driver, find->report);
        neu_event_del_timer(driver->driver_events, find->read);
        neu_event_del_timer(driver->driver_events, find->write);
        if (find->grp.group_free != NULL) {
            find->grp.group_free(&find->grp);
        }
        free(find->grp.group_name);
        free(find->name);

        utarray_foreach(find->static_tags, neu_datatag_t *, tag)
        {
            neu_driver_cache_del(driver->cache, name, tag->name);
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

        utarray_free(find->static_tags);
        utarray_free(find->grp.tags);
        utarray_free(find->wt_tags);
        neu_group_destroy(find->group);
        free(find);

        pthread_mutex_destroy(&find->wt_mtx);
        neu_adapter_del_group_metrics(&driver->adapter, name);
        ret = NEU_ERR_SUCCESS;
    }

    return ret;
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

    if (neu_tag_attribute_test(tag, NEU_ATTRIBUTE_SUBSCRIBE) &&
        neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC)) {
        return NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT;
    }

    if (!neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC)) {
        int ret = driver->adapter.module->intf_funs->driver.validate_tag(
            driver->adapter.plugin, tag);
        if (ret != NEU_ERR_SUCCESS) {
            return ret;
        }
    }

    neu_datatag_parse_addr_option(tag, &tag->option);

    return NEU_ERR_SUCCESS;
}

int neu_adapter_driver_add_tag(neu_adapter_driver_t *driver, const char *group,
                               neu_datatag_t *tag)
{
    int      ret  = NEU_ERR_SUCCESS;
    group_t *find = NULL;

    neu_datatag_parse_addr_option(tag, &tag->option);
    driver->adapter.module->intf_funs->driver.validate_tag(
        driver->adapter.plugin, tag);

    HASH_FIND_STR(driver->groups, group, find);
    if (find == NULL) {
        neu_adapter_driver_add_group(driver, group, 3000);
        adapter_storage_add_group(driver->adapter.name, group, 3000);
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

    if (!neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC)) {
        ret = driver->adapter.module->intf_funs->driver.validate_tag(
            driver->adapter.plugin, tag);
        if (ret != NEU_ERR_SUCCESS) {
            return ret;
        }
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

static int report_callback(void *usr_data)
{
    group_t *                group  = (group_t *) usr_data;
    neu_reqresp_head_t       header = { 0 };
    neu_node_running_state_e state  = group->driver->adapter.state;
    if (state != NEU_NODE_RUNNING_STATE_RUNNING) {
        return 0;
    }

    header.type = NEU_REQRESP_TRANS_DATA;

    UT_array *tags =
        neu_adapter_driver_get_read_tag(group->driver, group->name);

    neu_reqresp_trans_data_t *data =
        calloc(1,
               sizeof(neu_reqresp_trans_data_t) +
                   utarray_len(tags) * sizeof(neu_resp_tag_value_meta_t));

    strcpy(data->driver, group->driver->adapter.name);
    strcpy(data->group, group->name);
    data->n_tag = read_report_group(global_timestamp,
                                    neu_group_get_interval(group->group) *
                                        NEU_DRIVER_TAG_CACHE_EXPIRE_TIME,
                                    group->driver->cache, group->name, tags,
                                    (neu_resp_tag_value_meta_t *) &data[1]);

    if (data->n_tag > 0) {
        group->driver->adapter.cb_funs.response(&group->driver->adapter,
                                                &header, data);
    }
    utarray_free(tags);
    free(data);
    return 0;
}

static void group_change(void *arg, int64_t timestamp, UT_array *static_tags,
                         UT_array *other_tags, uint32_t interval)
{
    group_t *group   = (group_t *) arg;
    group->timestamp = timestamp;
    (void) interval;

    if (group->grp.group_free != NULL)
        group->grp.group_free(&group->grp);

    utarray_foreach(group->static_tags, neu_datatag_t *, tag)
    {
        neu_driver_cache_del(group->driver->cache, group->name, tag->name);
    }

    utarray_foreach(group->grp.tags, neu_datatag_t *, tag)
    {
        neu_driver_cache_del(group->driver->cache, group->name, tag->name);
    }

    utarray_foreach(static_tags, neu_datatag_t *, tag)
    {
        neu_dvalue_t value = { 0 };

        value.precision = tag->precision;
        value.type      = tag->type;
        if (0 != neu_tag_get_static_value(tag, &value.value)) {
            value.type      = NEU_TYPE_ERROR;
            value.value.i32 = NEU_ERR_EINTERNAL;
        }

        neu_driver_cache_add(group->driver->cache, group->name, tag->name,
                             value);
    }

    utarray_foreach(other_tags, neu_datatag_t *, tag)
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
        .tags       = NULL,
        .group_free = NULL,
        .user_data  = NULL,
    };

    grp.tags = other_tags;
    free(group->grp.group_name);
    if (group->static_tags != NULL) {
        utarray_free(group->static_tags);
    }
    if (group->grp.tags != NULL) {
        utarray_free(group->grp.tags);
    }

    group->static_tags = static_tags;
    group->grp         = grp;
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
        if (wtag->single) {
            group->driver->adapter.module->intf_funs->driver.write_tag(
                group->driver->adapter.plugin, (void *) wtag->req, wtag->tag,
                wtag->value);
            neu_tag_free(wtag->tag);
        } else {
            group->driver->adapter.module->intf_funs->driver.write_tags(
                group->driver->adapter.plugin, (void *) wtag->req, wtag->tvs);
            utarray_foreach(wtag->tvs, neu_plugin_tag_value_t *, tv)
            {
                neu_tag_free(tv->tag);
            }
            utarray_free(wtag->tvs);
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

static int read_report_group(int64_t timestamp, int64_t timeout,
                             neu_driver_cache_t *cache, const char *group,
                             UT_array *tags, neu_resp_tag_value_meta_t *datas)
{
    int index = 0;

    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        neu_driver_cache_value_t value = { 0 };

        if (neu_tag_attribute_test(tag, NEU_ATTRIBUTE_SUBSCRIBE)) {
            if (neu_driver_cache_meta_get_changed(cache, group, tag->name,
                                                  &value, datas[index].metas,
                                                  NEU_TAG_META_SIZE) != 0) {
                nlog_info("tag: %s not changed", tag->name);
                continue;
            }
        } else {
            if (neu_driver_cache_meta_get(cache, group, tag->name, &value,
                                          datas[index].metas,
                                          NEU_TAG_META_SIZE) != 0) {
                strcpy(datas[index].tag, tag->name);
                datas[index].value.type      = NEU_TYPE_ERROR;
                datas[index].value.value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY;
                index += 1;
                continue;
            }
        }
        strcpy(datas[index].tag, tag->name);

        if (value.value.type == NEU_TYPE_ERROR) {
            datas[index].value = value.value;
            index += 1;
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
                value.value.value.u64 = neu_htonll(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_L64:
            default:
                break;
            }
            break;
        default:
            break;
        }

        if (!neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC) &&
            (timestamp - value.timestamp) > timeout && timeout > 0) {
            datas[index].value.type      = NEU_TYPE_ERROR;
            datas[index].value.value.i32 = NEU_ERR_PLUGIN_TAG_VALUE_EXPIRED;
        } else {
            if (value.value.type == NEU_TYPE_PTR) {
                datas[index].value.type = NEU_TYPE_PTR;
                datas[index].value.value.ptr.length =
                    value.value.value.ptr.length;
                datas[index].value.value.ptr.type = value.value.value.ptr.type;
                datas[index].value.value.ptr.ptr  = value.value.value.ptr.ptr;
            } else {
                datas[index].value = value.value;
            }
            if (tag->decimal != 0) {
                datas[index].value.type = NEU_TYPE_DOUBLE;
                switch (tag->type) {
                case NEU_TYPE_INT8:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.i8 * tag->decimal;
                    break;
                case NEU_TYPE_UINT8:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.u8 * tag->decimal;
                    break;
                case NEU_TYPE_INT16:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.i16 * tag->decimal;
                    break;
                case NEU_TYPE_UINT16:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.u16 * tag->decimal;
                    break;
                case NEU_TYPE_INT32:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.i32 * tag->decimal;
                    break;
                case NEU_TYPE_UINT32:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.u32 * tag->decimal;
                    break;
                case NEU_TYPE_INT64:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.i64 * tag->decimal;
                    break;
                case NEU_TYPE_UINT64:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.u64 * tag->decimal;
                    break;
                case NEU_TYPE_FLOAT:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.f32 * tag->decimal;
                    break;
                case NEU_TYPE_DOUBLE:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.d64 * tag->decimal;
                    break;
                default:
                    datas[index].value.type = tag->type;
                    break;
                }
            }
        }
        index += 1;
    }

    return index;
}

static void read_group(int64_t timestamp, int64_t timeout,
                       neu_tag_cache_type_e cache_type,
                       neu_driver_cache_t *cache, const char *group,
                       UT_array *tags, neu_resp_tag_value_meta_t *datas)
{
    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        neu_driver_cache_value_t value = { 0 };
        int                      index = utarray_eltidx(tags, tag);

        strcpy(datas[index].tag, tag->name);

        if (neu_driver_cache_meta_get(cache, group, tag->name, &value,
                                      datas[index].metas,
                                      NEU_TAG_META_SIZE) != 0) {
            datas[index].value.type      = NEU_TYPE_ERROR;
            datas[index].value.value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY;
            continue;
        }

        if (value.value.type == NEU_TYPE_ERROR) {
            datas[index].value = value.value;
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
                value.value.value.u64 = neu_htonll(value.value.value.u64);
                break;
            case NEU_DATATAG_ENDIAN_L64:
            default:
                break;
            }
            break;
        default:
            break;
        }

        if (cache_type != NEU_TAG_CACHE_TYPE_NEVER &&
            !neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC) &&
            (timestamp - value.timestamp) > timeout) {
            datas[index].value.type      = NEU_TYPE_ERROR;
            datas[index].value.value.i32 = NEU_ERR_PLUGIN_TAG_VALUE_EXPIRED;
        } else {
            if (value.value.type == NEU_TYPE_PTR) {
                datas[index].value.type = NEU_TYPE_PTR;
                datas[index].value.value.ptr.length =
                    value.value.value.ptr.length;
                datas[index].value.value.ptr.type = value.value.value.ptr.type;
                datas[index].value.value.ptr.ptr  = value.value.value.ptr.ptr;
            } else {
                datas[index].value = value.value;
            }
            if (tag->decimal != 0) {
                datas[index].value.type = NEU_TYPE_DOUBLE;
                switch (tag->type) {
                case NEU_TYPE_INT8:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.i8 * tag->decimal;
                    break;
                case NEU_TYPE_UINT8:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.u8 * tag->decimal;
                    break;
                case NEU_TYPE_INT16:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.i16 * tag->decimal;
                    break;
                case NEU_TYPE_UINT16:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.u16 * tag->decimal;
                    break;
                case NEU_TYPE_INT32:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.i32 * tag->decimal;
                    break;
                case NEU_TYPE_UINT32:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.u32 * tag->decimal;
                    break;
                case NEU_TYPE_INT64:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.i64 * tag->decimal;
                    break;
                case NEU_TYPE_UINT64:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.u64 * tag->decimal;
                    break;
                case NEU_TYPE_FLOAT:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.f32 * tag->decimal;
                    break;
                case NEU_TYPE_DOUBLE:
                    datas[index].value.value.d64 =
                        (double) datas[index].value.value.d64 * tag->decimal;
                    break;
                default:
                    datas[index].value.type = tag->type;
                    break;
                }
            }
        }
    }
}

static void store_write_tag(group_t *group, to_be_write_tag_t *tag)
{
    pthread_mutex_lock(&group->wt_mtx);
    utarray_push_back(group->wt_tags, tag);
    pthread_mutex_unlock(&group->wt_mtx);
}
