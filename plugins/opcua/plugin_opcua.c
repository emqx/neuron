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

#include <neuron.h>

#include "option.h"

#include "open62541_client.h"
#include "plugin_handle.h"

#define UNUSED(x) (void) (x)

const neu_plugin_module_t neu_plugin_module;

struct neu_plugin {
    neu_plugin_common_t common;
    option_t            option;
    open62541_client_t *client;
};

static neu_plugin_t *opcua_plugin_open(neu_adapter_t *            adapter,
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

static int opcua_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    free(plugin);
    log_info("Success to free plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int opcua_plugin_init(neu_plugin_t *plugin)
{
    // OPC-UA option
    plugin->option.default_cert_file = "./opcua-default-cert.der";
    plugin->option.default_key_file  = "./opcua-default-key.der";
    plugin->option.host              = "localhost";
    plugin->option.port              = 4840;
    plugin->option.username          = "gaoc01";
    plugin->option.password          = "123456";
    plugin->option.cert_file         = "";
    plugin->option.key_file          = "";

    int rc = open62541_client_open(&plugin->option, plugin, &plugin->client);
    if (0 != rc) {
        log_error("Can not connect to opc.tcp://%s:%d", plugin->option.host,
                  plugin->option.port);
        return -1;
    }

    plugin_handle_set_open62541_client(plugin->client);

    log_info("Initialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int opcua_plugin_uninit(neu_plugin_t *plugin)
{
    open62541_client_close(plugin->client);
    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int opcua_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    log_info("config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static neu_datatag_t *get_datatag_by_id(neu_plugin_t *plugin, datatag_id_t id)
{
    neu_node_id_t        self_node_id = neu_plugin_self_node_id(plugin);
    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, self_node_id);
    if (NULL == table) {
        return NULL;
    }

    return neu_datatag_tbl_get(table, id);
}

static int fix_address(const char *address, int *identifier_type,
                       char *namespace_str, char *identifier_str)
{
    UNUSED(identifier_type);

    char *sub_str = strchr(address, '!');
    if (NULL != sub_str) {
        int offset = sub_str - address;
        memcpy(namespace_str, address, offset);
        strcpy(identifier_str, sub_str + 1);
    }
    log_info("address:%s, namespace_str:%s, identifier_str:%s", address,
             namespace_str, identifier_str);
    return 0;
}

static void read_variable_array(vector_t *data, neu_variable_t *array)
{
    opcua_data_t *d;
    VECTOR_FOR_EACH(data, iter)
    {
        d = (opcua_data_t *) iterator_get(&iter);

        neu_variable_t *v       = neu_variable_create();
        char            key[16] = { 0 };
        sprintf(key, "%d", d->opcua_node.id);
        neu_variable_set_key(v, key);

        int type = d->type;
        switch (type) {
        case NEU_DATATYPE_DWORD: {
            neu_variable_set_dword(v, d->value.value_int32);
            neu_variable_set_error(v, 0);
            break;
        }
        case NEU_DATATYPE_STRING: {
            neu_variable_set_string(v, d->value.value_string);
            neu_variable_set_error(v, 0);
            break;
        }
        case NEU_DATATYPE_FLOAT: {
            neu_variable_set_float(v, d->value.value_float);
            neu_variable_set_error(v, 0);
            break;
        }
        case NEU_DATATYPE_BOOLEAN: {
            neu_variable_set_boolean(v, d->value.value_boolean);
            neu_variable_set_error(v, 0);
            break;
        }
        default:
            break;
        }

        neu_variable_add_item(array, v);
    }
}

static void read_data_vector(neu_plugin_t *       plugin,
                             neu_taggrp_config_t *grp_config, vector_t *data)
{
    log_info("Group config name:%s", neu_taggrp_cfg_get_name(grp_config));
    vector_t *ids = neu_taggrp_cfg_get_datatag_ids(grp_config);
    if (NULL == ids) {
        return;
    }
    datatag_id_t tag_id;
    VECTOR_FOR_EACH(ids, iter)
    {
        tag_id                 = *(datatag_id_t *) iterator_get(&iter);
        neu_datatag_t *datatag = get_datatag_by_id(plugin, tag_id);
        if (NULL == datatag) {
            continue;
        }

        log_info("Datatag in group config, name:%s, id:%u", datatag->name,
                 datatag->id);

        char namespace_str[16]  = { 0 };
        char identifier_str[32] = { 0 };
        int  identifier_type    = 0;
        fix_address(datatag->addr_str, &identifier_type, namespace_str,
                    identifier_str);

        opcua_data_t read_data;
        read_data.opcua_node.name            = datatag->name;
        read_data.opcua_node.id              = tag_id;
        read_data.opcua_node.namespace_index = atoi(namespace_str);
        read_data.opcua_node.identifier_type = identifier_type;
        read_data.opcua_node.identifier_str  = strdup(identifier_str);
        vector_push_back(data, &read_data);
    }
}

static void write_variable_array(neu_plugin_t *plugin, neu_variable_t *array,
                                 vector_t *data)
{
    char *key = NULL;
    for (neu_variable_t *cursor = array; NULL != cursor;
         cursor                 = neu_variable_next(cursor)) {
        int type = neu_variable_get_type(cursor);
        neu_variable_get_key(cursor, &key);
        int tag_id = atoi(key);

        neu_datatag_t *datatag = get_datatag_by_id(plugin, tag_id);
        if (NULL == datatag) {
            continue;
        }

        char namespace_str[16]  = { 0 };
        char identifier_str[32] = { 0 };
        int  identifier_type    = 0;
        fix_address(datatag->addr_str, &identifier_type, namespace_str,
                    identifier_str);

        opcua_data_t write_data;
        write_data.opcua_node.name            = datatag->name;
        write_data.opcua_node.id              = tag_id;
        write_data.opcua_node.namespace_index = atoi(namespace_str);
        write_data.opcua_node.identifier_type = identifier_type;
        write_data.opcua_node.identifier_str  = strdup(identifier_str);

        write_data.type = type;
        switch (type) {
        case NEU_DATATYPE_DWORD: {
            int value;
            neu_variable_get_dword(cursor, &value);
            write_data.value.value_int32 = value;
            break;
        }
        case NEU_DATATYPE_STRING: {
            char * str;
            size_t len;
            neu_variable_get_string(cursor, &str, &len);
            write_data.value.value_string = str;
            break;
        }
        case NEU_DATATYPE_DOUBLE: {
            double value;
            neu_variable_get_double(cursor, &value);
            // write_data.value.value_double = value;
            write_data.type              = NEU_DATATYPE_FLOAT;
            write_data.value.value_float = value;
            break;
        }
        case NEU_DATATYPE_BOOLEAN: {
            bool value;
            neu_variable_get_boolean(cursor, &value);
            write_data.value.value_boolean = value;
            break;
        }

        default:
            break;
        }

        vector_push_back(data, &write_data);
    }
}

static int opcua_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    if (plugin == NULL || req == NULL) {
        log_warn("The plugin pointer or request is NULL");
        return -1;
    }

    int rc = 0;
    UNUSED(rc);
    log_info("send request to plugin: %s", neu_plugin_module.module_name);
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        neu_reqresp_read_t *read_req;
        assert(req->buf_len == sizeof(neu_reqresp_read_t));
        read_req = (neu_reqresp_read_t *) req->buf;

        vector_t data;
        vector_init(&data, 5, sizeof(opcua_data_t));
        read_data_vector(plugin, read_req->grp_config, &data);
        rc                   = plugin_handle_read(plugin, &data);
        neu_variable_t *head = neu_variable_create();
        read_variable_array(&data, head);

        neu_response_t     resp;
        neu_reqresp_data_t data_resp;
        data_resp.grp_config = read_req->grp_config;
        data_resp.data_var   = head;
        memset(&resp, 0, sizeof(resp));
        resp.req_id    = req->req_id;
        resp.resp_type = NEU_REQRESP_TRANS_DATA;
        resp.buf_len   = sizeof(neu_reqresp_data_t);
        resp.buf       = &data_resp;
        rc = adapter_callbacks->response(plugin->common.adapter, &resp);

        neu_variable_destroy(head);
        vector_uninit(&data);
        break;
    }
    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *write_req;
        assert(req->buf_len == sizeof(neu_reqresp_write_t));
        write_req                = (neu_reqresp_write_t *) req->buf;
        neu_variable_t *data_var = write_req->data_var;
        vector_t        v;
        vector_init(&v, 10, sizeof(opcua_data_t));
        write_variable_array(plugin, data_var, &v);
        rc = plugin_handle_write(plugin, &v);
        vector_uninit(&v);
        break;
    }

    default:
        break;
    }
    return 0;
}

static int opcua_plugin_event_reply(neu_plugin_t *     plugin,
                                    neu_event_reply_t *reply)
{
    int rv = 0;

    (void) plugin;
    (void) reply;

    log_info("reply event to plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open        = opcua_plugin_open,
    .close       = opcua_plugin_close,
    .init        = opcua_plugin_init,
    .uninit      = opcua_plugin_uninit,
    .config      = opcua_plugin_config,
    .request     = opcua_plugin_request,
    .event_reply = opcua_plugin_event_reply
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-opcua-plugin",
    .module_descr = "Neuron OPC-UA interactive communication plugin",
    .intf_funs    = &plugin_intf_funs
};
