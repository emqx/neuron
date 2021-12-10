*** Settings ***
Resource          api_http.resource
Suite Setup       Neuron Context Ready 
Suite Teardown    Stop All Processes

*** Variables ***
${node_id}
${group}    config_opcua_sample

*** Test Cases ***
Read a point with data type of int8, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_int8", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT8}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     -10

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of uint8, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_uint8", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT8}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =    Read Tags    ${node_id}    ${group}

    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    10

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of bool, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_bool", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BOOL}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     1

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of int16, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_int16", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     -1001

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of uint16, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_uint16", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     1001

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of int32, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_int32", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT32}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     -100

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of uint32, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_uint32", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     111111

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of int64, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_int64", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT64}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     -10011111111111

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of uint64, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_uint64", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT64}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     5678710019198784

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of float, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_float", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                     Read Tags       ${node_id}    ${group}
    Compare Tag Value As Float    ${tags}.tags    ${tag_id}     38.9

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of double, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_double", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_DOUBLE}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                     Read Tags       ${node_id}    ${group}
    Compare Tag Value As Float    ${tags}.tags    ${tag_id}     100038.9999

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read a point with data type of string, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_cstr", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_STRING}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    ${tags} =                      Read Tags       ${node_id}    ${group}
    Compare Tag Value As String    ${tags}.tags    ${tag_id}     hello world!

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read multiple points, including multiple data types(int8/uint8/int16/uint16/int32/uint32/int64/uint64/float/double/string), it should return success
    Add Tags    ${node_id}    ${group}    {"name": "type_int8", "address": "1!neu.type_int8", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT8}}
    Add Tags    ${node_id}    ${group}    {"name": "type_uint8", "address": "1!neu.type_uint8", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT8}}
    Add Tags    ${node_id}    ${group}    {"name": "type_bool", "address": "1!neu.type_bool", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BOOL}}
    Add Tags    ${node_id}    ${group}    {"name": "type_int16", "address": "1!neu.type_int16", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    Add Tags    ${node_id}    ${group}    {"name": "type_uint16", "address": "1!neu.type_uint16", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}
    Add Tags    ${node_id}    ${group}    {"name": "type_int32", "address": "1!neu.type_int32", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT32}}
    Add Tags    ${node_id}    ${group}    {"name": "type_uint32", "address": "1!neu.type_uint32", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    Add Tags    ${node_id}    ${group}    {"name": "type_int64", "address": "1!neu.type_int64", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT64}}
    Add Tags    ${node_id}    ${group}    {"name": "type_uint64", "address": "1!neu.type_uint64", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT64}}
    Add Tags    ${node_id}    ${group}    {"name": "type_float", "address": "1!neu.type_float", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
    Add Tags    ${node_id}    ${group}    {"name": "type_double", "address": "1!neu.type_double", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_DOUBLE}}
    Add Tags    ${node_id}    ${group}    {"name": "type_cstr", "address": "1!neu.type_cstr", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_STRING}}

    ${tag_id_int8} =      Get Tag ID    ${node_id}    ${group}    type_int8
    ${tag_id_uint8} =     Get Tag ID    ${node_id}    ${group}    type_uint8
    ${tag_id_bool} =      Get Tag ID    ${node_id}    ${group}    type_bool
    ${tag_id_int16} =     Get Tag ID    ${node_id}    ${group}    type_int16
    ${tag_id_uint16} =    Get Tag ID    ${node_id}    ${group}    type_uint16
    ${tag_id_int32} =     Get Tag ID    ${node_id}    ${group}    type_int32
    ${tag_id_uint32} =    Get Tag ID    ${node_id}    ${group}    type_uint32
    ${tag_id_int64} =     Get Tag ID    ${node_id}    ${group}    type_int64
    ${tag_id_uint64} =    Get Tag ID    ${node_id}    ${group}    type_uint64
    ${tag_id_float} =     Get Tag ID    ${node_id}    ${group}    type_float
    ${tag_id_double} =    Get Tag ID    ${node_id}    ${group}    type_double
    ${tag_id_cstr} =      Get Tag ID    ${node_id}    ${group}    type_cstr

    should not be equal as integers    ${tag_id_int8}      -1
    should not be equal as integers    ${tag_id_uint8}     -1
    should not be equal as integers    ${tag_id_bool}      -1
    should not be equal as integers    ${tag_id_int16}     -1
    should not be equal as integers    ${tag_id_uint16}    -1
    should not be equal as integers    ${tag_id_int32}     -1
    should not be equal as integers    ${tag_id_uint32}    -1
    should not be equal as integers    ${tag_id_int64}     -1
    should not be equal as integers    ${tag_id_uint64}    -1
    should not be equal as integers    ${tag_id_float}     -1
    should not be equal as integers    ${tag_id_double}    -1
    should not be equal as integers    ${tag_id_cstr}      -1
    Sleep                              4s

    ${tags} =                      Read Tags       ${node_id}          ${group}
    Compare Tag Value As Int       ${tags}.tags    ${tag_id_int8}      -10
    Compare Tag Value As Int       ${tags}.tags    ${tag_id_uint8}     10
    Compare Tag Value As Int       ${tags}.tags    ${tag_id_bool}      1
    Compare Tag Value As Int       ${tags}.tags    ${tag_id_int16}     -1001
    Compare Tag Value As Int       ${tags}.tags    ${tag_id_uint16}    1001
    Compare Tag Value As Int       ${tags}.tags    ${tag_id_int32}     -100
    Compare Tag Value As Int       ${tags}.tags    ${tag_id_uint32}    111111
    Compare Tag Value As Int       ${tags}.tags    ${tag_id_int64}     -10011111111111
    Compare Tag Value As Int       ${tags}.tags    ${tag_id_uint64}    5678710019198784
    Compare Tag Value As Float     ${tags}.tags    ${tag_id_float}     38.9
    Compare Tag Value As Float     ${tags}.tags    ${tag_id_double}    100038.9999
    Compare Tag Value As String    ${tags}.tags    ${tag_id_cstr}      hello world!

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id_int8},${tag_id_uint8},${tag_id_bool},${tag_id_int16},${tag_id_uint16},${tag_id_int32},${tag_id_uint32},${tag_id_int64},${tag_id_uint64},${tag_id_float},${tag_id_double},${tag_id_cstr}

Write a point with data type of int8, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_int8", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT8}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": -9}
    Sleep         4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     -9

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of uint8, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_uint8", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT8}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 66}
    Sleep         4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     66

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of bool, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_bool", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BOOL}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              -1

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": false}
    Sleep         4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     0

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of int16, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_int16", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": -1000}
    Sleep         4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     -1000

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of uint16, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_uint16", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 999}
    Sleep         4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     999

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of int32, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_int32", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 99}
    Sleep         4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     99

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of uint32, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_uint32", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 222221}
    Sleep         4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     222221

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of int64, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_int64", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT64}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 10011111111111}
    Sleep         4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     10011111111111

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of uint64, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_uint64", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT64}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 5678710019198785}
    Sleep         4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}     5678710019198785

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of float, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_float", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 12.345}
    Sleep         4s

    ${tags} =                     Read Tags       ${node_id}    ${group}
    Compare Tag Value As Float    ${tags}.tags    ${tag_id}     12.345

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of double, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_double", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_DOUBLE}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 100099.8888}
    Sleep         4s

    ${tags} =                     Read Tags       ${node_id}    ${group}
    Compare Tag Value As Float    ${tags}.tags    ${tag_id}     100099.88

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write a point with data type of string, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_cstr", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}

    ${tag_id} =                        Get Tag ID    ${node_id}    ${group}    tag1
    should not be equal as integers    ${tag_id}     -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": "Hello Neuron!"}
    Sleep         4s

    ${tags} =                      Read Tags       ${node_id}    ${group}
    Compare Tag Value As String    ${tags}.tags    ${tag_id}     Hello Neuron!

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Write multiple points, including multiple data types(int8/uint8/int16/uint16/int32/uint32/int64/uint64/float/double/string), it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!neu.type_int8", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT8}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!neu.type_uint8", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT8}}
    Add Tags    ${node_id}    ${group}    {"name": "tag3", "address": "1!neu.type_bool", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BOOL}}
    Add Tags    ${node_id}    ${group}    {"name": "tag4", "address": "1!neu.type_int16", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag5", "address": "1!neu.type_uint16", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag6", "address": "1!neu.type_int32", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag7", "address": "1!neu.type_uint32", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag8", "address": "1!neu.type_int64", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT64}}
    Add Tags    ${node_id}    ${group}    {"name": "tag9", "address": "1!neu.type_uint64", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT64}}
    Add Tags    ${node_id}    ${group}    {"name": "tag10", "address": "1!neu.type_float", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag11", "address": "1!neu.type_double", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_DOUBLE}}
    Add Tags    ${node_id}    ${group}    {"name": "tag12", "address": "1!neu.type_cstr", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}

    ${tag1_id} =     Get Tag ID    ${node_id}    ${group}    tag1
    ${tag2_id} =     Get Tag ID    ${node_id}    ${group}    tag2
    ${tag3_id} =     Get Tag ID    ${node_id}    ${group}    tag3
    ${tag4_id} =     Get Tag ID    ${node_id}    ${group}    tag4
    ${tag5_id} =     Get Tag ID    ${node_id}    ${group}    tag5
    ${tag6_id} =     Get Tag ID    ${node_id}    ${group}    tag6
    ${tag7_id} =     Get Tag ID    ${node_id}    ${group}    tag7
    ${tag8_id} =     Get Tag ID    ${node_id}    ${group}    tag8
    ${tag9_id} =     Get Tag ID    ${node_id}    ${group}    tag9
    ${tag10_id} =    Get Tag ID    ${node_id}    ${group}    tag10
    ${tag11_id} =    Get Tag ID    ${node_id}    ${group}    tag11
    ${tag12_id} =    Get Tag ID    ${node_id}    ${group}    tag12

    should not be equal as integers    ${tag1_id}     -1
    should not be equal as integers    ${tag2_id}     -1
    should not be equal as integers    ${tag3_id}     -1
    should not be equal as integers    ${tag4_id}     -1
    should not be equal as integers    ${tag5_id}     -1
    should not be equal as integers    ${tag6_id}     -1
    should not be equal as integers    ${tag7_id}     -1
    should not be equal as integers    ${tag8_id}     -1
    should not be equal as integers    ${tag9_id}     -1
    should not be equal as integers    ${tag10_id}    -1
    should not be equal as integers    ${tag11_id}    -1
    should not be equal as integers    ${tag12_id}    -1
    Sleep                              4s

    Write Tags    ${node_id}    ${group}    {"id": ${tag1_id}, "value": 8}
    Write Tags    ${node_id}    ${group}    {"id": ${tag2_id}, "value": 7}
    Write Tags    ${node_id}    ${group}    {"id": ${tag3_id}, "value": true}
    Write Tags    ${node_id}    ${group}    {"id": ${tag4_id}, "value": -999}
    Write Tags    ${node_id}    ${group}    {"id": ${tag5_id}, "value": 888}
    Write Tags    ${node_id}    ${group}    {"id": ${tag6_id}, "value": 77}
    Write Tags    ${node_id}    ${group}    {"id": ${tag7_id}, "value": 123456}
    Write Tags    ${node_id}    ${group}    {"id": ${tag8_id}, "value": -10011111111112}
    Write Tags    ${node_id}    ${group}    {"id": ${tag9_id}, "value": 123456789101112}
    Write Tags    ${node_id}    ${group}    {"id": ${tag10_id}, "value": 33.33}
    Write Tags    ${node_id}    ${group}    {"id": ${tag11_id}, "value": 123456.7890}
    Write Tags    ${node_id}    ${group}    {"id": ${tag12_id}, "value": "Hello Strangers!!"}

    ${tags} =                      Read Tags       ${node_id}     ${group}
    Compare Tag Value As Int       ${tags}.tags    ${tag1_id}     8
    Compare Tag Value As Int       ${tags}.tags    ${tag2_id}     7
    Compare Tag Value As Int       ${tags}.tags    ${tag3_id}     1
    Compare Tag Value As Int       ${tags}.tags    ${tag4_id}     -999
    Compare Tag Value As Int       ${tags}.tags    ${tag5_id}     888
    Compare Tag Value As Int       ${tags}.tags    ${tag6_id}     77
    Compare Tag Value As Int       ${tags}.tags    ${tag7_id}     123456
    Compare Tag Value As Int       ${tags}.tags    ${tag8_id}     -10011111111112
    Compare Tag Value As Int       ${tags}.tags    ${tag9_id}     123456789101112
    Compare Tag Value As Float     ${tags}.tags    ${tag10_id}    33.33
    Compare Tag Value As Float     ${tags}.tags    ${tag11_id}    123456.7890
    Compare Tag Value As String    ${tags}.tags    ${tag12_id}    Hello Strangers!!

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id},${tag11_id},${tag12_id}

*** Keywords ***
Neuron Context Ready
    Start Simulator    ${OPCUA_SERVER_SIMULATOR}    ${SIMULATOR_DIR}

	Neuron Ready
	${token} =      LOGIN
	${jwt} =        Catenate                       Bearer    ${token} 
	Set Headers     {"Authorization": "${jwt}"}

	${id} =                Get Node ID    ${NODE_DRIVER}     opcua-adapter
	Set Global Variable    ${node_id}     ${id}
	Node Setting           ${node_id}     ${OPCUA_CONFIG}

Stop All Processes
	LOGOUT

	Stop Neuron

    Sleep    1s

	Terminate All Processes    kill=false
