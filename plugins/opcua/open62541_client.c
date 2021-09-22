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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <open62541.h>

#include "open62541_client.h"

#define UNUSED(x) (void) (x)
#define MAX_SERVER_URL_LENGTH 200

struct subscribe_tuple {
    neu_list_node                node;
    char *                       name;
    bool                         subscribed;
    UA_MonitoredItemCreateResult result;
    open62541_client_t *         client;
};

struct open62541_client {
    option_t *                    option;
    UA_Client *                   ua_client;
    UA_ClientConfig *             ua_config;
    UA_CreateSubscriptionResponse subscription_response;
    char                          server_url[MAX_SERVER_URL_LENGTH];
    void *                        user_data;
    neu_list                      subscribe_list;
};

static int client_connect_option_bind(open62541_client_t *client)
{
    UNUSED(client);
    return 0;
}

static UA_ByteString client_load_file(const char *const path)
{
    UA_ByteString file_contents = UA_STRING_NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        errno = 0;
        return file_contents;
    }

    fseek(fp, 0, SEEK_END);
    file_contents.length = (size_t) ftell(fp);
    file_contents.data =
        (UA_Byte *) UA_malloc(file_contents.length * sizeof(UA_Byte));
    if (file_contents.data) {
        fseek(fp, 0, SEEK_SET);
        size_t read = fread(file_contents.data, sizeof(UA_Byte),
                            file_contents.length, fp);
        if (read != file_contents.length)
            UA_ByteString_clear(&file_contents);
    } else {
        file_contents.length = 0;
    }
    fclose(fp);

    return file_contents;
}

static int client_ssl_option_bind(open62541_client_t *client)
{
    UA_ByteString certificate;
    UA_ByteString privatekey;
    if (strlen(client->option->cert_file) > 0) {
        certificate = client_load_file(client->option->cert_file);
        privatekey  = client_load_file(client->option->key_file);
    } else {
        log_info("Load default cert and key");
        certificate = client_load_file(client->option->default_cert_file);
        privatekey  = client_load_file(client->option->default_key_file);
    }

    UA_ClientConfig_setDefaultEncryption(client->ua_config, certificate,
                                         privatekey, NULL, 0, NULL, 0);
    UA_ByteString_clear(&certificate);
    UA_ByteString_clear(&privatekey);

    UNUSED(certificate);
    UNUSED(privatekey);
    return 0;
}

static int client_field_init(open62541_client_t *client)
{
    if (NULL == client->option->host) {
        return -1;
    }

    NEU_LIST_INIT(&client->subscribe_list, subscribe_tuple_t, node);

    sprintf(client->server_url, "opc.tcp://%s:%d", client->option->host,
            client->option->port);
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

    client->option    = option;
    client->user_data = context;
    client->ua_client = UA_Client_new();
    client->ua_config = UA_Client_getConfig(client->ua_client);

    client_field_init(client);
    client_ssl_option_bind(client);
    client_connect_option_bind(client);
    UA_ClientConfig_setDefault(client->ua_config);

    return client;
}

static void delete_subscription_callback(UA_Client *client,
                                         UA_UInt32  subscription_id,
                                         void *     subscription_context)
{
    UNUSED(client);
    UNUSED(subscription_id);
    UNUSED(subscription_context);
    log_info("Subscription id %u was deleted", subscription_id);
}

static int client_create_subscription(open62541_client_t *client)
{
    UA_CreateSubscriptionRequest request;
    request                       = UA_CreateSubscriptionRequest_default();
    client->subscription_response = UA_Client_Subscriptions_create(
        client->ua_client, request, NULL, NULL, delete_subscription_callback);
    if (UA_STATUSCODE_GOOD !=
        client->subscription_response.responseHeader.serviceResult) {
        UA_Client_disconnect(client->ua_client);
        UA_Client_delete(client->ua_client);
        return -1;
    }
    log_info("Create subscription succeeded, id:%u",
             client->subscription_response.subscriptionId);

    return 0;
}

int open62541_client_connect(open62541_client_t *client)
{
    if (NULL == client) {
        return -1;
    }

    int rc = -1;
    if (0 < strlen(client->option->username)) {
        rc = UA_Client_connectUsername(client->ua_client, client->server_url,
                                       client->option->username,
                                       client->option->password);
        log_info("Connect with username");
    } else {
        rc = UA_Client_connect(client->ua_client, client->server_url);
        log_info("Connect without username");
    }

    if (UA_STATUSCODE_GOOD != rc) {
        log_error("Connection trial failure, code:%d", rc);
        return -1;
    }

    rc = client_create_subscription(client);
    if (0 != rc) {
        return -2;
    }

    return 0;
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
    UA_Client_getState(client->ua_client, NULL, NULL, &connect_status);
    return connect_status;
}

int open62541_client_disconnect(open62541_client_t *client)
{
    if (0 == open62541_client_is_connected(client)) {
        UA_Client_disconnect(client->ua_client);
    }

    UA_Client_Subscriptions_deleteSingle(
        client->ua_client, client->subscription_response.subscriptionId);
    return 0;
}

static void client_generate_read_value_id(opcua_data_t *data, int index,
                                          UA_ReadValueId *ids)
{
    UA_ReadValueId_init(&ids[index]);
    if (0 == data->opcua_node.identifier_type) {
        ids[index].nodeId = UA_NODEID_STRING_ALLOC(
            data->opcua_node.namespace_index, data->opcua_node.identifier_str);
    } else {
        ids[index].nodeId = UA_NODEID_NUMERIC(data->opcua_node.namespace_index,
                                              data->opcua_node.identifier_int);
    }

    ids[index].attributeId = UA_ATTRIBUTEID_VALUE;
}

static int client_set_read_value(opcua_data_t *data, UA_Variant *value)
{
    if (NULL == value) {
        return -1;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_SBYTE])) {
        data->value.value_int8 = *(UA_SByte *) value->data;
        data->type             = NEU_DATATYPE_BYTE;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_BYTE])) {
        data->value.value_uint8 = *(UA_Byte *) value->data;
        data->type              = NEU_DATATYPE_UBYTE;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_BOOLEAN])) {
        data->value.value_boolean = *(UA_Boolean *) value->data;
        data->type                = NEU_DATATYPE_BOOLEAN;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_INT16])) {
        data->value.value_int16 = *(UA_Int16 *) value->data;
        data->type              = NEU_DATATYPE_WORD;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_UINT16])) {
        data->value.value_uint16 = *(UA_UInt16 *) value->data;
        data->type               = NEU_DATATYPE_UWORD;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_INT32])) {
        data->value.value_int32 = *(UA_Int32 *) value->data;
        data->type              = NEU_DATATYPE_DWORD;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_UINT32])) {
        data->value.value_uint32 = *(UA_UInt32 *) value->data;
        data->type               = NEU_DATATYPE_UDWORD;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_DATETIME])) {
        UA_DateTime time           = *(UA_DateTime *) value->data;
        data->value.value_datetime = (UA_UInt32) UA_DateTime_toUnixTime(time);
        data->type                 = NEU_DATATYPE_DATETIME;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_FLOAT])) {
        data->value.value_float = *(UA_Float *) value->data;
        data->type              = NEU_DATATYPE_FLOAT;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_DOUBLE])) {
        data->value.value_double = *(UA_Double *) value->data;
        data->type               = NEU_DATATYPE_DOUBLE;
        return 0;
    }

    if (UA_Variant_hasScalarType(value, &UA_TYPES[UA_TYPES_STRING])) {
        UA_String str            = *(UA_String *) value->data;
        data->value.value_string = (char *) malloc(str.length + 1);
        memset(data->value.value_string, 0, str.length + 1);
        memcpy(data->value.value_string, str.data, str.length);
        data->type = NEU_DATATYPE_STRING;
        return 0;
    }

    return -2; // no matched
}

int open62541_client_read(open62541_client_t *client, vector_t *data)
{
    if (NULL == client) {
        return -1;
    }

    if (0 >= data->size) {
        return -2;
    }

    UA_ReadRequest req;
    UA_ReadRequest_init(&req);
    UA_ReadValueId *ids;
    ids = (UA_ReadValueId *) malloc(data->size * sizeof(UA_ReadValueId));
    if (NULL == ids) {
        return -3;
    }

    int count = 0;
    VECTOR_FOR_EACH(data, iter)
    {
        opcua_data_t *d = (opcua_data_t *) iterator_get(&iter);
        client_generate_read_value_id(d, count, ids);
        count++;
    }

    // Send read
    req.nodesToRead      = ids;
    req.nodesToReadSize  = count;
    UA_ReadResponse resp = UA_Client_Service_read(client->ua_client, req);
    uint32_t        status_code = resp.responseHeader.serviceResult;
    if (UA_STATUSCODE_GOOD != status_code || count != (int) resp.resultsSize) {
        log_error("Respons status code:%x", status_code);
        return -4;
    }

    count = 0;
    VECTOR_FOR_EACH(data, iter)
    {
        opcua_data_t *d = (opcua_data_t *) iterator_get(&iter);

        if (!resp.results[count].hasValue) {
            d->error = -1;
            continue;
        }
        if (!UA_Variant_isScalar(&resp.results[count].value)) {
            d->error = -2;
            continue;
        }

        UA_Variant *value = &resp.results[count].value;
        client_set_read_value(d, value);
        count++;
    }

    // Cleanup
    count = 0;
    VECTOR_FOR_EACH(data, iter)
    {
        UA_ReadValueId_clear(&ids[count]);
        count++;
    }

    free(ids);
    UA_ReadResponse_clear(&resp);
    return 0;
}

static int client_write(open62541_client_t *client, opcua_data_t *data)
{
    if (NULL == data) {
        return -1;
    }

    int             flag = 0;
    UA_WriteRequest write_req;
    UA_WriteRequest_init(&write_req);
    write_req.nodesToWrite     = UA_WriteValue_new();
    write_req.nodesToWriteSize = 1;

    if (0 == data->opcua_node.identifier_type) {
        write_req.nodesToWrite[0].nodeId = UA_NODEID_STRING_ALLOC(
            data->opcua_node.namespace_index, data->opcua_node.identifier_str);
        log_info("write data type:%d, namespace:%d, identifier_str:%s",
                 data->type, data->opcua_node.namespace_index,
                 data->opcua_node.identifier_str);

    } else {
        write_req.nodesToWrite[0].nodeId = UA_NODEID_NUMERIC(
            data->opcua_node.namespace_index, data->opcua_node.identifier_int);
    }

    write_req.nodesToWrite[0].attributeId    = UA_ATTRIBUTEID_VALUE;
    write_req.nodesToWrite[0].value.hasValue = true;
    write_req.nodesToWrite[0].value.value.storageType =
        UA_VARIANT_DATA_NODELETE;

    switch (data->type) {
    case NEU_DATATYPE_BYTE:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_SBYTE];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_int8;
        break;
    case NEU_DATATYPE_UBYTE:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_BYTE];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_uint8;
        break;
    case NEU_DATATYPE_BOOLEAN:
        write_req.nodesToWrite[0].value.value.type =
            &UA_TYPES[UA_TYPES_BOOLEAN];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_boolean;
        break;
    case NEU_DATATYPE_WORD:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_INT16];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_int16;
        break;
    case NEU_DATATYPE_UWORD:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_UINT16];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_uint16;
        break;
    case NEU_DATATYPE_DWORD:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_INT32];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_int32;
        break;
    case NEU_DATATYPE_UDWORD:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_UINT32];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_int32;
        break;
    case NEU_DATATYPE_QWORD:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_INT64];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_int64;
        break;
    case NEU_DATATYPE_UQWORD:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_UINT64];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_uint64;
        break;
    case NEU_DATATYPE_FLOAT:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_FLOAT];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_float;
        break;
    case NEU_DATATYPE_DOUBLE:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_DOUBLE];
        write_req.nodesToWrite[0].value.value.data = &data->value.value_double;
        break;
    case NEU_DATATYPE_STRING:
        write_req.nodesToWrite[0].value.value.type = &UA_TYPES[UA_TYPES_STRING];
        UA_String str = UA_STRING(data->value.value_string);
        write_req.nodesToWrite[0].value.value.data =
            UA_malloc(sizeof(UA_String));
        *((UA_String *) write_req.nodesToWrite[0].value.value.data) = str;
        break;

    default:
        flag++;
        break;
    }

    if (0 != flag) {
        UA_WriteRequest_clear(&write_req);
        return -2;
    }

    UA_WriteResponse write_resp =
        UA_Client_Service_write(client->ua_client, write_req);
    if (write_resp.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        UA_WriteRequest_clear(&write_req);
        UA_WriteResponse_clear(&write_resp);
        if (NEU_DATATYPE_STRING == data->type) {
            UA_free(write_req.nodesToWrite[0].value.value.data);
        }
        return -3;
    }
    UA_WriteRequest_clear(&write_req);
    UA_WriteResponse_clear(&write_resp);
    return 0;
}

int open62541_client_write(open62541_client_t *client, vector_t *data)
{
    if (NULL == client) {
        return -1;
    }

    if (0 >= data->size) {
        return -2;
    }

    int count = 0;
    VECTOR_FOR_EACH(data, iter)
    {
        opcua_data_t *d  = (opcua_data_t *) iterator_get(&iter);
        int           rc = client_write(client, d);
        d->error         = rc;
        count++;
    }

    return 0;
}

static void client_handler_node_changed(UA_Client *client, UA_UInt32 sub_Id,
                                        void *sub_context, UA_UInt32 mon_id,
                                        void *mon_context, UA_DataValue *value)
{
    UNUSED(client);
    UNUSED(sub_Id);
    UNUSED(sub_context);
    UNUSED(mon_id);
    UNUSED(mon_context);
    UNUSED(value);
}

int client_subscribe(open62541_client_t *client, opcua_node_t *node)
{
    UA_NodeId node_id;
    if (0 == node->identifier_type) {
        node_id =
            UA_NODEID_STRING_ALLOC(node->namespace_index, node->identifier_str);
    } else {
        node_id =
            UA_NODEID_NUMERIC(node->namespace_index, node->identifier_int);
    }

    char *name = "";

    UA_MonitoredItemCreateRequest monitor_request =
        UA_MonitoredItemCreateRequest_default(node_id);
    UA_MonitoredItemCreateResult result =
        UA_Client_MonitoredItems_createDataChange(
            client->ua_client, client->subscription_response.subscriptionId,
            UA_TIMESTAMPSTORETURN_BOTH, monitor_request, name,
            client_handler_node_changed, NULL);
    if (UA_STATUSCODE_GOOD != result.statusCode) {
        log_error("Monitoring %s fail", node->name);
        return -1;
    }

    subscribe_tuple_t *tuple =
        (subscribe_tuple_t *) malloc(sizeof(subscribe_tuple_t));
    tuple->name       = strdup(node->name);
    tuple->subscribed = 1;
    tuple->client     = client;
    tuple->result     = result;

    NEU_LIST_NODE_INIT(&tuple->node);
    neu_list_append(&client->subscribe_list, tuple);

    log_info("Monitoring %s succeeded, id:%u", node->name,
             result.monitoredItemId);

    return 0;
}

int open62541_client_subscribe(open62541_client_t *client, vector_t *node)
{
    if (NULL == client) {
        return -1;
    }

    if (NULL == node) {
        return -2;
    }

    int count = 0;
    VECTOR_FOR_EACH(node, iter)
    {
        opcua_node_t *n  = (opcua_node_t *) iterator_get(&iter);
        int           rc = client_subscribe(client, n);
        n->error         = rc;
        count++;
    }

    return 0;
}

static int client_unsubscribe(open62541_client_t *client, opcua_node_t *node)
{
    if (NULL == node) {
        return -1;
    }

    subscribe_tuple_t *item = NULL;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        if (0 == strcmp(item->name, node->name)) {
            break;
        }
    }

    if (NULL != item) {
        UA_MonitoredItemCreateResult_clear(&item->result);
        free(item->name);
        free(item);
    }

    return 0;
}

int open62541_client_unsubscribe(open62541_client_t *client, vector_t *node)
{
    if (NULL == client) {
        return -1;
    }

    if (NULL == node) {
        return -2;
    }

    int count = 0;
    VECTOR_FOR_EACH(node, iter)
    {
        opcua_node_t *n  = (opcua_node_t *) iterator_get(&iter);
        int           rc = client_unsubscribe(client, n);
        n->error         = rc;
        count++;
    }
    return 0;
}

int open62541_client_destroy(open62541_client_t *client)
{
    while (!neu_list_empty(&client->subscribe_list)) {
        subscribe_tuple_t *tuple = neu_list_first(&client->subscribe_list);
        neu_list_remove(&client->subscribe_list, tuple);
    }

    if (NULL != client->ua_client) {
        UA_Client_delete(client->ua_client);
    }

    free(client);
    return 0;
}

int open62541_client_close(open62541_client_t *client)
{
    open62541_client_disconnect(client);
    open62541_client_destroy(client);
    return 0;
}