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
#include <stdio.h>
#include <stdlib.h>

#include "neuron.h"

const neu_plugin_module_t neu_plugin_module;

#define TAG_DATA_STR_LEN 100
static int64_t tag001                       = 42;
static double  tag002                       = 2.718281828;
static char    tag003[TAG_DATA_STR_LEN + 1] = "Hello, sample app";

struct neu_plugin {
    neu_plugin_common_t common;
};

static neu_plugin_t *
sample_drv_plugin_open(neu_adapter_t *            adapter,
                       const adapter_callbacks_t *callbacks)
{
    neu_plugin_t *plugin;

    if (adapter == NULL || callbacks == NULL) {
        log_error("Open plugin with NULL adapter or callbacks");
        return NULL;
    }

    plugin = (neu_plugin_t *) malloc(sizeof(neu_plugin_t));
    if (plugin == NULL) {
        log_error("Failed to allocate plugin %s",
                  neu_plugin_module.module_name);
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);
    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    log_info("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;
}

static int sample_drv_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    free(plugin);
    log_info("Success to free plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int sample_drv_plugin_init(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    log_info("Initialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int sample_drv_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int sample_drv_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    log_info("config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static neu_data_val_t *setup_read_resp_data_value()
{
    neu_data_val_t *   resp_val;
    neu_fixed_array_t *array;

    resp_val = neu_dvalue_unit_new();
    if (resp_val == NULL) {
        log_error("Failed to allocate data value for response tags");
        return NULL;
    }

    // Return array of reading result, the first element is error code of
    // all reading, so length is count of tags plus 1.
    // In this case, length is 3 + 1 = 4.
    array = neu_fixed_array_new(4, sizeof(neu_int_val_t));
    if (array == NULL) {
        log_error("Failed to allocate array for response tags");
        neu_dvalue_free(resp_val);
        return NULL;
    }

    neu_int_val_t   int_val;
    neu_data_val_t *val_err;
    val_err = neu_dvalue_new(NEU_DTYPE_ERRORCODE);
    neu_dvalue_set_errorcode(val_err, 0);
    // tag id of first element is 0 for indicate all reading
    neu_int_val_init(&int_val, 0, val_err);
    neu_fixed_array_set(array, 0, (void *) &int_val);

    neu_data_val_t *val_i64;
    val_i64 = neu_dvalue_new(NEU_DTYPE_INT64);
    neu_dvalue_set_int64(val_i64, tag001);
    neu_int_val_init(&int_val, 1, val_i64);
    neu_fixed_array_set(array, 1, (void *) &int_val);

    neu_data_val_t *val_f64;
    val_f64 = neu_dvalue_new(NEU_DTYPE_DOUBLE);
    neu_dvalue_set_double(val_f64, tag002);
    neu_int_val_init(&int_val, 2, val_f64);
    neu_fixed_array_set(array, 2, (void *) &int_val);

    neu_data_val_t *val_cstr;
    val_cstr = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(val_cstr, tag003);
    neu_int_val_init(&int_val, 3, val_cstr);
    neu_fixed_array_set(array, 3, (void *) &int_val);

    neu_dvalue_init_move_array(resp_val, NEU_DTYPE_INT_VAL, array);
    return resp_val;
}

static int handle_write_data_value(neu_data_val_t * write_val,
                                   neu_data_val_t **p_resp_val)
{
    neu_fixed_array_t *array;

    neu_dvalue_get_ref_array(write_val, &array);

    neu_int_val_t * int_val;
    neu_data_val_t *val_i64;
    neu_data_val_t *val_f64;
    neu_data_val_t *val_cstr;
    char *          cstr;

    log_info("Write data values to driver cache");
    int_val = neu_fixed_array_get(array, 0);
    val_i64 = int_val->val;
    neu_dvalue_get_int64(val_i64, &tag001);
    int_val = neu_fixed_array_get(array, 1);
    val_f64 = int_val->val;
    neu_dvalue_get_double(val_f64, &tag002);
    int_val  = neu_fixed_array_get(array, 2);
    val_cstr = int_val->val;
    neu_dvalue_get_ref_cstr(val_cstr, &cstr);
    strncpy(tag003, cstr, TAG_DATA_STR_LEN);

    neu_data_val_t *resp_val;

    *p_resp_val = NULL;
    resp_val    = neu_dvalue_unit_new();
    if (resp_val == NULL) {
        log_error("Failed to allocate data value for response write data");
        return -1;
    }

    // Return array of writting error code, the first element is error code of
    // all writting, so length is count of tags plus 1.
    // In this case, length is 3 + 1 = 4.
    array = neu_fixed_array_new(4, sizeof(neu_int_val_t));
    if (array == NULL) {
        log_error("Failed to allocate array for response write data");
        neu_dvalue_free(resp_val);
        return -1;
    }

    // tag id of first element is 0 for indicate all writting
    size_t        i;
    neu_int_val_t resp_int_val;
    for (i = 0; i < array->length; i++) {
        neu_data_val_t *val_i32;

        val_i32 = neu_dvalue_unit_new();
        // all results of writing is success, so set 0
        neu_dvalue_init_errorcode(val_i32, 0);
        neu_int_val_init(&resp_int_val, i, val_i32);
        neu_fixed_array_set(array, i, &resp_int_val);
    }

    neu_dvalue_init_move_array(resp_val, NEU_DTYPE_INT_VAL, array);
    *p_resp_val = resp_val;
    return 0;
}

static int sample_drv_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    int rv = 0;

    if (plugin == NULL || req == NULL) {
        log_warn("The plugin pointer or request is NULL");
        return (-1);
    }

    log_info("send request to plugin: %s", neu_plugin_module.module_name);
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        neu_reqresp_read_t *read_req;
        neu_response_t      resp;
        neu_reqresp_data_t  data_resp;
        neu_node_id_t       sender_id;

        assert(req->buf_len == sizeof(neu_reqresp_read_t));
        read_req  = (neu_reqresp_read_t *) req->buf;
        sender_id = req->sender_id;

        data_resp.grp_config = read_req->grp_config;
        data_resp.data_val   = setup_read_resp_data_value();
        if (data_resp.data_val == NULL) {
            rv = -1;
            break;
        }

        memset(&resp, 0, sizeof(resp));
        resp.req_id    = req->req_id;
        resp.resp_type = NEU_REQRESP_READ_RESP;
        resp.recver_id = sender_id;
        resp.buf_len   = sizeof(neu_reqresp_read_t);
        resp.buf       = &data_resp;
        rv = adapter_callbacks->response(plugin->common.adapter, &resp);
        break;
    }

    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *    write_req;
        neu_response_t           resp;
        neu_reqresp_write_resp_t data_resp;
        neu_node_id_t            sender_id;

        assert(req->buf_len == sizeof(neu_reqresp_write_t));
        write_req = (neu_reqresp_write_t *) req->buf;
        sender_id = req->sender_id;

        neu_data_val_t *resp_val;
        handle_write_data_value(write_req->data_val, &resp_val);
        if (resp_val == NULL) {
            rv = -1;
            break;
        }

        data_resp.grp_config = write_req->grp_config;
        data_resp.data_val   = resp_val;

        memset(&resp, 0, sizeof(resp));
        resp.req_id    = req->req_id;
        resp.resp_type = NEU_REQRESP_WRITE_RESP;
        resp.recver_id = sender_id;
        resp.buf_len   = sizeof(neu_reqresp_write_resp_t);
        resp.buf       = &data_resp;
        rv = adapter_callbacks->response(plugin->common.adapter, &resp);
        break;
    }

    case NEU_REQRESP_TRANS_DATA: {
        break;
    }

    default:
        break;
    }
    return rv;
}

static int sample_drv_plugin_event_reply(neu_plugin_t *     plugin,
                                         neu_event_reply_t *reply)
{
    int rv = 0;

    (void) plugin;
    (void) reply;

    log_info("reply event to plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int sample_drv_plugin_validate_tag(neu_plugin_t * plugin,
                                          neu_datatag_t *tag)
{
    (void) plugin;
    (void) tag;

    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open         = sample_drv_plugin_open,
    .close        = sample_drv_plugin_close,
    .init         = sample_drv_plugin_init,
    .uninit       = sample_drv_plugin_uninit,
    .config       = sample_drv_plugin_config,
    .request      = sample_drv_plugin_request,
    .validate_tag = sample_drv_plugin_validate_tag,
    .event_reply  = sample_drv_plugin_event_reply
};

#define SAMPLE_DRV_PLUGIN_DESCR \
    "A sample plugin for demonstrate how to write a neuron driver plugin"

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-sample-drv-plugin",
    .module_descr = SAMPLE_DRV_PLUGIN_DESCR,
    .intf_funs    = &plugin_intf_funs,
    .kind         = PLUGIN_KIND_SYSTEM,
    .type         = NEU_NODE_TYPE_DRIVER
};
