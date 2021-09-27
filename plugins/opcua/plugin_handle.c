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

#include <stdlib.h>

#include <neuron.h>

#include "open62541_client.h"
#include "plugin_handle.h"

#define UNUSED(x) (void) (x)

#define NAMESPACE_LEN 16
#define IDENTIFIER_LEN 128

static void start_periodic_read(opc_subscribe_tuple_t *tuple);
static void stop_periodic_read(opc_subscribe_tuple_t *tuple);

static neu_datatag_t *get_datatag_by_id(neu_datatag_table_t *table,
                                        datatag_id_t         id)
{
    if (NULL == table) {
        return NULL;
    }
    return neu_datatag_tbl_get(table, id);
}

static int fixed_address(const char *address, int *identifier_type,
                         char *namespace_str, char *identifier_str)
{
    UNUSED(identifier_type);

    char *sub_str = strchr(address, '!');
    if (NULL != sub_str) {
        int offset = sub_str - address;
        memcpy(namespace_str, address, offset);
        strcpy(identifier_str, sub_str + 1);
    } else {
        log_error("address:%s", address);
        return -1;
    }

    log_info("address:%s, namespace_str:%s, identifier_str:%s", address,
             namespace_str, identifier_str);

    if (0 != string_is_number(namespace_str)) {
        log_error("address:%s, namespace_str:%s", address, namespace_str);
        return -2;
    }

    if (0 != string_is_number(identifier_str)) {
        *identifier_type = 0;
    } else {
        *identifier_type = 1;
    }

    return 0;
}

static void generate_read_result_array(vector_t *         datas,
                                       neu_fixed_array_t *array)
{
    opcua_data_t *data;

    int total = datas->size;
    for (int i = 0; i < total; i++) {
        data = vector_get(datas, i);

        neu_data_val_t *val;
        neu_int_val_t   int_val;
        if (0 != data->error) {
            val = neu_dvalue_new(NEU_DTYPE_ERRORCODE);
            neu_dvalue_set_errorcode(val, data->error);
            neu_int_val_init(&int_val, data->opcua_node.id, val);
            neu_fixed_array_set(array, i, (void *) &int_val);
            continue;
        }

        int type = data->type;
        switch (type) {
        case NEU_DTYPE_BYTE: {
            val = neu_dvalue_new(NEU_DTYPE_BYTE);
            neu_dvalue_set_uint8(val, data->value.value_uint8);
            break;
        }
        case NEU_DTYPE_BOOL: {
            val = neu_dvalue_new(NEU_DTYPE_BOOL);
            neu_dvalue_set_bool(val, data->value.value_boolean);
            break;
        }
        case NEU_DTYPE_INT16: {
            val = neu_dvalue_new(NEU_DTYPE_INT16);
            neu_dvalue_set_int16(val, data->value.value_int16);
            break;
        }
        case NEU_DTYPE_UINT16: {
            val = neu_dvalue_new(NEU_DTYPE_UINT16);
            neu_dvalue_set_uint16(val, data->value.value_uint16);
            break;
        }
        case NEU_DTYPE_INT32: {
            val = neu_dvalue_new(NEU_DTYPE_INT32);
            neu_dvalue_set_int32(val, data->value.value_int32);
            break;
        }
        case NEU_DTYPE_UINT32: {
            val = neu_dvalue_new(NEU_DTYPE_UINT32);
            neu_dvalue_set_uint32(val, data->value.value_uint32);
            break;
        }
        case NEU_DTYPE_INT64: {
            val = neu_dvalue_new(NEU_DTYPE_INT64);
            neu_dvalue_set_int64(val, data->value.value_int64);
            break;
        }
        case NEU_DTYPE_UINT64: {
            val = neu_dvalue_new(NEU_DTYPE_UINT64);
            neu_dvalue_set_uint64(val, data->value.value_uint64);
            break;
        }
        case NEU_DTYPE_FLOAT: {
            val = neu_dvalue_new(NEU_DTYPE_FLOAT);
            neu_dvalue_set_float(val, data->value.value_float);
            break;
        }
        case NEU_DTYPE_DOUBLE: {
            val = neu_dvalue_new(NEU_DTYPE_DOUBLE);
            neu_dvalue_set_float(val, data->value.value_double);
            break;
        }
        case NEU_DTYPE_CSTR: {
            val = neu_dvalue_new(NEU_DTYPE_CSTR);
            neu_dvalue_set_cstr(val, data->value.value_string);
            break;
        }

        default:
            break;
        }

        neu_int_val_init(&int_val, data->opcua_node.id, val);
        neu_fixed_array_set(array, i, (void *) &int_val);
    }
}

static void generate_read_node_vector(opc_handle_context_t *context,
                                      neu_taggrp_config_t * config,
                                      vector_t *            datas)
{
    log_info("Group config name:%s", neu_taggrp_cfg_get_name(config));
    vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);
    if (NULL == ids) {
        return;
    }
    datatag_id_t tag_id;
    VECTOR_FOR_EACH(ids, iter)
    {
        tag_id                 = *(datatag_id_t *) iterator_get(&iter);
        neu_datatag_t *datatag = get_datatag_by_id(context->table, tag_id);
        if (NULL == datatag) {
            continue;
        }

        log_info("Datatag in group config, name:%s, id:%u", datatag->name,
                 datatag->id);

        char namespace_str[16]  = { 0 };
        char identifier_str[32] = { 0 };
        int  identifier_type    = 0;
        int  rc = fixed_address(datatag->addr_str, &identifier_type,
                               namespace_str, identifier_str);
        if (0 != rc) {
            continue;
        }

        opcua_data_t read_data;
        read_data.opcua_node.name            = datatag->name;
        read_data.opcua_node.id              = tag_id;
        read_data.opcua_node.namespace_index = atoi(namespace_str);
        read_data.opcua_node.identifier_type = identifier_type;
        if (0 == identifier_type) {
            read_data.opcua_node.identifier_str = strdup(identifier_str);
        } else if (1 == identifier_type) {
            read_data.opcua_node.identifier_int = atoi(identifier_str);
        }
        vector_push_back(datas, &read_data);
    }
}

static void generate_write_result_array(vector_t *      datas,
                                        neu_data_val_t *resp_val)
{
    int                size = datas->size;
    neu_fixed_array_t *array;
    array = neu_fixed_array_new(size, sizeof(neu_int_val_t));
    if (array == NULL) {
        log_error("Failed to allocate array for response write data");
        neu_dvalue_free(resp_val);
    }

    neu_int_val_t resp_int_val;
    VECTOR_FOR_EACH(datas, iter)
    {
        opcua_data_t *  data = (opcua_data_t *) iterator_get(&iter);
        neu_data_val_t *val_i32;
        val_i32 = neu_dvalue_unit_new();
        neu_dvalue_init_int32(val_i32, data->error);
        neu_int_val_init(&resp_int_val, data->opcua_node.id, val_i32);
    }

    neu_dvalue_init_move_array(resp_val, NEU_DTYPE_INT_VAL, array);
}

static void generate_write_node_vector(opc_handle_context_t *context,
                                       neu_fixed_array_t *   array,
                                       vector_t *            datas)
{
    int size = neu_fixed_array_size(array);

    neu_int_val_t * int_val;
    neu_data_val_t *val;
    uint32_t        id;
    for (int i = 0; i < size; i++) {
        int_val = neu_fixed_array_get(array, i);
        id      = int_val->key;
        val     = int_val->val;

        neu_dtype_e type = neu_dvalue_get_type(val);

        neu_datatag_t *datatag = get_datatag_by_id(context->table, id);
        if (NULL == datatag) {
            continue; // error return ?
        }

        char namespace_str[NAMESPACE_LEN]   = { 0 };
        char identifier_str[IDENTIFIER_LEN] = { 0 };
        int  identifier_type                = 0;
        fixed_address(datatag->addr_str, &identifier_type, namespace_str,
                      identifier_str);

        opcua_data_t write_data;
        write_data.opcua_node.name            = datatag->name;
        write_data.opcua_node.id              = id;
        write_data.opcua_node.namespace_index = atoi(namespace_str);
        write_data.opcua_node.identifier_type = identifier_type;
        if (0 == identifier_type) {
            write_data.opcua_node.identifier_str = strdup(identifier_str);
        } else if (1 == identifier_type) {
            write_data.opcua_node.identifier_int = atoi(identifier_str);
        }

        write_data.type = type;

        switch (type) {
        case NEU_DTYPE_BYTE: {
            uint8_t value;
            neu_dvalue_get_uint8(val, &value);
            write_data.value.value_uint8 = value;
            break;
        }
        case NEU_DTYPE_INT8: {
            int8_t value;
            neu_dvalue_get_int8(val, &value);
            write_data.value.value_int8 = value;
            break;
        }
        case NEU_DTYPE_INT16: {
            int16_t value;
            neu_dvalue_get_int16(val, &value);
            write_data.value.value_int16 = value;
            break;
        }
        case NEU_DTYPE_INT32: {
            int32_t value;
            neu_dvalue_get_int32(val, &value);
            write_data.value.value_int32 = value;
            break;
        }
        case NEU_DTYPE_INT64: {
            int64_t value;
            neu_dvalue_get_int64(val, &value);
            write_data.value.value_int64 = value;
            break;
        }
        case NEU_DTYPE_UINT8: {
            uint8_t value;
            neu_dvalue_get_uint8(val, &value);
            write_data.value.value_uint8 = value;
            break;
        }
        case NEU_DTYPE_UINT16: {
            uint16_t value;
            neu_dvalue_get_uint16(val, &value);
            write_data.value.value_int16 = value;
            break;
        }
        case NEU_DTYPE_UINT32: {
            uint32_t value;
            neu_dvalue_get_uint32(val, &value);
            write_data.value.value_uint32 = value;
            break;
        }
        case NEU_DTYPE_UINT64: {
            uint64_t value;
            neu_dvalue_get_uint64(val, &value);
            write_data.value.value_uint64 = value;
            break;
        }
        case NEU_DTYPE_FLOAT: {
            float value;
            neu_dvalue_get_float(val, &value);
            write_data.value.value_float = value;
            break;
        }
        case NEU_DTYPE_DOUBLE: {
            double value;
            neu_dvalue_get_double(val, &value);
            write_data.value.value_double = value;
            break;
        }
        case NEU_DTYPE_CSTR: {
            char *value;
            neu_dvalue_get_ref_cstr(val, &value);
            write_data.value.value_string = value;
            break;
        }
        default:
            break;
        }

        vector_push_back(datas, &write_data);
    }
}

int plugin_handle_read_once(opc_handle_context_t *context,
                            neu_taggrp_config_t * config,
                            neu_data_val_t *      resp_val)
{
    if (NULL == config) {
        log_error("Group config is NULL");
        return -1;
    }

    const vector_t *ids = neu_taggrp_cfg_ref_datatag_ids(config);
    if (NULL == ids) {
        log_error("Datatag vector is NULL");
        return -2;
    }

    vector_t data;
    vector_init(&data, ids->size, sizeof(opcua_data_t));
    generate_read_node_vector(context, config, &data);

    int rc = open62541_client_is_connected(context->client);
    if (0 != rc) {
        log_error("Open62541 client offline");
        vector_uninit(&data);
        return -3;
    }

    rc = open62541_client_read(context->client, &data);
    if (0 != rc) {
        log_error("Open62541 read error:%d", rc);
        return -4;
    }

    neu_fixed_array_t *array;
    array = neu_fixed_array_new(ids->size, sizeof(neu_int_val_t));
    if (array == NULL) {
        log_error("Failed to allocate array for response tags");
    }

    generate_read_result_array(&data, array);
    vector_uninit(&data);
    neu_dvalue_init_move_array(resp_val, NEU_DTYPE_INT_VAL, array);

    return 0;
}

int plugin_handle_write_value(opc_handle_context_t *context,
                              neu_data_val_t *      write_val,
                              neu_data_val_t *      resp_val)
{
    neu_fixed_array_t *array;
    neu_dvalue_get_ref_array(write_val, &array);

    vector_t datas;
    vector_init(&datas, 10, sizeof(opcua_data_t));

    generate_write_node_vector(context, array, &datas);

    int rc = open62541_client_is_connected(context->client);
    if (0 != rc) {
        log_error("open62541 client offline");
        return -1;
    }

    rc = open62541_client_write(context->client, &datas);
    generate_write_result_array(&datas, resp_val);
    vector_uninit(&datas);

    return 0;
}

static void periodic_read(nng_aio *aio, void *arg, int code)
{
    switch (code) {
    case NNG_ETIMEDOUT: {
        opc_subscribe_tuple_t *tuple = (opc_subscribe_tuple_t *) arg;
        if (neu_taggrp_cfg_is_anchored(tuple->config)) {
            neu_variable_t *head = neu_variable_create();

            neu_data_val_t *resp_val;
            resp_val = neu_dvalue_unit_new();
            if (resp_val == NULL) {
                log_error("Failed to allocate data value for response tags");
                break;
            }

            plugin_handle_read_once(tuple->context, tuple->config, resp_val);
            if (NULL != tuple->periodic_cb) {
                tuple->periodic_cb(tuple->plugin, tuple->config, resp_val);
            }
            neu_variable_destroy(head);
            nng_aio_defer(aio, periodic_read, arg);
        } else {
            neu_taggrp_config_t *new_config = neu_system_find_group_config(
                tuple->plugin, tuple->node_id, tuple->name);

            stop_periodic_read(tuple);

            if (new_config != NULL) {
                neu_taggrp_cfg_free(tuple->config);
                tuple->config = new_config;
                start_periodic_read(tuple);
            }
        }

        break;
    }
    case NNG_ECANCELED:
        log_warn("aio: %p cancel", aio);
        break;
    default:
        log_warn("aio: %p, skip error: %d", aio, code);
        break;
    }
}

static opc_subscribe_tuple_t *add_subscribe(opc_handle_context_t *context,
                                            neu_taggrp_config_t * config)
{
    opc_subscribe_tuple_t *opc_subscribe =
        malloc(sizeof(opc_subscribe_tuple_t));

    opc_subscribe->plugin  = context->plugin;
    opc_subscribe->node_id = context->self_node_id;
    opc_subscribe->name    = strdup(neu_taggrp_cfg_get_name(config));
    opc_subscribe->config  = config;

    NEU_LIST_NODE_INIT(&opc_subscribe->node);
    neu_list_append(&context->subscribe_list, opc_subscribe);
    return opc_subscribe;
}

static void start_periodic_read(opc_subscribe_tuple_t *tuple)
{
    uint32_t interval = neu_taggrp_cfg_get_interval(tuple->config);
    nng_aio_alloc(&tuple->aio, NULL, NULL);
    nng_aio_set_timeout(tuple->aio, interval != 0 ? interval : 10000);

    nng_aio_defer(tuple->aio, periodic_read, tuple);
}

static void start_opc_subscribe(opc_handle_context_t *context,
                                neu_taggrp_config_t * config)
{
    UNUSED(config);
    open62541_client_subscribe(context->client, NULL);
}

int plugin_handle_subscribe(opc_handle_context_t *      context,
                            neu_taggrp_config_t *       config,
                            periodic_response_callback  periodic_cb,
                            subscribe_response_callback subscribe_cb)
{
    opc_subscribe_tuple_t *tuple = add_subscribe(context, config);
    if (NULL == tuple) {
        return -1;
    }

    tuple->periodic_cb  = periodic_cb;
    tuple->subscribe_cb = subscribe_cb;

    start_periodic_read(tuple);
    start_opc_subscribe(context, config);
    return 0;
}

static opc_subscribe_tuple_t *find_subscribe(opc_handle_context_t *context,
                                             neu_taggrp_config_t * config)
{
    opc_subscribe_tuple_t *item;
    opc_subscribe_tuple_t *tuple = NULL;
    NEU_LIST_FOREACH(&context->subscribe_list, item)
    {
        if (item->config == config) {
            tuple = item;
        }
    }
    return tuple;
}

static void stop_periodic_read(opc_subscribe_tuple_t *tuple)
{
    nng_aio_cancel(tuple->aio);
    nng_aio_free(tuple->aio);
}

static void remove_subscribe(opc_subscribe_tuple_t *tuple)
{
    if (NULL == tuple) {
        return;
    }

    if (NULL != tuple->name) {
        free(tuple->name);
    }

    stop_periodic_read(tuple);
    neu_list_remove(&tuple->context->subscribe_list, tuple);
}

static void stop_opc_subscribe(opc_handle_context_t *context,
                               neu_taggrp_config_t * config)
{
    UNUSED(config);
    open62541_client_unsubscribe(context->client, NULL);
}

int plugin_handle_unsubscribe(opc_handle_context_t *context,
                              neu_taggrp_config_t * config)
{
    opc_subscribe_tuple_t *tuple = find_subscribe(context, config);
    stop_periodic_read(tuple);
    stop_opc_subscribe(context, config);
    remove_subscribe(tuple);
    return 0;
}
