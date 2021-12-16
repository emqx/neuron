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

#include <open62541.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNUSED(x) (void) (x)

static volatile UA_Boolean running = true;

static void node_attributes_set(UA_Server *server, char *name, char *node_id,
                                int type, void *data, const int readonly)
{
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_Variant_setScalar(&attr.value, data, &UA_TYPES[type]);
    attr.description = UA_LOCALIZEDTEXT("en-US", name);
    attr.displayName = UA_LOCALIZEDTEXT("en-US", name);
    attr.dataType    = UA_TYPES[type].typeId;

    if (readonly) {
        attr.accessLevel = UA_ACCESSLEVELMASK_READ;
    } else {
        attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    }

    UA_NodeId        integer_node_id = UA_NODEID_STRING(1, node_id);
    UA_QualifiedName integer_name    = UA_QUALIFIEDNAME(1, name);
    UA_NodeId parent_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parent_reference_node_id =
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(
        server, integer_node_id, parent_node_id, parent_reference_node_id,
        integer_name, UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr,
        NULL, NULL);
}

// UA_SByte <-> int8_t
static void data_int8_add(UA_Server *server, char *name, char *node_id,
                          int8_t data, const int readonly)
{
    UA_SByte sbyte = data;
    node_attributes_set(server, name, node_id, UA_TYPES_SBYTE, &sbyte,
                        readonly);
}

// UA_Byte <-> uint8_t
static void data_uint8_add(UA_Server *server, char *name, char *node_id,
                           uint8_t data, const int readonly)
{
    UA_Byte byte = data;
    node_attributes_set(server, name, node_id, UA_TYPES_BYTE, &byte, readonly);
}

// UA_Boolean
static void data_bool_add(UA_Server *server, char *name, char *node_id,
                          bool data, const int readonly)
{
    UA_Boolean boolean = data;
    node_attributes_set(server, name, node_id, UA_TYPES_BOOLEAN, &boolean,
                        readonly);
}

// UA_Int16
static void data_int16_add(UA_Server *server, char *name, char *node_id,
                           int16_t data, const int readonly)
{
    UA_Int16 i16 = data;
    node_attributes_set(server, name, node_id, UA_TYPES_INT16, &i16, readonly);
}

// UA_UInt16
static void data_uint16_add(UA_Server *server, char *name, char *node_id,
                            int16_t data, const int readonly)
{
    UA_UInt16 u16 = data;
    node_attributes_set(server, name, node_id, UA_TYPES_UINT16, &u16, readonly);
}

// UA_Int32
static void data_int32_add(UA_Server *server, char *name, char *node_id,
                           int32_t data, const int readonly)
{
    UA_Int32 i32 = data;
    node_attributes_set(server, name, node_id, UA_TYPES_INT32, &i32, readonly);
}

// UA_UInt32
static void data_uint32_add(UA_Server *server, char *name, char *node_id,
                            uint32_t data, const int readonly)
{
    UA_UInt32 u32 = data;
    node_attributes_set(server, name, node_id, UA_TYPES_UINT32, &u32, readonly);
}

// UA_Int64
static void data_int64_add(UA_Server *server, char *name, char *node_id,
                           int64_t data, const int readonly)
{
    UA_Int64 i64 = data;
    node_attributes_set(server, name, node_id, UA_TYPES_INT64, &i64, readonly);
}

// UA_UInt64
static void data_uint64_add(UA_Server *server, char *name, char *node_id,
                            uint64_t data, const int readonly)
{
    UA_UInt64 u64 = data;
    node_attributes_set(server, name, node_id, UA_TYPES_UINT64, &u64, readonly);
}

// UA_Float
static void data_float_add(UA_Server *server, char *name, char *node_id,
                           float data, const int readonly)
{
    UA_Float f32 = data;
    node_attributes_set(server, name, node_id, UA_TYPES_FLOAT, &f32, readonly);
}

// UA_Double
static void data_double_add(UA_Server *server, char *name, char *node_id,
                            double data, const int readonly)
{
    UA_Double f64 = data;
    node_attributes_set(server, name, node_id, UA_TYPES_DOUBLE, &f64, readonly);
}

static void data_string_add(UA_Server *server, char *name, char *node_id,
                            char *data, const int readonly)
{
    UA_String str = UA_STRING(data);
    node_attributes_set(server, name, node_id, UA_TYPES_STRING, &str, readonly);
}

static UA_StatusCode
current_time_read(UA_Server *server, const UA_NodeId *sessionId,
                  void *sessionContext, const UA_NodeId *nodeId,
                  void *nodeContext, UA_Boolean sourceTimeStamp,
                  const UA_NumericRange *range, UA_DataValue *dataValue)
{
    UNUSED(server);
    UNUSED(sessionId);
    UNUSED(sessionContext);
    UNUSED(nodeId);
    UNUSED(nodeContext);
    UNUSED(sourceTimeStamp);
    UNUSED(range);

    UA_DateTime now = UA_DateTime_now();
    UA_Variant_setScalarCopy(&dataValue->value, &now,
                             &UA_TYPES[UA_TYPES_DATETIME]);
    dataValue->hasValue = true;
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
current_time_write(UA_Server *server, const UA_NodeId *sessionId,
                   void *sessionContext, const UA_NodeId *nodeId,
                   void *nodeContext, const UA_NumericRange *range,
                   const UA_DataValue *data)
{
    UNUSED(server);
    UNUSED(sessionId);
    UNUSED(sessionContext);
    UNUSED(nodeId);
    UNUSED(nodeContext);
    UNUSED(range);
    UNUSED(data);

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Changing the system time is not implemented");
    return UA_STATUSCODE_BADINTERNALERROR;
}

static void current_time_datasource_variable_add(UA_Server *server)
{
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName           = UA_LOCALIZEDTEXT("en-US", "type_datetime");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_NodeId        currentNodeId = UA_NODEID_STRING(1, "neu.type_datetime");
    UA_QualifiedName currentName   = UA_QUALIFIEDNAME(1, "type_datetime");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_NodeId variableTypeNodeId =
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);

    UA_DataSource timeDataSource;
    timeDataSource.read  = current_time_read;
    timeDataSource.write = current_time_write;
    UA_Server_addDataSourceVariableNode(
        server, currentNodeId, parentNodeId, parentReferenceNodeId, currentName,
        variableTypeNodeId, attr, timeDataSource, NULL, NULL);
}

int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    UA_ServerNetworkLayer nl =
        UA_ServerNetworkLayerTCP(UA_ConnectionConfig_default, 4840, 12);
    UA_Server *server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));
    UA_ServerConfig *config        = UA_Server_getConfig(server);
    config->networkLayers          = &nl;
    config->networkLayersSize      = 1;
    config->verifyRequestTimestamp = UA_RULEHANDLING_ACCEPT;

    static UA_UsernamePasswordLogin logins[2] = {
        { UA_STRING_STATIC("user"), UA_STRING_STATIC("123456") },
    };

    UA_AccessControl_default(
        config, false,
        &config->securityPolicies[config->securityPoliciesSize - 1].policyUri,
        2, logins);

    data_int8_add(server, "type_int8", "neu.type_int8", -10, 0);
    data_uint8_add(server, "type_uint8", "neu.type_uint8", 10, 0);
    data_bool_add(server, "type_bool", "neu.type_bool", true, 0);
    data_int16_add(server, "type_int16", "neu.type_int16", -1001, 0);
    data_uint16_add(server, "type_uint16", "neu.type_uint16", 1001, 0);
    data_int32_add(server, "type_int32", "neu.type_int32", -100, 0);
    data_uint32_add(server, "type_uint32", "neu.type_uint32", 111111, 0);
    data_int64_add(server, "type_int64", "neu.type_int64", -10011111111111, 0);
    data_uint64_add(server, "type_uint64", "neu.type_uint64", 5678710019198784,
                    0);
    data_float_add(server, "type_float", "neu.type_float", 38.9, 0);
    data_double_add(server, "type_double", "neu.type_double", 100038.9999, 0);
    data_string_add(server, "type_cstr", "neu.type_cstr", "hello world!", 0);

    data_string_add(server, "str01", "cstr01", "hello world 1!", 0);
    data_string_add(server, "str02", "cstr02", "hello world 2!", 0);
    data_string_add(server, "str03", "cstr03", "hello world 3!", 0);
    data_string_add(server, "字符串10", "字符串10", "中文字符串测试 9!", 0);
    data_string_add(server, "字符串十一", "\"测试\".\"字符串十一\"",
                    "中文字符串测试 十一!", 0);

    current_time_datasource_variable_add(server);

    UA_StatusCode retval = UA_Server_run(server, &running);
    UNUSED(retval);
    UA_Server_delete(server);
    return 0;
}
