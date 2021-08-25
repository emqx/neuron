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

static int64_t tag001;
static double  tag002;
static char    tag003[100];

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
        neu_variable_t *    head = neu_variable_create();
        neu_variable_set_qword(head, 0); // No error

        assert(req->buf_len == sizeof(neu_reqresp_read_t));
        read_req = (neu_reqresp_read_t *) req->buf;
        // log_info("Read data from addr(%d) with group config(%p), "
        //          "dst_node_id:%d, req_id:%d",
        //          read_req->addr, read_req->grp_config, read_req->dst_node_id,
        //          req->req_id);

        // log_info("------------------>%s",
        //          neu_taggrp_cfg_get_name(read_req->grp_config));
        neu_variable_t *t001 = neu_variable_create();
        neu_variable_set_qword(t001, tag001);
        neu_variable_add_item(head, t001);

        neu_variable_t *t002 = neu_variable_create();
        neu_variable_set_double(t002, tag002);
        neu_variable_add_item(head, t002);

        neu_variable_t *t003 = neu_variable_create();
        neu_variable_set_string(t003, tag003);
        neu_variable_add_item(head, t003);

        data_resp.grp_config = read_req->grp_config;
        data_resp.data_var   = head;

        memset(&resp, 0, sizeof(resp));
        resp.req_id    = req->req_id;
        resp.resp_type = NEU_REQRESP_TRANS_DATA;
        resp.buf_len   = sizeof(neu_reqresp_data_t);
        resp.buf       = &data_resp;
        rv = adapter_callbacks->response(plugin->common.adapter, &resp);
        neu_variable_destroy(head);
        break;
    }

    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *write_req;

        assert(req->buf_len == sizeof(neu_reqresp_write_t));
        write_req = (neu_reqresp_write_t *) req->buf;

        int type = neu_variable_get_type(write_req->data_var);
        if (type == NEU_DATATYPE_STRING) {
            char * ret = NULL;
            size_t len;
            neu_variable_get_string(write_req->data_var, &ret, &len);
            memset(tag003, 0x00, sizeof(tag003));
            memcpy(tag003, ret, strlen(ret));

            log_info("Write NEU_DATATYPE_STRING value: %s", ret);
        }
        if (type == NEU_DATATYPE_QWORD) {
            neu_variable_get_qword(write_req->data_var, &tag001);
            log_info("Write NEU_DATATYPE_QWORD value: %ld", tag001);
        }
        if (type == NEU_DATATYPE_DOUBLE) {
            neu_variable_get_double(write_req->data_var, &tag002);
            log_info("Write NEU_DATATYPE_DOUBLE value: %lf", tag002);
        }
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

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open        = sample_drv_plugin_open,
    .close       = sample_drv_plugin_close,
    .init        = sample_drv_plugin_init,
    .uninit      = sample_drv_plugin_uninit,
    .config      = sample_drv_plugin_config,
    .request     = sample_drv_plugin_request,
    .event_reply = sample_drv_plugin_event_reply
};

#define SAMPLE_DRV_PLUGIN_DESCR \
    "A sample plugin for demonstrate how to write a neuron driver plugin"

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-sample-drv-plugin",
    .module_descr = SAMPLE_DRV_PLUGIN_DESCR,
    .intf_funs    = &plugin_intf_funs
};
