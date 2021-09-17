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

static void generate_read_result_variable(vector_t *data, neu_variable_t *array)
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

static void generate_write_node_vector(opc_handle_context_t *context,
                                       neu_variable_t *array, vector_t *datas)
{
    char *key = NULL;
    for (neu_variable_t *cursor = array; NULL != cursor;
         cursor                 = neu_variable_next(cursor)) {
        int type = neu_variable_get_type(cursor);
        neu_variable_get_key(cursor, &key);
        int tag_id = atoi(key);

        neu_datatag_t *datatag = get_datatag_by_id(context->table, tag_id);
        if (NULL == datatag) {
            continue;
        }

        char namespace_str[16]  = { 0 };
        char identifier_str[32] = { 0 };
        int  identifier_type    = 0;
        fixed_address(datatag->addr_str, &identifier_type, namespace_str,
                      identifier_str);

        opcua_data_t write_data;
        write_data.opcua_node.name            = datatag->name;
        write_data.opcua_node.id              = tag_id;
        write_data.opcua_node.namespace_index = atoi(namespace_str);
        write_data.opcua_node.identifier_type = identifier_type;
        if (0 == identifier_type) {
            write_data.opcua_node.identifier_str = strdup(identifier_str);
        } else if (1 == identifier_type) {
            write_data.opcua_node.identifier_int = atoi(identifier_str);
        }

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

        vector_push_back(datas, &write_data);
    }
}

int plugin_handle_read_once(opc_handle_context_t *context,
                            neu_taggrp_config_t *config, neu_variable_t *array)
{
    vector_t data;
    vector_init(&data, 5, sizeof(opcua_data_t));
    generate_read_node_vector(context, config, &data);

    int rc = open62541_client_is_connected(context->client);
    if (0 != rc) {
        vector_uninit(&data);
        return -1;
    }

    rc = open62541_client_read(context->client, &data);
    generate_read_result_variable(&data, array);
    vector_uninit(&data);
    return 0;
}

int plugin_handle_write_value(opc_handle_context_t *context,
                              neu_variable_t *array, vector_t *data)
{
    generate_write_node_vector(context, array, data);

    int rc = open62541_client_is_connected(context->client);
    if (0 != rc) {
        return -1;
    }

    rc = open62541_client_write(context->client, data);
    return 0;
}