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

#include "opcua_handle.h"

#define UNUSED(x) (void) (x)

#define SET_RESULT_ERRORCODE(resp_val, code)                            \
    {                                                                   \
        neu_fixed_array_t *array;                                       \
        array = neu_fixed_array_new(1, sizeof(neu_int_val_t));          \
        if (array == NULL) {                                            \
            log_error("Failed to allocate array for response tags");    \
            neu_dvalue_free(resp_val);                                  \
            return -10;                                                 \
        }                                                               \
        neu_int_val_t   int_val;                                        \
        neu_data_val_t *val_err;                                        \
        val_err = neu_dvalue_new(NEU_DTYPE_ERRORCODE);                  \
        neu_dvalue_set_errorcode(val_err, code);                        \
        neu_int_val_init(&int_val, 0, val_err);                         \
        neu_fixed_array_set(array, 0, (void *) &int_val);               \
        neu_dvalue_init_move_array(resp_val, NEU_DTYPE_INT_VAL, array); \
    }

#define NAMESPACE_LEN 16
#define IDENTIFIER_LEN 128

static neu_datatag_t *datatag_get_by_id(neu_datatag_table_t *table,
                                        datatag_id_t         id)
{
    if (NULL == table) {
        return NULL;
    }

    return neu_datatag_tbl_get(table, id);
}

static int address_fixed(const char *address, int *identifier_type,
                         char *namespace_str, char *identifier_str)
{
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

static void node_vector_release(vector_t *datas)
{
    if (NULL == datas) {
        return;
    }

    VECTOR_FOR_EACH(datas, iter)
    {
        opcua_data_t *data = (opcua_data_t *) iterator_get(&iter);
        if (NULL != data) {
            if (NULL != data->opcua_node.identifier_str) {
                free(data->opcua_node.identifier_str);
                data->opcua_node.identifier_str = NULL;
            }
        }
    }
}

static int read_response_array_generate(vector_t *         datas,
                                        neu_fixed_array_t *array)
{
    int           rc    = 0;
    opcua_data_t *data  = NULL;
    int           total = datas->size;
    for (int i = 0; i < total; i++) {
        data                    = vector_get(datas, i);
        neu_data_val_t *val     = NULL;
        neu_int_val_t   int_val = { 0 };
        if (0 != data->error) {
            val = neu_dvalue_new(NEU_DTYPE_ERRORCODE);
            neu_dvalue_set_errorcode(val, data->error);
            neu_int_val_init(&int_val, data->opcua_node.id, val);
            neu_fixed_array_set(array, i + 1, (void *) &int_val);
            rc++;
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

            if (NULL != data->value.value_string) {
                free(data->value.value_string);
            }

            break;
        }

        default: {
            val = neu_dvalue_new(NEU_DTYPE_ERRORCODE);
            neu_dvalue_set_errorcode(val, -1); // no match
            break;
        }
        }

        neu_int_val_init(&int_val, data->opcua_node.id, val);
        neu_fixed_array_set(array, i + 1, (void *) &int_val);
    }

    return rc;
}

static void read_request_vector_generate(opc_handle_context_t *context,
                                         neu_taggrp_config_t * config,
                                         vector_t *            datas)
{
    // log_info("Group config name:%s", neu_taggrp_cfg_get_name(config));
    vector_t *datatag_ids = neu_taggrp_cfg_get_datatag_ids(config);
    if (NULL == datatag_ids) {
        return;
    }

    datatag_id_t datatag_id = 0;
    VECTOR_FOR_EACH(datatag_ids, iter)
    {
        datatag_id             = *(datatag_id_t *) iterator_get(&iter);
        neu_datatag_t *datatag = datatag_get_by_id(context->table, datatag_id);
        if (NULL == datatag) {
            continue;
        }

        log_info("Datatag in group config, name:%s, id:%u", datatag->name,
                 datatag->id);

        char namespace_str[NAMESPACE_LEN]   = { 0 };
        char identifier_str[IDENTIFIER_LEN] = { 0 };
        int  identifier_type                = 0;
        int  rc = address_fixed(datatag->addr_str, &identifier_type,
                               namespace_str, identifier_str);
        if (0 != rc) {
            continue;
        }

        opcua_data_t read_data               = { 0 };
        read_data.opcua_node.name            = datatag->name;
        read_data.opcua_node.id              = datatag_id;
        read_data.opcua_node.namespace_index = atoi(namespace_str);
        read_data.opcua_node.identifier_type = identifier_type;
        read_data.type                       = datatag->type;
        if (0 == identifier_type) {
            read_data.opcua_node.identifier_str = strdup(identifier_str);
        } else if (1 == identifier_type) {
            read_data.opcua_node.identifier_int = atoi(identifier_str);
        }

        vector_push_back(datas, &read_data);
    }
}

static int write_response_array_generate(vector_t *         datas,
                                         neu_fixed_array_t *array)
{
    int index = 1;
    int error = 0;
    VECTOR_FOR_EACH(datas, iter)
    {
        opcua_data_t *data = (opcua_data_t *) iterator_get(&iter);
        if (0 != data->error) {
            error++;
        }

        neu_data_val_t *val_i32 = neu_dvalue_unit_new();
        neu_dvalue_init_int32(val_i32, data->error);
        neu_int_val_t resp_int_val = { 0 };
        neu_int_val_init(&resp_int_val, data->opcua_node.id, val_i32);
        neu_fixed_array_set(array, index, (void *) &resp_int_val);
        index++;
    }

    return error;
}

static bool datatag_id_equal(const void *a, const void *b)
{
    neu_datatag_id_t *id_a = (neu_datatag_id_t *) a;
    neu_datatag_id_t *id_b = (neu_datatag_id_t *) b;
    return *id_a == *id_b;
}

static void write_request_vector_generate(opc_handle_context_t *context,
                                          neu_taggrp_config_t * config,
                                          neu_fixed_array_t *   array,
                                          vector_t *            datas)
{
    vector_t *      datatag_ids = neu_taggrp_cfg_get_datatag_ids(config);
    int             size        = array->length;
    neu_int_val_t * int_val     = NULL;
    neu_data_val_t *val         = NULL;
    uint32_t        id          = 0;
    for (int i = 0; i < size; i++) {
        int_val = neu_fixed_array_get(array, i);
        id      = int_val->key;
        val     = int_val->val;

        neu_datatag_t *datatag = datatag_get_by_id(context->table, id);
        if (NULL == datatag) {
            opcua_data_t write_data = { 0 };
            write_data.error        = OPCUA_ERROR_DATATAG_NULL;
            vector_push_back(datas, &write_data);
            continue;
        }

        if (!vector_has_elem(datatag_ids, &id, datatag_id_equal)) {
            opcua_data_t write_data = { 0 };
            write_data.error        = OPCUA_ERROR_DATATAG_NOT_MATCH;
            vector_push_back(datas, &write_data);
            continue;
        }

        neu_dtype_e type                         = (neu_dtype_e) datatag->type;
        char        namespace_str[NAMESPACE_LEN] = { 0 };
        char        identifier_str[IDENTIFIER_LEN] = { 0 };
        int         identifier_type                = 0;
        address_fixed(datatag->addr_str, &identifier_type, namespace_str,
                      identifier_str);

        opcua_data_t write_data               = { 0 };
        write_data.opcua_node.name            = datatag->name;
        write_data.opcua_node.id              = id;
        write_data.opcua_node.namespace_index = atoi(namespace_str);
        write_data.opcua_node.identifier_type = identifier_type;
        write_data.type                       = type;
        if (0 == identifier_type) {
            write_data.opcua_node.identifier_str = strdup(identifier_str);
        } else if (1 == identifier_type) {
            write_data.opcua_node.identifier_int = atoi(identifier_str);
        }

        switch (type) {
        case NEU_DTYPE_INT8: {
            // int8(NEURON) -> sbyte(OPCUA)
            int64_t value = 0;
            neu_dvalue_get_int64(val, &value);
            write_data.value.value_int8 = (int8_t) value;
            break;
        }
        case NEU_DTYPE_INT16: {
            int64_t value = 0;
            neu_dvalue_get_int64(val, &value);
            write_data.value.value_int16 = (int16_t) value;
            break;
        }
        case NEU_DTYPE_INT32: {
            int64_t value = 0;
            neu_dvalue_get_int64(val, &value);
            write_data.value.value_int32 = (int32_t) value;
            break;
        }
        case NEU_DTYPE_INT64: {
            int64_t value = 0;
            neu_dvalue_get_int64(val, &value);
            write_data.value.value_int64 = value;
            break;
        }
        case NEU_DTYPE_UINT8: {
            // uint8(Neuron) -> byte(OPCUA)
            int64_t value = 0;
            neu_dvalue_get_int64(val, &value);
            write_data.value.value_uint8 = (uint8_t) value;
            break;
        }
        case NEU_DTYPE_UINT16: {
            int64_t value = 0;
            neu_dvalue_get_int64(val, &value);
            write_data.value.value_uint16 = (uint16_t) value;
            break;
        }
        case NEU_DTYPE_UINT32: {
            int64_t value = 0;
            neu_dvalue_get_int64(val, &value);
            write_data.value.value_uint32 = (uint32_t) value;
            break;
        }
        case NEU_DTYPE_UINT64: {
            int64_t value = 0;
            neu_dvalue_get_int64(val, &value);
            write_data.value.value_uint64 = (uint64_t) value;
            break;
        }
        case NEU_DTYPE_FLOAT: {
            double value = 0;
            neu_dvalue_get_double(val, &value);
            write_data.value.value_float = (float) value;
            break;
        }
        case NEU_DTYPE_DOUBLE: {
            double value = 0;
            neu_dvalue_get_double(val, &value);
            write_data.value.value_double = value;
            break;
        }
        case NEU_DTYPE_CSTR: {
            char *value = NULL;
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

opcua_error_code_e opcua_handle_read_once(opc_handle_context_t *context,
                                          neu_taggrp_config_t * config,
                                          neu_data_val_t *      resp_val)
{
    opcua_error_code_e code = OPCUA_ERROR_SUCESS;

    if (NULL == config) {
        code = OPCUA_ERROR_GROUP_CONFIG_NULL;
        log_error("Group config is NULL");
        SET_RESULT_ERRORCODE(resp_val, code);
        return code;
    }

    const vector_t *datatag_ids = neu_taggrp_cfg_ref_datatag_ids(config);
    if (NULL == datatag_ids) {
        code = OPCUA_ERROR_DATATAG_VECTOR_NULL;
        log_error("Datatag vector is NULL");
        SET_RESULT_ERRORCODE(resp_val, code);
        return code;
    }

    vector_t datas = { 0 };
    vector_init(&datas, datatag_ids->size, sizeof(opcua_data_t));
    read_request_vector_generate(context, config, &datas);

    int rc = open62541_client_is_connected(context->client);
    if (0 != rc) {
        code = OPCUA_ERROR_CLIENT_OFFLINE;
        log_error("Open62541 client offline");
        SET_RESULT_ERRORCODE(resp_val, code);

        node_vector_release(&datas);
        vector_uninit(&datas);
        return code;
    }

    rc = open62541_client_read(context->client, &datas);
    if (0 != rc) {
        code = OPCUA_ERROR_READ_FAIL;
        log_error("Open62541 read error:%d", rc);
        SET_RESULT_ERRORCODE(resp_val, code);

        node_vector_release(&datas);
        vector_uninit(&datas);
        return code;
    }

    neu_fixed_array_t *array = NULL;
    array = neu_fixed_array_new(datatag_ids->size + 1, sizeof(neu_int_val_t));
    if (NULL == array) {
        code = OPCUA_ERROR_ARRAY_NULL;
        log_error("Failed to allocate array for response tags");
        neu_dvalue_free(resp_val);

        node_vector_release(&datas);
        vector_uninit(&datas);
        return code;
    }

    rc                      = read_response_array_generate(&datas, array);
    neu_int_val_t   int_val = { 0 };
    neu_data_val_t *val_err = NULL;
    val_err                 = neu_dvalue_new(NEU_DTYPE_ERRORCODE);
    neu_dvalue_set_errorcode(val_err, rc);
    neu_int_val_init(&int_val, 0, val_err);
    neu_fixed_array_set(array, 0, (void *) &int_val);
    neu_dvalue_init_move_array(resp_val, NEU_DTYPE_INT_VAL, array);

    node_vector_release(&datas);
    vector_uninit(&datas);
    return code;
}

opcua_error_code_e opcua_handle_write_value(opc_handle_context_t *context,
                                            neu_taggrp_config_t * config,
                                            neu_data_val_t *      write_val,
                                            neu_data_val_t *      resp_val)
{
    opcua_error_code_e code  = OPCUA_ERROR_SUCESS;
    neu_fixed_array_t *array = NULL;
    neu_dvalue_get_ref_array(write_val, &array);
    int size = array->length;

    vector_t datas = { 0 };
    vector_init(&datas, size, sizeof(opcua_data_t));
    write_request_vector_generate(context, config, array, &datas);

    int rc = open62541_client_is_connected(context->client);
    if (0 != rc) {
        code = OPCUA_ERROR_CLIENT_OFFLINE;
        log_error("open62541 client offline");
        SET_RESULT_ERRORCODE(resp_val, code);

        node_vector_release(&datas);
        vector_uninit(&datas);
        return code;
    }

    rc = open62541_client_write(context->client, &datas);
    if (0 != rc) {
        code = OPCUA_ERROR_WRITE_FAIL;
        log_error("open62541 write error");
        SET_RESULT_ERRORCODE(resp_val, code);

        node_vector_release(&datas);
        vector_uninit(&datas);
        return code;
    }

    neu_fixed_array_t *resp_array = NULL;
    resp_array = neu_fixed_array_new(size + 1, sizeof(neu_int_val_t));
    if (NULL == resp_array) {
        code = OPCUA_ERROR_ARRAY_NULL;
        log_error("Failed to allocate array for response tags");
        neu_dvalue_free(resp_val);

        node_vector_release(&datas);
        vector_uninit(&datas);
        return code;
    }

    rc                      = write_response_array_generate(&datas, resp_array);
    neu_int_val_t   int_val = { 0 };
    neu_data_val_t *val_err = NULL;
    val_err                 = neu_dvalue_new(NEU_DTYPE_ERRORCODE);
    neu_dvalue_set_errorcode(val_err, rc);
    neu_int_val_init(&int_val, 0, val_err);
    neu_fixed_array_set(resp_array, 0, (void *) &int_val);
    neu_dvalue_init_move_array(resp_val, NEU_DTYPE_INT_VAL, resp_array);

    node_vector_release(&datas);
    vector_uninit(&datas);
    return code;
}

static void group_config_read(opc_subscribe_tuple_t *tuple)
{
    neu_variable_t *head = neu_variable_create();
    if (NULL == head) {
        log_warn("Failed to allocate variable");
        return;
    }

    neu_data_val_t *resp_val = NULL;
    resp_val                 = neu_dvalue_unit_new();
    if (NULL == resp_val) {
        neu_variable_destroy(head);
        log_warn("Failed to allocate data value for response tags");
        return;
    }

    opcua_handle_read_once(tuple->context, tuple->config, resp_val);
    if (NULL != tuple->cycle_cb) {
        tuple->cycle_cb(tuple->plugin, tuple->config, resp_val);
    }

    neu_variable_destroy(head);
}

static void group_config_swap(opc_subscribe_tuple_t *tuple)
{
    neu_taggrp_config_t *new_config = neu_system_find_group_config(
        tuple->plugin, tuple->node_id, tuple->name);
    neu_taggrp_cfg_anchor(new_config);
    neu_taggrp_cfg_free(tuple->config);
    tuple->config = (neu_taggrp_config_t *) neu_taggrp_cfg_ref(new_config);
}

static void cycle_read(nng_aio *aio, void *arg, int code)
{
    opc_subscribe_tuple_t *tuple = (opc_subscribe_tuple_t *) arg;

    switch (code) {
    case NNG_ETIMEDOUT: {
        if (neu_taggrp_cfg_is_anchored(tuple->config)) {
            group_config_read(tuple);
        } else {
            group_config_swap(tuple); // Update config
        }

        uint32_t interval = neu_taggrp_cfg_get_interval(tuple->config);
        nng_aio_set_timeout(tuple->aio, interval != 0 ? interval : 10000);
        nng_aio_defer(aio, cycle_read, arg);
        break;
    }
    case NNG_ECANCELED:
        log_info("aio: %p cancel", aio);
        break;
    default:
        log_warn("aio: %p, skip error: %d", aio, code);
        break;
    }
}

static void cycle_read_start(opc_subscribe_tuple_t *tuple)
{
    uint32_t interval = neu_taggrp_cfg_get_interval(tuple->config);
    nng_aio_alloc(&tuple->aio, NULL, NULL);
    nng_aio_wait(tuple->aio);
    nng_aio_set_timeout(tuple->aio, interval != 0 ? interval : 10000);
    nng_aio_defer(tuple->aio, cycle_read, tuple);
}

static void cycle_read_stop(opc_subscribe_tuple_t *tuple)
{
    nng_aio_cancel(tuple->aio);
    nng_aio_free(tuple->aio);
    neu_taggrp_cfg_free(tuple->config);
}

static opc_subscribe_tuple_t *subscribe_tuple_new(opc_handle_context_t *context,
                                                  neu_taggrp_config_t * config)
{
    opc_subscribe_tuple_t *tuple = malloc(sizeof(opc_subscribe_tuple_t));
    if (NULL == tuple) {
        return NULL;
    }
    memset(tuple, 0, sizeof(opc_subscribe_tuple_t));

    tuple->plugin  = context->plugin;
    tuple->node_id = context->self_node_id;
    tuple->name    = strdup(neu_taggrp_cfg_get_name(config));
    tuple->config  = (neu_taggrp_config_t *) neu_taggrp_cfg_ref(config);
    tuple->context = context;
    return tuple;
}

static void subscribe_tuple_add(opc_handle_context_t * context,
                                opc_subscribe_tuple_t *tuple)
{
    NEU_LIST_NODE_INIT(&tuple->node);
    neu_list_append(&context->subscribe_list, tuple);
}

static opc_subscribe_tuple_t *
subscribe_tuple_find(opc_handle_context_t *context, neu_taggrp_config_t *config)
{
    opc_subscribe_tuple_t *iterator = NULL;
    opc_subscribe_tuple_t *tuple    = NULL;
    NEU_LIST_FOREACH(&context->subscribe_list, iterator)
    {
        if (iterator->config == config) {
            tuple = iterator;
        }
    }

    return tuple;
}

static void subscribe_tuple_release(opc_subscribe_tuple_t *tuple)
{
    if (NULL == tuple) {
        return;
    }

    if (NULL != tuple->name) {
        free(tuple->name);
    }

    free(tuple);
}

static void subscribe(opc_handle_context_t * context,
                      opc_subscribe_tuple_t *tuple)
{
    cycle_read_start(tuple);
    subscribe_tuple_add(context, tuple);
}

static void unsubscribe(opc_handle_context_t * context,
                        opc_subscribe_tuple_t *tuple)
{
    cycle_read_stop(tuple);
    neu_list_remove(&context->subscribe_list, tuple);
    subscribe_tuple_release(tuple);
}

opcua_error_code_e opcua_handle_subscribe(opc_handle_context_t *  context,
                                          neu_taggrp_config_t *   config,
                                          cycle_response_callback cycle_cb)
{
    opcua_error_code_e code = OPCUA_ERROR_SUCESS;

    opc_subscribe_tuple_t *tuple = NULL;
    tuple                        = subscribe_tuple_new(context, config);
    if (NULL == tuple) {
        return OPCUA_ERROR_ADD_SUBSCRIBE_FAIL;
    }

    tuple->cycle_cb = cycle_cb;
    subscribe(context, tuple);
    return code;
}

opcua_error_code_e opcua_handle_unsubscribe(opc_handle_context_t *context,
                                            neu_taggrp_config_t * config)
{
    opcua_error_code_e     code  = OPCUA_ERROR_SUCESS;
    opc_subscribe_tuple_t *tuple = subscribe_tuple_find(context, config);
    unsubscribe(context, tuple);
    return code;
}

opcua_error_code_e opcua_handle_stop(opc_handle_context_t *context)
{
    opcua_error_code_e code = OPCUA_ERROR_SUCESS;

    while (!neu_list_empty(&context->subscribe_list)) {
        opc_subscribe_tuple_t *tuple = NULL;
        tuple                        = neu_list_first(&context->subscribe_list);
        if (NULL != tuple) {
            unsubscribe(context, tuple);
        }
    }

    return code;
}
