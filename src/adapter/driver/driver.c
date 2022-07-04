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
#include <stdlib.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "event/event.h"
#include "utils/log.h"
#include "utils/utextend.h"

#include "adapter.h"
#include "adapter/adapter_internal.h"
#include "base/group.h"
#include "cache.h"
#include "driver_internal.h"
#include "errcodes.h"
#include "tag.h"

typedef struct group {
    char *name;

    int64_t      timestamp;
    neu_group_t *group;

    neu_event_timer_t *report;
    neu_event_timer_t *read;

    neu_plugin_group_t    grp;
    neu_adapter_driver_t *driver;

    UT_hash_handle hh;
} group_t;

struct neu_adapter_driver {
    neu_adapter_t adapter;

    neu_driver_cache_t *cache;
    neu_events_t *      driver_events;

    struct group *groups;
};

static int  report_callback(void *usr_data);
static int  read_callback(void *usr_data);
static void read_group(int64_t timestamp, int64_t timeout,
                       neu_driver_cache_t *cache, const char *group,
                       UT_array *tags, neu_resp_tag_value_t *datas);
static void update(neu_adapter_t *adapter, const char *group, const char *tag,
                   neu_dvalue_t value);
static void write_response(neu_adapter_t *adapter, void *r, neu_error error);
static group_t *find_group(neu_adapter_driver_t *driver, const char *name);

static void write_response(neu_adapter_t *adapter, void *r, neu_error error)
{
    neu_reqresp_head_t *req    = (neu_reqresp_head_t *) r;
    neu_resp_error_t    nerror = { .error = error };

    req->type = NEU_RESP_ERROR;

    adapter->cb_funs.response(adapter, req, &nerror);
    free(req);
}

static void update(neu_adapter_t *adapter, const char *group, const char *tag,
                   neu_dvalue_t value)
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
    case NEU_TYPE_ERROR:
        break;
    default:
        assert(value.type == NEU_TYPE_INT8);
        break;
    }

    if (value.type == NEU_TYPE_ERROR) {
        if (tag != NULL) {
            neu_driver_cache_error(driver->cache, group, tag,
                                   driver->adapter.timestamp, value.value.i32);
        } else {
            group_t *g = find_group(driver, group);
            if (g != NULL) {
                UT_array *tags = neu_group_get_read_tag(g->group);

                utarray_foreach(tags, neu_datatag_t *, t)
                {
                    neu_driver_cache_error(driver->cache, group, t->name,
                                           driver->adapter.timestamp,
                                           value.value.i32);
                }
                utarray_free(tags);
            }
        }
    } else {
        neu_driver_cache_update(driver->cache, group, tag,
                                driver->adapter.timestamp, n_byte,
                                value.value.bytes);
    }
    nlog_debug(
        "update driver: %s, group: %s, tag: %s, type: %s, timestamp: %" PRId64,
        driver->adapter.name, group, tag, neu_type_string(value.type),
        driver->adapter.timestamp);
}

neu_adapter_driver_t *neu_adapter_driver_create()
{
    neu_adapter_driver_t *driver = calloc(1, sizeof(neu_adapter_driver_t));

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
    (void) driver;

    return 0;
}

int neu_adapter_driver_uninit(neu_adapter_driver_t *driver)
{
    group_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, driver->groups, el, tmp)
    {
        HASH_DEL(driver->groups, el);

        neu_adapter_del_timer((neu_adapter_t *) driver, el->report);
        neu_event_del_timer(driver->driver_events, el->read);
        if (el->grp.group_free != NULL) {
            el->grp.group_free(&el->grp);
        }
        free(el->grp.group_name);
        free(el->name);
        utarray_free(el->grp.tags);

        neu_group_destroy(el->group);
        free(el);
    }

    return 0;
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
    resp.tags  = calloc(1, utarray_len(tags) * sizeof(neu_resp_tag_value_t));

    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        utarray_foreach(tags, neu_datatag_t *, tag)
        {
            int index = utarray_eltidx(tags, tag);
            strcpy(resp.tags[index].tag, tag->name);
            resp.tags[index].value.type      = NEU_TYPE_ERROR;
            resp.tags[index].value.value.i32 = NEU_ERR_NODE_NOT_RUNNING;
        }

    } else {
        read_group(driver->adapter.timestamp,
                   neu_group_get_interval(group) *
                       NEU_DRIVER_TAG_CACHE_EXPIRE_TIME,
                   driver->cache, cmd->group, tags, resp.tags);
    }

    strcpy(resp.driver, cmd->driver);
    strcpy(resp.group, cmd->group);

    utarray_free(tags);

    req->type = NEU_RESP_READ_GROUP;
    driver->adapter.cb_funs.response(&driver->adapter, req, &resp);
}

void neu_adapter_driver_write_tag(neu_adapter_driver_t *driver,
                                  neu_reqresp_head_t *  req)
{
    if (driver->adapter.state != NEU_NODE_RUNNING_STATE_RUNNING) {
        driver->adapter.cb_funs.driver.write_response(&driver->adapter, req,
                                                      NEU_ERR_NODE_NOT_RUNNING);
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
            return;
        }
        switch (tag->type) {
        case NEU_TYPE_BOOL:
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_UINT64:
        case NEU_TYPE_INT64:
        case NEU_TYPE_STRING:
            break;
        case NEU_TYPE_BIT:
            cmd->value.type     = NEU_TYPE_BIT;
            cmd->value.value.u8 = (uint8_t) cmd->value.value.u64;
            break;
        case NEU_TYPE_UINT8:
        case NEU_TYPE_INT8:
            cmd->value.type     = NEU_TYPE_UINT8;
            cmd->value.value.u8 = (uint8_t) cmd->value.value.u64;
            break;
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
            cmd->value.type      = NEU_TYPE_UINT16;
            cmd->value.value.u16 = (uint16_t) cmd->value.value.u64;
            break;
        case NEU_TYPE_UINT32:
        case NEU_TYPE_INT32:
            cmd->value.type      = NEU_TYPE_UINT32;
            cmd->value.value.u32 = (uint32_t) cmd->value.value.u64;
            break;
        case NEU_TYPE_FLOAT:
            cmd->value.type      = NEU_TYPE_FLOAT;
            cmd->value.value.f32 = (float) cmd->value.value.d64;
            break;
        default:
            assert(false);
            break;
        }

        driver->adapter.module->intf_funs->driver.write_tag(
            driver->adapter.plugin, (void *) req, tag, cmd->value.value);
        free(tag->address);
        free(tag->name);
        free(tag->description);
        free(tag);
    }
}

int neu_adapter_driver_add_group(neu_adapter_driver_t *driver, const char *name,
                                 uint32_t interval)
{
    group_t *find = NULL;
    int      ret  = NEU_ERR_GROUP_EXIST;

    HASH_FIND_STR(driver->groups, name, find);
    if (find == NULL) {
        find = calloc(1, sizeof(group_t));

        neu_event_timer_param_t param = {
            .second      = interval / 1000,
            .millisecond = interval % 1000,
            .usr_data    = (void *) find,
        };

        find->driver         = driver;
        find->name           = strdup(name);
        find->group          = neu_group_new(name, interval);
        find->grp.group_name = strdup(name);
        find->grp.tags       = neu_group_get_tag(find->group);

        param.cb     = report_callback;
        find->report = neu_adapter_add_timer((neu_adapter_t *) driver, param);
        param.cb     = read_callback;
        find->read   = neu_event_add_timer(driver->driver_events, param);

        HASH_ADD_STR(driver->groups, name, find);
        ret = NEU_ERR_SUCCESS;
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

        uint16_t tag_size = neu_group_tag_size(find->group);
        neu_adapter_del_timer((neu_adapter_t *) driver, find->report);
        neu_event_del_timer(driver->driver_events, find->read);
        if (find->grp.group_free != NULL) {
            find->grp.group_free(&find->grp);
        }
        free(find->grp.group_name);
        free(find->name);

        utarray_foreach(find->grp.tags, neu_datatag_t *, tag)
        {
            neu_driver_cache_del(driver->cache, name, tag->name);
        }

        utarray_free(find->grp.tags);
        neu_group_destroy(find->group);
        free(find);

        neu_plugin_to_plugin_common(driver->adapter.plugin)->tag_size -=
            tag_size;
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

int neu_adapter_driver_add_tag(neu_adapter_driver_t *driver, const char *group,
                               neu_datatag_t *tag)
{
    int      ret  = NEU_ERR_SUCCESS;
    group_t *find = NULL;

    if (strlen(tag->name) > NEU_TAG_NAME_LEN) {
        return NEU_ERR_TAG_NAME_TOO_LONG;
    }

    if (strlen(tag->address) > NEU_TAG_ADDRESS_LEN) {
        return NEU_ERR_TAG_ADDRESS_TOO_LONG;
    }

    if (strlen(tag->description) > NEU_TAG_DESCRIPTION_LEN) {
        return NEU_ERR_TAG_DESCRIPTION_TOO_LONG;
    }

    ret = driver->adapter.module->intf_funs->driver.validate_tag(
        driver->adapter.plugin, tag);
    if (ret != NEU_ERR_SUCCESS) {
        return ret;
    }

    HASH_FIND_STR(driver->groups, group, find);
    if (find != NULL) {
        ret = neu_group_add_tag(find->group, tag);
    } else {
        ret = NEU_ERR_GROUP_NOT_EXIST;
    }

    if (ret == NEU_ERR_SUCCESS) {
        neu_plugin_to_plugin_common(driver->adapter.plugin)->tag_size += 1;
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
        neu_plugin_to_plugin_common(driver->adapter.plugin)->tag_size -= 1;
    }

    return ret;
}

int neu_adapter_driver_update_tag(neu_adapter_driver_t *driver,
                                  const char *group, neu_datatag_t *tag)
{
    int      ret  = NEU_ERR_SUCCESS;
    group_t *find = NULL;

    if (strlen(tag->name) > NEU_TAG_NAME_LEN) {
        return NEU_ERR_TAG_NAME_TOO_LONG;
    }

    if (strlen(tag->address) > NEU_TAG_ADDRESS_LEN) {
        return NEU_ERR_TAG_ADDRESS_TOO_LONG;
    }

    if (strlen(tag->description) > NEU_TAG_DESCRIPTION_LEN) {
        return NEU_ERR_TAG_DESCRIPTION_TOO_LONG;
    }

    ret = driver->adapter.module->intf_funs->driver.validate_tag(
        driver->adapter.plugin, tag);
    if (ret != NEU_ERR_SUCCESS) {
        return ret;
    }

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
                   utarray_len(tags) * sizeof(neu_resp_tag_value_t));

    strcpy(data->driver, group->driver->adapter.name);
    strcpy(data->group, group->name);
    data->n_tag = utarray_len(tags);
    read_group(group->driver->adapter.timestamp,
               neu_group_get_interval(group->group) *
                   NEU_DRIVER_TAG_CACHE_EXPIRE_TIME,
               group->driver->cache, group->name, tags,
               (neu_resp_tag_value_t *) &data[1]);

    group->driver->adapter.cb_funs.response(&group->driver->adapter, &header,
                                            data);
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

    neu_plugin_group_t grp = {
        .group_name = strdup(group->name),
        .tags       = NULL,
        .group_free = NULL,
        .user_data  = NULL,
    };

    grp.tags = tags;
    free(group->grp.group_name);
    if (group->grp.tags != NULL) {
        utarray_free(group->grp.tags);
    }

    group->grp = grp;
    nlog_notice("group: %s changed", group->name);
}

static int read_callback(void *usr_data)
{
    group_t *                group = (group_t *) usr_data;
    neu_node_running_state_e state = group->driver->adapter.state;
    if (state != NEU_NODE_RUNNING_STATE_RUNNING) {
        return 0;
    }

    if (!neu_group_is_change(group->group, group->timestamp)) {
        if (group->grp.tags != NULL) {
            group->driver->adapter.module->intf_funs->driver.group_timer(
                group->driver->adapter.plugin, &group->grp);
        }
    } else {
        neu_group_change_test(group->group, group->timestamp, (void *) group,
                              group_change);
    }

    return 0;
}

static void read_group(int64_t timestamp, int64_t timeout,
                       neu_driver_cache_t *cache, const char *group,
                       UT_array *tags, neu_resp_tag_value_t *datas)
{
    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        neu_driver_cache_value_t value = { 0 };
        int                      index = utarray_eltidx(tags, tag);

        strcpy(datas[index].tag, tag->name);

        if (neu_driver_cache_get(cache, group, tag->name, &value) != 0) {
            datas[index].value.type      = NEU_TYPE_ERROR;
            datas[index].value.value.i32 = NEU_ERR_PLUGIN_READ_FAILURE;
            continue;
        }

        if (value.error != 0) {
            datas[index].value.type      = NEU_TYPE_ERROR;
            datas[index].value.value.i32 = value.error;
            continue;
        }

        if ((timestamp - value.timestamp) > timeout) {
            datas[index].value.type      = NEU_TYPE_ERROR;
            datas[index].value.value.i32 = NEU_ERR_PLUGIN_TAG_EXPIRED;
        } else {
            datas[index].value.type  = tag->type;
            datas[index].value.value = value.value;
        }
    }
}
