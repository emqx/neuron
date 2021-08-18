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
        neu_variable_t      data_var;

        assert(req->buf_len == sizeof(neu_reqresp_read_t));
        read_req = (neu_reqresp_read_t *) req->buf;
        log_info("Read data from addr(%d) with group config(%p)",
                 read_req->addr, read_req->grp_config);

        if (3 == read_req->addr) {
            // NEU_DATATYPE_STRING
            data_var.var_type.typeId = NEU_DATATYPE_STRING;
            data_var.data            = (void *) tag003;
            log_info("NEU_REQRESP_READ_DATA string:%s", (char *) data_var.data);
        }
        if (4 == read_req->addr) {
            // NEU_DATATYPE_DOUBLE
            data_var.var_type.typeId = NEU_DATATYPE_DOUBLE;
            data_var.data            = (void *) &tag002;
            log_info("NEU_REQRESP_READ_DATA double:%lf",
                     *(double *) data_var.data);
        }
        if (5 == read_req->addr) {
            // NEU_DATATYPE_QWORD
            data_var.var_type.typeId = NEU_DATATYPE_QWORD;
            data_var.data            = (void *) &tag001;
            log_info("NEU_REQRESP_READ_DATA qword:%ld",
                     *(int64_t *) data_var.data);
        }

        // static const char *resp_str = "Sample plugin read response";
        // data_var.var_type.typeId    = NEU_DATATYPE_STRING;
        // data_var.data               = (void *) resp_str;

        data_resp.grp_config = read_req->grp_config;
        data_resp.data_var   = &data_var;

        memset(&resp, 0, sizeof(resp));
        resp.req_id    = req->req_id;
        resp.resp_type = NEU_REQRESP_TRANS_DATA;
        resp.buf_len   = sizeof(neu_reqresp_data_t);
        resp.buf       = &data_resp;
        rv = adapter_callbacks->response(plugin->common.adapter, &resp);
        break;
    }

    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *write_req;

        assert(req->buf_len == sizeof(neu_reqresp_write_t));
        write_req = (neu_reqresp_write_t *) req->buf;

        int type = write_req->data_var->var_type.typeId;
        if (type == NEU_DATATYPE_STRING) {
            const char *data_str;
            data_str = neu_variable_get_str(write_req->data_var);
            memset(tag003, 0x00, sizeof(tag003));
            memcpy(tag003, data_str, strlen(data_str));

            // log_info("Write %s \n\tto addr(%d) with group config(%p)",
            // data_str,
            //          write_req->addr, write_req->grp_config);
            log_info("Write NEU_DATATYPE_STRING value: %s", data_str);
        }
        if (type == NEU_DATATYPE_QWORD) {
            tag001 = *(int64_t *) write_req->data_var->data;
            log_info("Write NEU_DATATYPE_QWORD value: %ld", tag001);
        }
        if (type == NEU_DATATYPE_DOUBLE) {
            tag002 = *(double *) write_req->data_var->data;
            log_info("Write NEU_DATATYPE_DOUBLE value: %lf", tag002);
        }
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
