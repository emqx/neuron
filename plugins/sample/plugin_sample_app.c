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
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "neuron.h"

const neu_plugin_module_t neu_plugin_module;

struct neu_plugin {
    neu_plugin_common_t common;
    pthread_t           work_tid;
    uint32_t            new_event_id;
};

static uint32_t plugin_get_event_id(neu_plugin_t *plugin)
{
    uint32_t req_id;

    plugin->new_event_id++;
    if (plugin->new_event_id == 0) {
        plugin->new_event_id = 1;
    }

    req_id = plugin->new_event_id;
    return req_id;
}

static neu_data_val_t *setup_write_data_val()
{
    neu_data_val_t *   write_val;
    neu_fixed_array_t *array;

    write_val = neu_dvalue_unit_new();
    if (write_val == NULL) {
        log_error("Failed to allocate data value for write data");
        return NULL;
    }

    array = neu_fixed_array_new(3, sizeof(neu_int_val_t));
    if (array == NULL) {
        log_error("Failed to allocate array for write data");
        neu_dvalue_free(write_val);
        return NULL;
    }

    neu_int_val_t   int_val;
    neu_data_val_t *val_i64;
    val_i64 = neu_dvalue_new(NEU_DTYPE_INT64);
    neu_dvalue_set_int64(val_i64, 114514);
    neu_int_val_init(&int_val, 1, val_i64);
    neu_fixed_array_set(array, 0, (void *) &int_val);

    neu_data_val_t *val_f64;
    val_f64 = neu_dvalue_new(NEU_DTYPE_DOUBLE);
    neu_dvalue_set_double(val_f64, 3.14159265);
    neu_int_val_init(&int_val, 2, val_f64);
    neu_fixed_array_set(array, 1, (void *) &int_val);

    neu_data_val_t *val_cstr;
    val_cstr = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(val_cstr, "hello, sampe driver");
    neu_int_val_init(&int_val, 3, val_cstr);
    neu_fixed_array_set(array, 2, (void *) &int_val);

    neu_dvalue_init_move_array(write_val, NEU_DTYPE_INT_VAL, array);
    return write_val;
}

static void *sample_app_work_loop(void *arg)
{
    int           rv;
    neu_plugin_t *plugin;

    plugin = (neu_plugin_t *) arg;
    usleep(1000000);

    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    /* example of add plugin lib */
    neu_request_t            sync_cmd;
    neu_response_t *         result = NULL;
    neu_cmd_add_plugin_lib_t plugin_add_cmd;

    plugin_add_cmd.plugin_lib_name = "libplugin-sample-drv.so";

    sync_cmd.req_type = NEU_REQRESP_ADD_PLUGIN_LIB;
    sync_cmd.req_id   = plugin_get_event_id(plugin);
    sync_cmd.buf      = (void *) &plugin_add_cmd;
    sync_cmd.buf_len  = sizeof(neu_cmd_add_plugin_lib_t);
    log_info("Add a new plugin lib");
    rv = adapter_callbacks->command(plugin->common.adapter, &sync_cmd, &result);
    if (rv < 0) {
        log_error("Failed to add plugin lib with name: %s",
                  plugin_add_cmd.plugin_lib_name);
        return NULL;
    }
    log_info("Result of add command is %d", (intptr_t) result->buf);
    free(result);

    /* example of get plugin libs */
    neu_cmd_get_plugin_libs_t  get_plugins_cmd;
    neu_reqresp_plugin_libs_t *resp_plugins;

    result            = NULL;
    sync_cmd.req_type = NEU_REQRESP_GET_PLUGIN_LIBS;
    sync_cmd.req_id   = plugin_get_event_id(plugin);
    sync_cmd.buf      = (void *) &get_plugins_cmd;
    sync_cmd.buf_len  = sizeof(neu_cmd_get_plugin_libs_t);
    log_info("Get list of driver plugins");
    rv = adapter_callbacks->command(plugin->common.adapter, &sync_cmd, &result);
    if (rv < 0) {
        return NULL;
    }

    plugin_lib_info_t *plugin_info;
    plugin_id_t        plugin_id;
    assert(result->buf_len == sizeof(neu_reqresp_plugin_libs_t));
    resp_plugins = (neu_reqresp_plugin_libs_t *) result->buf;
    plugin_info =
        (plugin_lib_info_t *) vector_get(&resp_plugins->plugin_libs, 0);
    plugin_id = plugin_info->plugin_id;
    log_info("The first plugin(%d) of plugin list, name:%s", plugin_id,
             plugin_info->plugin_name);
    vector_uninit(&resp_plugins->plugin_libs);
    free(resp_plugins);
    free(result);

    /* example of add node */
    neu_cmd_add_node_t node_add_cmd;

    result                        = NULL;
    node_add_cmd.node_type        = NEU_NODE_TYPE_DRIVER;
    node_add_cmd.adapter_name     = "sample-driver-adapter";
    node_add_cmd.plugin_name      = "sample-drv-plugin";
    node_add_cmd.plugin_id.id_val = 0;

    sync_cmd.req_type = NEU_REQRESP_ADD_NODE;
    sync_cmd.req_id   = plugin_get_event_id(plugin);
    sync_cmd.buf      = (void *) &node_add_cmd;
    sync_cmd.buf_len  = sizeof(neu_cmd_add_node_t);
    log_info("Add a new node");
    rv = adapter_callbacks->command(plugin->common.adapter, &sync_cmd, &result);
    if (rv < 0) {
        log_error("Failed to add node with adapter: %s, plugin: %s",
                  node_add_cmd.adapter_name, node_add_cmd.plugin_name);
    }
    log_info("Result of add command is %d", (intptr_t) result->buf);
    free(result);

    /* example of get nodes */
    neu_cmd_get_nodes_t  get_nodes_cmd;
    neu_reqresp_nodes_t *resp_nodes;

    result                  = NULL;
    get_nodes_cmd.node_type = NEU_NODE_TYPE_DRIVER;
    sync_cmd.req_type       = NEU_REQRESP_GET_NODES;
    sync_cmd.req_id         = plugin_get_event_id(plugin);
    sync_cmd.buf            = (void *) &get_nodes_cmd;
    sync_cmd.buf_len        = sizeof(neu_cmd_get_nodes_t);
    log_info("Get list of driver nodes");
    rv = adapter_callbacks->command(plugin->common.adapter, &sync_cmd, &result);
    if (rv < 0) {
        return NULL;
    }

    neu_node_info_t *node_info;
    neu_node_id_t    dst_node_id;
    assert(result->buf_len == sizeof(neu_reqresp_nodes_t));
    resp_nodes  = (neu_reqresp_nodes_t *) result->buf;
    node_info   = (neu_node_info_t *) vector_get(&resp_nodes->nodes, 0);
    dst_node_id = node_info->node_id;
    log_info("The first node(%d) of driver nodes list, name:%s", dst_node_id,
             node_info->node_name);
    VECTOR_FOR_EACH(&resp_nodes->nodes, iter)
    {
        node_info = (neu_node_info_t *) iterator_get(&iter);
        free(node_info->node_name);
    }
    vector_uninit(&resp_nodes->nodes);
    free(resp_nodes);
    free(result);

    /* example of get group configs */
    neu_cmd_get_grp_configs_t  get_grps_cmd;
    neu_reqresp_grp_configs_t *resp_grps;
    neu_taggrp_config_t *      grp_config = NULL;
    result                                = NULL;

    get_grps_cmd.node_id = dst_node_id;
    sync_cmd.req_type    = NEU_REQRESP_GET_GRP_CONFIGS;
    sync_cmd.req_id      = plugin_get_event_id(plugin);
    sync_cmd.buf         = (void *) &get_grps_cmd;
    sync_cmd.buf_len     = sizeof(neu_cmd_get_grp_configs_t);
    log_info("Get list of group configs");
    int retry_count = 3;
get_grp_configs_retry:
    rv = adapter_callbacks->command(plugin->common.adapter, &sync_cmd, &result);
    if (rv < 0) {
        return NULL;
    }

    assert(result->buf_len == sizeof(neu_reqresp_grp_configs_t));
    resp_grps = (neu_reqresp_grp_configs_t *) result->buf;
    if (resp_grps->grp_configs.size == 0) {
        vector_uninit(&resp_grps->grp_configs);
        free(resp_grps);
        resp_grps = NULL;
        usleep(300000);
        if (retry_count > 0) {
            retry_count--;
            goto get_grp_configs_retry;
        }
    }

    if (resp_grps != NULL) {
        grp_config =
            *(neu_taggrp_config_t **) vector_get(&resp_grps->grp_configs, 0);
        neu_taggrp_cfg_ref(grp_config);
        log_info("The first group config(%s) of node(%d)",
                 neu_taggrp_cfg_get_name(grp_config), dst_node_id);
        VECTOR_FOR_EACH(&resp_grps->grp_configs, iter)
        {
            neu_taggrp_config_t *cur_grp_config;
            cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
            neu_taggrp_cfg_free(cur_grp_config);
        }
        vector_uninit(&resp_grps->grp_configs);
        free(resp_grps);
    }
    free(result);

    /* example of get datatag table */
    neu_cmd_get_datatags_t  get_tags_cmd;
    neu_reqresp_datatags_t *resp_tags;
    result = NULL;

    get_tags_cmd.node_id = dst_node_id;
    sync_cmd.req_type    = NEU_REQRESP_GET_DATATAGS;
    sync_cmd.req_id      = plugin_get_event_id(plugin);
    sync_cmd.buf         = (void *) &get_tags_cmd;
    sync_cmd.buf_len     = sizeof(neu_cmd_get_datatags_t);
    log_info("Get datatag table of node(%d)", dst_node_id);
    rv = adapter_callbacks->command(plugin->common.adapter, &sync_cmd, &result);
    if (rv < 0) {
        return NULL;
    }

    neu_datatag_table_t *tag_table;
    assert(result->buf_len == sizeof(neu_reqresp_datatags_t));
    resp_tags = (neu_reqresp_datatags_t *) result->buf;
    tag_table = resp_tags->datatag_tbl;
    log_info("Get the tag_table(%p)", tag_table);
    free(resp_tags);
    free(result);

    /* example of send read command */
    neu_request_t      cmd;
    neu_reqresp_read_t read_req;

    read_req.grp_config =
        (neu_taggrp_config_t *) neu_taggrp_cfg_ref(grp_config);
    read_req.dst_node_id = dst_node_id;
    cmd.req_type         = NEU_REQRESP_READ_DATA;
    cmd.req_id           = plugin_get_event_id(plugin);
    cmd.buf              = (void *) &read_req;
    cmd.buf_len          = sizeof(neu_reqresp_read_t);
    log_info("Send a read command to node: %d", dst_node_id);
    adapter_callbacks->command(plugin->common.adapter, &cmd, NULL);

    usleep(100000);

    /* example of send write command */
    neu_request_t       cmd1;
    neu_reqresp_write_t write_req;
    neu_data_val_t *    write_val = setup_write_data_val();

    write_req.grp_config =
        (neu_taggrp_config_t *) neu_taggrp_cfg_ref(grp_config);
    write_req.dst_node_id = dst_node_id;
    write_req.data_val    = write_val;
    cmd1.req_type         = NEU_REQRESP_WRITE_DATA;
    cmd1.req_id           = plugin_get_event_id(plugin);
    cmd1.buf              = (void *) &write_req;
    cmd1.buf_len          = sizeof(neu_reqresp_write_t);
    log_info("Send a write command to node: %d", dst_node_id);
    adapter_callbacks->command(plugin->common.adapter, &cmd1, NULL);

    neu_taggrp_cfg_free(grp_config);

    usleep(500000);
    log_info("Stop a node(%d)", dst_node_id);
    neu_plugin_node_ctl(plugin, dst_node_id, NEU_ADAPTER_CTL_STOP);
    usleep(500000);
    log_info("Restart a node(%d)", dst_node_id);
    neu_plugin_node_ctl(plugin, dst_node_id, NEU_ADAPTER_CTL_START);
    return NULL;
}

static neu_plugin_t *
sample_app_plugin_open(neu_adapter_t *            adapter,
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
    plugin->new_event_id             = 1;

    int rv;
    rv = pthread_create(&plugin->work_tid, NULL, sample_app_work_loop, plugin);
    if (rv != 0) {
        log_error("Failed to create work thread for sample app plugin");
        free(plugin);
        return NULL;
    }
    log_info("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;
}

static int sample_app_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    pthread_join(plugin->work_tid, NULL);
    free(plugin);
    log_info("Success to free plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int sample_app_plugin_init(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    log_info("Initialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int sample_app_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int sample_app_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    log_info("config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int handle_trans_data_value(neu_data_val_t *trans_val)
{
    neu_fixed_array_t *array;

    neu_dvalue_get_ref_array(trans_val, &array);

    neu_int_val_t * int_val;
    neu_data_val_t *val_i64;
    neu_data_val_t *val_f64;
    neu_data_val_t *val_cstr;
    int64_t         i64;
    double          f64;
    char *          cstr;

    int_val = neu_fixed_array_get(array, 1);
    val_i64 = int_val->val;
    neu_dvalue_get_int64(val_i64, &i64);
    int_val = neu_fixed_array_get(array, 2);
    val_f64 = int_val->val;
    neu_dvalue_get_double(val_f64, &f64);
    int_val  = neu_fixed_array_get(array, 3);
    val_cstr = int_val->val;
    neu_dvalue_get_ref_cstr(val_cstr, &cstr);

    log_info("The sample driver report data, i64: %" PRIi64
             ", f64: %f, cstr: %s",
             i64, f64, cstr);
    return 0;
}

static int handle_read_resp_value(neu_data_val_t *data_val)
{
    neu_fixed_array_t *array;

    neu_dvalue_get_ref_array(data_val, &array);

    neu_int_val_t * int_val;
    neu_data_val_t *val_err;
    neu_data_val_t *val_i64;
    neu_data_val_t *val_f64;
    neu_data_val_t *val_cstr;
    int32_t         err;
    int64_t         i64;
    double          f64;
    char *          cstr;

    int_val = neu_fixed_array_get(array, 0);
    val_err = int_val->val;
    neu_dvalue_get_errorcode(val_err, &err);

    int_val = neu_fixed_array_get(array, 1);
    val_i64 = int_val->val;
    neu_dvalue_get_int64(val_i64, &i64);
    int_val = neu_fixed_array_get(array, 2);
    val_f64 = int_val->val;
    neu_dvalue_get_double(val_f64, &f64);
    int_val  = neu_fixed_array_get(array, 3);
    val_cstr = int_val->val;
    neu_dvalue_get_ref_cstr(val_cstr, &cstr);

    log_info("The sample driver read status: %d", err);

    if (err == 0) {
        log_info("The sample driver read data, i64: %" PRIi64
                 ", f64: %f, cstr: %s",
                 i64, f64, cstr);
    }
    return 0;
}

static int handle_write_resp_value(neu_data_val_t *data_value)
{
    neu_fixed_array_t *array;

    neu_dvalue_get_ref_array(data_value, &array);

    neu_int_val_t * int_val;
    neu_data_val_t *val_err;
    int32_t         err;
    size_t          i;

    for (i = 0; i < array->length; i++) {
        int_val = neu_fixed_array_get(array, i);
        val_err = int_val->val;
        neu_dvalue_get_errorcode(val_err, &err);
        log_info("The sample driver write datatag(%d) return result(%d)",
                 int_val->key, err);
    }
    return 0;
}

static int sample_app_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    int rv = 0;

    if (plugin == NULL || req == NULL) {
        log_warn("The plugin pointer or request is NULL");
        return (-1);
    }

    log_info("send request to plugin: %s", neu_plugin_module.module_name);
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;
    (void) adapter_callbacks;

    switch (req->req_type) {
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_data_t *neu_data;

        assert(req->buf_len == sizeof(neu_reqresp_data_t));
        neu_data = (neu_reqresp_data_t *) req->buf;

        handle_trans_data_value(neu_data->data_val);
        break;
    }

    case NEU_REQRESP_READ_RESP: {
        neu_reqresp_read_resp_t *read_resp;

        assert(req->buf_len == sizeof(neu_reqresp_read_resp_t));
        read_resp = (neu_reqresp_read_resp_t *) req->buf;

        handle_read_resp_value(read_resp->data_val);
        break;
    }

    case NEU_REQRESP_WRITE_RESP: {
        neu_reqresp_write_resp_t *write_resp;

        assert(req->buf_len == sizeof(neu_reqresp_write_resp_t));
        write_resp = (neu_reqresp_write_resp_t *) req->buf;

        handle_write_resp_value(write_resp->data_val);
        break;
    }

    default:
        break;
    }
    return rv;
}

static int sample_app_plugin_event_reply(neu_plugin_t *     plugin,
                                         neu_event_reply_t *reply)
{
    int rv = 0;

    (void) plugin;
    (void) reply;

    log_info("reply event to plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int sample_app_plugin_validate_tag(neu_plugin_t * plugin,
                                          neu_datatag_t *tag)
{
    (void) plugin;
    (void) tag;

    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open         = sample_app_plugin_open,
    .close        = sample_app_plugin_close,
    .init         = sample_app_plugin_init,
    .uninit       = sample_app_plugin_uninit,
    .config       = sample_app_plugin_config,
    .request      = sample_app_plugin_request,
    .validate_tag = sample_app_plugin_validate_tag,
    .event_reply  = sample_app_plugin_event_reply
};

#define SAMPLE_APP_PLUGIN_DESCR \
    "A sample plugin for demonstrate how to write a neuron application plugin"

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-sample-app-plugin",
    .module_descr = SAMPLE_APP_PLUGIN_DESCR,
    .intf_funs    = &plugin_intf_funs,
    .kind         = PLUGIN_KIND_SYSTEM,
    .type         = NEU_NODE_TYPE_APP,
};
