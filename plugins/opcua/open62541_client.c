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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <open62541.h>

#include "open62541_client.h"
#include "open62541_util.h"

#define MAX_SERVER_URL_LENGTH 200

struct open62541_client {
    option_t *       option;
    pthread_mutex_t  mutex;
    pthread_t        thread;
    UA_Boolean       running;
    UA_Client *      ua_client;
    UA_ClientConfig *ua_config;
    int              connected;
    char             server_url[MAX_SERVER_URL_LENGTH];
    void *           user_data;
};

static int client_tcp_option_bind(open62541_client_t *client)
{
    UA_ClientConfig_setDefault(client->ua_config);
    return 0;
}

static int client_connect_option_bind(open62541_client_t *client)
{
    if (NULL == client->option->host) {
        return -1;
    }

    snprintf(client->server_url, MAX_SERVER_URL_LENGTH, "opc.tcp://%s:%s",
             client->option->host, client->option->port);
    client_tcp_option_bind(client);
    return 0;
}

open62541_client_t *open62541_client_create(option_t *option, void *context)
{
    if (NULL == option) {
        return NULL;
    }

    open62541_client_t *client =
        (open62541_client_t *) malloc(sizeof(open62541_client_t));
    if (NULL == client) {
        return NULL;
    }
    memset(client, 0, sizeof(open62541_client_t));

    pthread_mutex_init(&client->mutex, NULL);
    client->running           = true;
    client->connected         = -1;
    client->option            = option;
    client->user_data         = context;
    client->ua_client         = UA_Client_new();
    client->ua_config         = UA_Client_getConfig(client->ua_client);
    UA_LogLevel log_level     = UA_LOGLEVEL_ERROR;
    client->ua_config->logger = open62541_log_with_level(log_level);

    client_connect_option_bind(client);
    return client;
}

static void *client_refresh(void *context)
{
    open62541_client_t *client = (open62541_client_t *) context;
    UA_StatusCode       error  = 0;
    option_t *          option = client->option;

    UA_Boolean run_flag = true;
    while (1) {
        pthread_mutex_lock(&client->mutex);
        run_flag = client->running;
        pthread_mutex_unlock(&client->mutex);

        if (!run_flag) {
            break;
        }

        if (0 < strlen(option->username)) {
            pthread_mutex_lock(&client->mutex);
            error =
                UA_Client_connectUsername(client->ua_client, client->server_url,
                                          option->username, option->password);
            pthread_mutex_unlock(&client->mutex);
        }

        pthread_mutex_lock(&client->mutex);
        error = UA_Client_connect(client->ua_client, client->server_url);
        pthread_mutex_unlock(&client->mutex);

        if (UA_STATUSCODE_GOOD != error) {
            client->connected = -1;
            UA_sleep_ms(1000);
            continue;
        }

        pthread_mutex_lock(&client->mutex);
        UA_Client_run_iterate(client->ua_client, 0);
        pthread_mutex_unlock(&client->mutex);

        client->connected = 0;
        UA_sleep_ms(1000);
    }

    return NULL;
}

int open62541_client_connect(open62541_client_t *client)
{
    if (NULL == client) {
        return -1;
    }

    int rc =
        pthread_create(&client->thread, NULL, client_refresh, (void *) client);
    if (0 == rc) {
        log_error("Create reconnect thread succeed");
        return 0;
    }

    return -2;
}

int open62541_client_open(option_t *option, void *context,
                          open62541_client_t **p_client)
{
    open62541_client_t *client = open62541_client_create(option, context);
    if (NULL == client) {
        return -1;
    }

    int rc = open62541_client_connect(client);
    if (0 != rc) {
        return -2;
    }

    *p_client = client;
    return 0;
}

int open62541_client_is_connected(open62541_client_t *client)
{
    UA_StatusCode connect_status;
    pthread_mutex_lock(&client->mutex);
    UA_Client_getState(client->ua_client, NULL, NULL, &connect_status);
    pthread_mutex_unlock(&client->mutex);
    return connect_status;
}

int open62541_client_disconnect(open62541_client_t *client)
{
    if (0 == open62541_client_is_connected(client)) {
        pthread_mutex_lock(&client->mutex);
        UA_Client_disconnect(client->ua_client);
        pthread_mutex_unlock(&client->mutex);
    }

    return 0;
}

static void client_generate_read_value_id(opcua_data_t *data, int index,
                                          UA_ReadValueId *read_ids)
{
    UA_ReadValueId_init(&read_ids[index]);
    if (0 == data->opcua_node.identifier_type) {
        read_ids[index].nodeId = UA_NODEID_STRING_ALLOC(
            data->opcua_node.namespace_index, data->opcua_node.identifier_str);
    } else {
        read_ids[index].nodeId = UA_NODEID_NUMERIC(
            data->opcua_node.namespace_index, data->opcua_node.identifier_int);
    }

    read_ids[index].attributeId = UA_ATTRIBUTEID_VALUE;
}

static int client_set_read_value(opcua_data_t *data, UA_Variant *value)
{
    // sbyte(OPCUA) -> int8(Neuron)
    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_SBYTE])) {
        data->value.value_int8 = *(UA_SByte *) value->data;
        data->type             = NEU_DTYPE_INT8;
        return NEU_ERR_SUCCESS;
    }

    // byte(OPCUA) -> uint8(Neuron)
    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_BYTE])) {
        data->value.value_uint8 = *(UA_Byte *) value->data;
        data->type              = NEU_DTYPE_UINT8;
        return NEU_ERR_SUCCESS;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_BOOLEAN])) {
        data->value.value_boolean = *(UA_Boolean *) value->data;
        data->type                = NEU_DTYPE_BOOL;
        return NEU_ERR_SUCCESS;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_INT16])) {
        data->value.value_int16 = *(UA_Int16 *) value->data;
        data->type              = NEU_DTYPE_INT16;
        return NEU_ERR_SUCCESS;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_UINT16])) {
        data->value.value_uint16 = *(UA_UInt16 *) value->data;
        data->type               = NEU_DTYPE_UINT16;
        return NEU_ERR_SUCCESS;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_INT32])) {
        data->value.value_int32 = *(UA_Int32 *) value->data;
        data->type              = NEU_DTYPE_INT32;
        return NEU_ERR_SUCCESS;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_UINT32])) {
        data->value.value_uint32 = *(UA_UInt32 *) value->data;
        data->type               = NEU_DTYPE_UINT32;
        return NEU_ERR_SUCCESS;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_DATETIME])) {
        UA_DateTime time           = *(UA_DateTime *) value->data;
        data->value.value_datetime = (UA_UInt32) UA_DateTime_toUnixTime(time);
        data->type                 = NEU_DTYPE_UINT32;
        return NEU_ERR_SUCCESS;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_INT64])) {
        data->value.value_int64 = *(UA_Int64 *) value->data;
        data->type              = NEU_DTYPE_INT64;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_UINT64])) {
        data->value.value_uint64 = *(UA_UInt64 *) value->data;
        data->type               = NEU_DTYPE_UINT64;
        return NEU_ERR_SUCCESS;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_FLOAT])) {
        data->value.value_float = *(UA_Float *) value->data;
        data->type              = NEU_DTYPE_FLOAT;
        return NEU_ERR_SUCCESS;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        data->value.value_double = *(UA_Double *) value->data;
        data->type               = NEU_DTYPE_DOUBLE;
        return NEU_ERR_SUCCESS;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_STRING])) {
        UA_String str            = *(UA_String *) value->data;
        data->value.value_string = (char *) malloc(str.length + 1);
        memset(data->value.value_string, 0, str.length + 1);
        memcpy(data->value.value_string, str.data, str.length);
        data->type = NEU_DTYPE_CSTR;
        return NEU_ERR_SUCCESS;
    }

    return NEU_ERR_DEVICE_TAG_TYPE_NOT_MATCH; // no matched
}

int open62541_client_read(open62541_client_t *client, vector_t *datas)
{
    if (NULL == client) {
        return -1;
    }

    if (0 >= datas->size) {
        return -2;
    }

    UA_ReadRequest read_request = { 0 };
    UA_ReadRequest_init(&read_request);
    UA_ReadValueId *read_ids = NULL;
    read_ids = (UA_ReadValueId *) malloc(datas->size * sizeof(UA_ReadValueId));
    if (NULL == read_ids) {
        return -3;
    }

    int count = 0;
    VECTOR_FOR_EACH(datas, iter)
    {
        opcua_data_t *data = (opcua_data_t *) iterator_get(&iter);
        client_generate_read_value_id(data, count, read_ids);
        count++;
    }

    // Send read
    read_request.nodesToRead     = read_ids;
    read_request.nodesToReadSize = count;

    pthread_mutex_lock(&client->mutex);
    UA_ReadResponse read_response =
        UA_Client_Service_read(client->ua_client, read_request);
    pthread_mutex_unlock(&client->mutex);

    uint32_t status_code = read_response.responseHeader.serviceResult;
    if (UA_STATUSCODE_GOOD != status_code ||
        count != (int) read_response.resultsSize) {
        log_error("Respons status code:%x", status_code);
        return -4;
    }

    count = 0;
    VECTOR_FOR_EACH(datas, iter)
    {
        opcua_data_t *data = (opcua_data_t *) iterator_get(&iter);
        if (!read_response.results[count].hasValue) {
            data->error = NEU_ERR_DEVICE_TAG_NOT_EXIST;
            count++;
            continue;
        }

        if (!UA_Variant_isScalar(&read_response.results[count].value)) {
            data->error = NEU_ERR_DEVICE_TAG_TYPE_NOT_MATCH;
            count++;
            continue;
        }

        UA_Variant *value = &read_response.results[count].value;
        client_set_read_value(data, value);
        count++;
    }

    // Cleanup
    count = 0;
    VECTOR_FOR_EACH(datas, iter)
    {
        UA_ReadValueId_clear(&read_ids[count]);
        count++;
    }

    free(read_ids);
    UA_ReadResponse_clear(&read_response);
    return NEU_ERR_SUCCESS;
}

static int client_write(open62541_client_t *client, opcua_data_t *data)
{
    int             flag          = 0;
    UA_WriteRequest write_request = { 0 };
    UA_WriteRequest_init(&write_request);
    write_request.nodesToWrite     = UA_WriteValue_new();
    write_request.nodesToWriteSize = 1;

    if (0 == data->opcua_node.identifier_type) {
        write_request.nodesToWrite[0].nodeId = UA_NODEID_STRING_ALLOC(
            data->opcua_node.namespace_index, data->opcua_node.identifier_str);
        log_info("write data type:%d, namespace:%d, identifier_str:%s",
                 data->type, data->opcua_node.namespace_index,
                 data->opcua_node.identifier_str);

    } else {
        write_request.nodesToWrite[0].nodeId = UA_NODEID_NUMERIC(
            data->opcua_node.namespace_index, data->opcua_node.identifier_int);
    }

    write_request.nodesToWrite[0].attributeId    = UA_ATTRIBUTEID_VALUE;
    write_request.nodesToWrite[0].value.hasValue = true;
    write_request.nodesToWrite[0].value.value.storageType =
        UA_VARIANT_DATA_NODELETE;

    switch (data->type) {
    case NEU_DTYPE_INT8: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_SBYTE];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_int8;
        break;
    }
    case NEU_DTYPE_UINT8: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_BYTE];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_uint8;
        break;
    }
    case NEU_DTYPE_BOOL: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_BOOLEAN];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_boolean;
        break;
    }
    case NEU_DTYPE_INT16: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_INT16];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_int16;
        break;
    }
    case NEU_DTYPE_UINT16: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_UINT16];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_uint16;
        break;
    }
    case NEU_DTYPE_INT32: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_INT32];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_int32;
        break;
    }
    case NEU_DTYPE_UINT32: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_UINT32];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_int32;
        break;
    }
    case NEU_DTYPE_INT64: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_INT64];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_int64;
        break;
    }
    case NEU_DTYPE_UINT64: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_UINT64];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_uint64;
        break;
    }
    case NEU_DTYPE_FLOAT: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_FLOAT];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_float;
        break;
    }
    case NEU_DTYPE_DOUBLE: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_DOUBLE];
        write_request.nodesToWrite[0].value.value.data =
            &data->value.value_double;
        break;
    }
    case NEU_DTYPE_CSTR: {
        write_request.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_STRING];
        UA_String str = UA_STRING(data->value.value_string);
        write_request.nodesToWrite[0].value.value.data =
            UA_malloc(sizeof(UA_String));
        *((UA_String *) write_request.nodesToWrite[0].value.value.data) = str;
        break;
    }
    default: {
        flag++;
        break;
    }
    }

    if (0 != flag) {
        UA_WriteRequest_clear(&write_request);
        return NEU_ERR_DEVICE_TAG_TYPE_NOT_MATCH; // No match data type
    }

    pthread_mutex_lock(&client->mutex);
    UA_WriteResponse write_response =
        UA_Client_Service_write(client->ua_client, write_request);
    pthread_mutex_unlock(&client->mutex);

    if (NEU_DTYPE_CSTR == data->type) {
        UA_free(write_request.nodesToWrite[0].value.value.data);
    }

    if (write_response.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        UA_WriteRequest_clear(&write_request);
        UA_WriteResponse_clear(&write_response);
        return NEU_ERR_DEVICE_ABNORMALITY;
    }

    UA_WriteRequest_clear(&write_request);
    UA_WriteResponse_clear(&write_response);
    return NEU_ERR_SUCCESS;
}

int open62541_client_write(open62541_client_t *client, vector_t *datas)
{
    if (NULL == client) {
        return -1;
    }

    if (0 >= datas->size) {
        return -2;
    }

    int count = 0;
    VECTOR_FOR_EACH(datas, iter)
    {
        opcua_data_t *data = (opcua_data_t *) iterator_get(&iter);
        int           rc   = client_write(client, data);
        data->error        = rc;
        count++;
    }

    return 0;
}

int open62541_client_destroy(open62541_client_t *client)
{
    pthread_mutex_destroy(&client->mutex);

    if (NULL != client->ua_client) {
        UA_Client_delete(client->ua_client); // No lock-unlock
    }

    free(client);
    return 0;
}

int open62541_client_close(open62541_client_t *client)
{
    pthread_mutex_lock(&client->mutex);
    client->running = false;
    pthread_mutex_unlock(&client->mutex);

    pthread_join(client->thread, NULL);

    open62541_client_disconnect(client);
    open62541_client_destroy(client);
    return 0;
}