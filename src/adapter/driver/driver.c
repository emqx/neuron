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
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>

#include "event/event.h"
#include "utils/uthash.h"

#include "adapter.h"
#include "adapter/adapter_internal.h"
#include "cache.h"
#include "driver_internal.h"

struct subscribe_group {
    neu_plugin_group_t   grp;
    neu_taggrp_config_t *group;

    char *name;

    neu_event_timer_t *report;
    neu_event_timer_t *read;

    neu_adapter_driver_t *driver;

    UT_hash_handle hh;
};

struct neu_adapter_driver {
    neu_adapter_t        adapter;
    neu_datatag_table_t *tag_table;

    neu_driver_cache_t *cache;

    neu_events_t *driver_events;

    pthread_mutex_t         grp_mtx;
    struct subscribe_group *grps;
};

static int             report_callback(void *usr_data);
static int             read_callback(void *usr_data);
static neu_data_val_t *read_group(neu_adapter_driver_t *driver,
                                  neu_taggrp_config_t * grp);
static void load_tags(neu_adapter_driver_t *driver, neu_taggrp_config_t *group,
                      UT_array **tags);
static void free_tags(UT_array *tags);
static void update(neu_adapter_t *adapter, const char *name,
                   neu_dvalue_t value);
static void write_response(neu_adapter_t *adapter, void *r, neu_error error);
static void write_response(neu_adapter_t *adapter, void *r, neu_error error)
{
    neu_request_t *req = (neu_request_t *) r;

    neu_reqresp_write_resp_t data = {
        .grp_config = NULL,
        .data_val   = neu_datatag_pack_create(1),
    };
    neu_response_t resp = {
        .req_id    = req->req_id,
        .resp_type = NEU_REQRESP_WRITE_RESP,
        .buf       = &data,
        .buf_len   = sizeof(neu_reqresp_write_resp_t),
        .recver_id = req->sender_id,
    };

    neu_datatag_pack_add(data.data_val, 0, NEU_DTYPE_ERRORCODE, 0,
                         (void *) &error);
    adapter->cb_funs.response(adapter, &resp);
}

static void update(neu_adapter_t *adapter, const char *name, neu_dvalue_t value)
{
    neu_adapter_driver_t *driver = (neu_adapter_driver_t *) adapter;
    uint16_t              n_byte = 0;

    switch (value.type) {
    case NEU_TYPE_INT8:
    case NEU_TYPE_UINT8:
    case NEU_TYPE_BIT:
    case NEU_TYPE_BOOL:
        n_byte = 1;
        break;
    case NEU_TYPE_INT16:
    case NEU_TYPE_UINT16:
        n_byte = 2;
        break;
    case NEU_TYPE_INT32:
    case NEU_TYPE_UINT32:
    case NEU_TYPE_FLOAT:
        n_byte = 4;
        break;
    case NEU_TYPE_INT64:
    case NEU_TYPE_UINT64:
    case NEU_TYPE_DOUBLE:
        n_byte = 8;
        break;
    case NEU_TYPE_STRING:
        n_byte = sizeof(value.value.str);
        break;
    case NEU_TYPE_BYTES:
        break;
    default:
        break;
    }

    if (value.type == NEU_TYPE_ERROR) {
        neu_driver_cache_error(driver->cache, name, 0, value.value.i32);
    } else {
        neu_driver_cache_update(driver->cache, name, 0, n_byte,
                                value.value.bytes);
    }
}

neu_adapter_driver_t *neu_adapter_driver_create(neu_adapter_t *adapter)
{
    neu_adapter_driver_t *driver = calloc(1, sizeof(neu_adapter_driver_t));

    driver->adapter = *adapter;
    free(adapter);

    pthread_mutex_init(&driver->grp_mtx, NULL);
    driver->cache                                 = neu_driver_cache_new();
    driver->driver_events                         = neu_event_new();
    driver->adapter.cb_funs.driver.update         = update;
    driver->adapter.cb_funs.driver.write_response = write_response;

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
    driver->tag_table = neu_manager_get_datatag_tbl(driver->adapter.manager,
                                                    driver->adapter.id);

    return 0;
}

int neu_adapter_driver_uninit(neu_adapter_driver_t *driver)
{
    struct subscribe_group *sg  = NULL;
    struct subscribe_group *tmp = NULL;

    pthread_mutex_lock(&driver->grp_mtx);
    HASH_ITER(hh, driver->grps, sg, tmp)
    {
        HASH_DEL(driver->grps, sg);
        neu_adapter_del_timer((neu_adapter_t *) driver, sg->report);
        neu_event_del_timer(driver->driver_events, sg->read);
        if (sg->grp.group_free != NULL)
            sg->grp.group_free(&sg->grp);
        free(sg);
    }

    pthread_mutex_unlock(&driver->grp_mtx);

    return 0;
}

void neu_adapter_driver_process_msg(neu_adapter_driver_t *driver,
                                    neu_request_t *       req)
{
    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        neu_reqresp_read_t *    read_req = (neu_reqresp_read_t *) req->buf;
        neu_reqresp_read_resp_t data     = {
            .grp_config = read_req->grp_config,
            .data_val   = read_group(driver, read_req->grp_config),
        };
        neu_response_t resp = {
            .req_id    = req->req_id,
            .resp_type = NEU_REQRESP_READ_RESP,
            .buf       = &data,
            .buf_len   = sizeof(neu_reqresp_read_resp_t),
            .recver_id = req->sender_id,
        };

        driver->adapter.cb_funs.response(&driver->adapter, &resp);
        break;
    }
    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *write_req = (neu_reqresp_write_t *) req->buf;
        neu_fixed_array_t *  array     = NULL;
        neu_int_val_t *      int_val   = NULL;
        neu_value_u          value     = { 0 };
        neu_datatag_t *      tag       = NULL;

        neu_dvalue_get_ref_array(write_req->data_val, &array);
        assert(array->length == 1);

        int_val = neu_fixed_array_get(array, 0);

        tag = neu_datatag_tbl_get(driver->tag_table, int_val->key);
        if (tag == NULL) {
            int                      error = NEU_ERR_TAG_NOT_EXIST;
            neu_reqresp_write_resp_t data  = {
                .grp_config = write_req->grp_config,
                .data_val   = neu_datatag_pack_create(1),
            };
            neu_response_t resp = {
                .req_id    = req->req_id,
                .resp_type = NEU_REQRESP_WRITE_RESP,
                .buf       = &data,
                .buf_len   = sizeof(neu_reqresp_write_resp_t),
                .recver_id = req->sender_id,
            };

            neu_datatag_pack_add(data.data_val, 0, NEU_DTYPE_ERRORCODE,
                                 int_val->key, (void *) &error);
            driver->adapter.cb_funs.response(&driver->adapter, &resp);
        } else {
            driver->adapter.plugin_info.module->intf_funs->driver.write_tag(
                driver->adapter.plugin_info.plugin, (void *) req, tag, value);
        }

        break;
    }
    case NEU_REQRESP_SUBSCRIBE_NODE: {
        neu_reqresp_subscribe_node_t *sub_req =
            (neu_reqresp_subscribe_node_t *) req->buf;
        uint32_t interval = neu_taggrp_cfg_get_interval(sub_req->grp_config);
        struct subscribe_group *sg = calloc(1, sizeof(struct subscribe_group));
        neu_event_timer_param_t param = {
            .second      = interval / 1000,
            .millisecond = interval % 1000,
            .usr_data    = sg,
        };

        sg->driver = driver;
        sg->group  = sub_req->grp_config;
        // strncpy(sg->name, neu_taggrp_cfg_get_name(sg->group),
        // sizeof(sg->name));
        sg->name           = strdup(neu_taggrp_cfg_get_name(sg->group));
        sg->grp.group_name = strdup(neu_taggrp_cfg_get_name(sg->group));
        load_tags(driver, sg->group, &sg->grp.tags);

        pthread_mutex_lock(&driver->grp_mtx);
        HASH_ADD_STR(driver->grps, name, sg);

        param.cb   = report_callback;
        sg->report = neu_adapter_add_timer((neu_adapter_t *) driver, param);
        param.cb   = read_callback;
        sg->read   = neu_event_add_timer(driver->driver_events, param);
        pthread_mutex_unlock(&driver->grp_mtx);

        break;
    }
    case NEU_REQRESP_UNSUBSCRIBE_NODE: {
        neu_reqresp_unsubscribe_node_t *unsub_req =
            (neu_reqresp_unsubscribe_node_t *) req->buf;
        struct subscribe_group *sg = NULL;

        pthread_mutex_lock(&driver->grp_mtx);
        HASH_FIND_STR(driver->grps,
                      neu_taggrp_cfg_get_name(unsub_req->grp_config), sg);
        if (sg != NULL) {
            HASH_DEL(driver->grps, sg);
            neu_adapter_del_timer((neu_adapter_t *) driver, sg->report);
            neu_event_del_timer(driver->driver_events, sg->read);
            if (sg->grp.group_free != NULL)
                sg->grp.group_free(&sg->grp);

            for (neu_datatag_t **tag =
                     (neu_datatag_t **) utarray_front(sg->grp.tags);
                 tag != NULL;
                 tag = (neu_datatag_t **) utarray_next(sg->grp.tags, tag)) {
                neu_driver_cache_del(driver->cache, (*tag)->name);
            }
            free(sg);
        }
        // assert(sg != NULL);

        pthread_mutex_unlock(&driver->grp_mtx);

        break;
    }
    default:
        assert(1 == 0);
        break;
    }
}

static int report_callback(void *usr_data)
{
    struct subscribe_group *sg = (struct subscribe_group *) usr_data;

    pthread_mutex_lock(&sg->driver->grp_mtx);
    neu_reqresp_data_t data = {
        .grp_config = sg->group,
        .data_val   = read_group(sg->driver, sg->group),
    };
    neu_response_t resp = {
        .req_id    = 1,
        .resp_type = NEU_REQRESP_TRANS_DATA,
        .buf       = &data,
        .buf_len   = sizeof(neu_reqresp_data_t),
    };

    sg->driver->adapter.cb_funs.response(&sg->driver->adapter, &resp);
    pthread_mutex_unlock(&sg->driver->grp_mtx);

    return 0;
}

static int read_callback(void *usr_data)
{
    struct subscribe_group *sg = (struct subscribe_group *) usr_data;

    pthread_mutex_lock(&sg->driver->grp_mtx);
    if (neu_taggrp_cfg_is_anchored(sg->group)) {
        sg->driver->adapter.plugin_info.module->intf_funs->driver.group_timer(
            sg->driver->adapter.plugin_info.plugin, &sg->grp);
    } else {
        neu_taggrp_config_t *new_config = neu_system_find_group_config(
            sg->driver->adapter.plugin_info.plugin, sg->driver->adapter.id,
            neu_taggrp_cfg_get_name(sg->group));
        assert(new_config != NULL);

        if (sg->grp.group_free != NULL)
            sg->grp.group_free(&sg->grp);

        for (neu_datatag_t **tag =
                 (neu_datatag_t **) utarray_front(sg->grp.tags);
             tag != NULL;
             tag = (neu_datatag_t **) utarray_next(sg->grp.tags, tag)) {
            neu_driver_cache_del(sg->driver->cache, (*tag)->name);
        }
        neu_plugin_group_t grp = {
            .group_name = sg->grp.group_name,
            .group_free = NULL,
            .user_data  = NULL,
        };
        load_tags(sg->driver, new_config, &grp.tags);

        sg->group = new_config;
        free_tags(sg->grp.tags);
        sg->grp = grp;
    }
    pthread_mutex_unlock(&sg->driver->grp_mtx);

    return 0;
}

static neu_data_val_t *read_group(neu_adapter_driver_t *driver,
                                  neu_taggrp_config_t * grp)
{
    vector_t *      ids   = neu_taggrp_cfg_get_datatag_ids(grp);
    neu_data_val_t *val   = neu_datatag_pack_create(ids->size);
    int             index = -1;

    VECTOR_FOR_EACH(ids, iter)
    {

        int                      error = NEU_ERR_SUCCESS;
        neu_driver_cache_value_t value = { 0 };
        neu_datatag_id_t *       id = (neu_datatag_id_t *) iterator_get(&iter);
        neu_datatag_t *tag = neu_datatag_tbl_get(driver->tag_table, *id);

        index += 1;
        if (tag == NULL) {
            error = NEU_ERR_TAG_NOT_EXIST;
            neu_datatag_pack_add(val, index, NEU_DTYPE_ERRORCODE, *id,
                                 (void *) &error);
            continue;
        }

        if ((tag->attribute & NEU_ATTRIBUTE_READ) != NEU_ATTRIBUTE_READ) {
            error = NEU_ERR_PLUGIN_TAG_NOT_ALLOW_READ;
            neu_datatag_pack_add(val, index, NEU_DTYPE_ERRORCODE, tag->id,
                                 (void *) &error);
            continue;
        }

        if (neu_driver_cache_get(driver->cache, tag->name, &value) != 0) {
            error = NEU_ERR_TAG_NOT_EXIST;
            neu_datatag_pack_add(val, index, NEU_DTYPE_ERRORCODE, *id,
                                 (void *) &error);
            continue;
        }

        if (value.error != 0) {
            neu_datatag_pack_add(val, index, NEU_DTYPE_ERRORCODE, *id,
                                 (void *) &value.error);
            continue;
        }

        switch (tag->type) {
        case NEU_DTYPE_BOOL:
        case NEU_DTYPE_BIT:
        case NEU_DTYPE_INT8:
        case NEU_DTYPE_UINT8:
            assert(value.n_byte == sizeof(uint8_t));

            neu_datatag_pack_add(val, index, tag->type, tag->id,
                                 (void *) &value.value.u8);
            break;
        case NEU_DTYPE_INT16:
        case NEU_DTYPE_UINT16: {
            assert(value.n_byte == sizeof(uint16_t));
            uint16_t v = value.value.u16;

            neu_datatag_pack_add(val, index, tag->type, tag->id, (void *) &v);
            break;
        }
        case NEU_DTYPE_FLOAT:
        case NEU_DTYPE_INT32:
        case NEU_DTYPE_UINT32: {
            assert(value.n_byte == sizeof(uint32_t));
            uint32_t v = value.value.u32;

            neu_datatag_pack_add(val, index, tag->type, tag->id, (void *) &v);
            break;
        }
        case NEU_DTYPE_INT64:
        case NEU_DTYPE_UINT64:
        case NEU_DTYPE_DOUBLE: {
            assert(value.n_byte == sizeof(uint64_t));
            uint64_t v = value.value.u64;

            neu_datatag_pack_add(val, index, tag->type, tag->id, (void *) &v);
            break;
        }
        case NEU_DTYPE_CSTR:
            if (neu_datatag_string_is_utf8(value.value.str, value.n_byte)) {
                neu_datatag_pack_add(val, index, tag->type, tag->id,
                                     (void *) value.value.str);
            } else {
                char *unknown = "?";
                neu_datatag_pack_add(val, index, tag->type, tag->id,
                                     (void *) unknown);
            }
            break;
        default:
            assert(tag->type == 0);
            break;
        }
    }

    return val;
}

static void load_tags(neu_adapter_driver_t *driver, neu_taggrp_config_t *group,
                      UT_array **tags)
{
    vector_t *ids = neu_taggrp_cfg_get_datatag_ids(group);

    utarray_new(*tags, &ut_ptr_icd);

    VECTOR_FOR_EACH(ids, iter)
    {

        neu_datatag_id_t *id  = (neu_datatag_id_t *) iterator_get(&iter);
        neu_datatag_t *   tag = neu_datatag_tbl_get(driver->tag_table, *id);
        neu_datatag_t *   tmp = calloc(1, sizeof(neu_datatag_t));
        memcpy(tmp, tag, sizeof(neu_datatag_t));
        tmp->name     = strdup(tag->name);
        tmp->addr_str = strdup(tag->addr_str);

        utarray_push_back(*tags, (void *) &tmp);
    }
}

static void free_tags(UT_array *tags)
{
    neu_datatag_t **tag = (neu_datatag_t **) utarray_back(tags);

    while (tag != NULL) {
        utarray_pop_back(tags);

        free((*tag)->addr_str);
        free((*tag)->name);
        free(*tag);

        tag = (neu_datatag_t **) utarray_back(tags);
    }
    utarray_free(tags);
}