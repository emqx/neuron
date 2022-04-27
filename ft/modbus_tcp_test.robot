*** Settings ***
Resource          common.resource
Suite Setup       Neuron Context Ready
Suite Teardown    Stop All Processes

*** Variables ***
${test_node_id}
${test_node_name}   modbus-tcp-adapter
${group}    config_modbus_tcp_sample_2

${tag_bit}       {"name": "tag_bit", "address": "1!000001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
${tag_int16}     {"name": "tag_int16", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
${tag_uint16}    {"name": "tag_uint16", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
${tag_int32}     {"name": "tag_int32", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
${tag_uint32}    {"name": "tag_uint32", "address": "1!400005", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
${tag_float}     {"name": "tag_float", "address": "1!400007", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

${tag1_bit}       {"name": "tag1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
${tag2_bit}       {"name": "tag2", "address": "1!100002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
${tag1_int16}     {"name": "tag1", "address": "1!301021", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
${tag2_int16}     {"name": "tag2", "address": "1!301022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
${tag1_uint16}    {"name": "tag1", "address": "1!301019", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}
${tag2_uint16}    {"name": "tag2", "address": "1!301020", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}
${tag1_int32}     {"name": "tag1", "address": "1!301010", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT32}}
${tag2_int32}     {"name": "tag2", "address": "1!301011", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT32}}
${tag1_uint32}    {"name": "tag1", "address": "1!301017", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
${tag2_uint32}    {"name": "tag2", "address": "1!301018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
${tag1_float}     {"name": "tag1", "address": "1!300009", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
${tag2_float}     {"name": "tag2", "address": "1!300008", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}

*** Test Cases ***
Read unsubscribed point, it should return failure
    Skip If Not Http API
    ${res} =                Add Tags    ${test_node_id}    xxgrp    ${tag_int16}
    Sleep                   1s 500ms

    ${res} =                 Read Tags          ${test_node_name}                    xxgrp
    Check Response Status    ${res}             200
    Integer                  $.tags[0].error    ${ERR_PLUGIN_GRP_NOT_SUBSCRIBE}

    ${res} =                 Del Group Config    ${test_node_id}    xxgrp
    Check Response Status    ${res}              200
    Check Error Code         ${res}              ${ERR_SUCCESS}

Read/Write a point in the hold reg or coil area, and the data as bit/int16/uint16/int32/uint32/float type, it should return success
    [Template]         Read Write A Point In The hold/coil Reg Area Should Success
    ${test_node_id}    ${test_node_name}                tag_bit               ${group}                                                       ${tag_bit}       coil        bit       1
    ${test_node_id}    ${test_node_name}                tag_int16             ${group}                                                       ${tag_int16}     hold reg    int16     123
    ${test_node_id}    ${test_node_name}                tag_uint16            ${group}                                                       ${tag_uint16}    hold reg    uint16    456
    ${test_node_id}    ${test_node_name}                tag_int32             ${group}                                                       ${tag_int32}     hold reg    int32     66778899
    ${test_node_id}    ${test_node_name}                tag_uint32            ${group}                                                       ${tag_uint32}    hold reg    int32     667788995
    ${test_node_id}    ${test_node_name}                tag_float             ${group}                                                       ${tag_float}     hold reg    float     11.22

Read a point in the input or input reg area, and the data as bit/int16/uint16/int32/uint32/float type, it should return success
    [Template]         Read A Point In The Input Or Input Reg Area Should Return Success
    ${test_node_id}    ${test_node_name}                        ${group}                                                             input        bit       ${tag1_bit}       0                        ${tag2_bit}       1
    ${test_node_id}    ${test_node_name}                        ${group}                                                             input reg    int16     ${tag1_int16}     1020                     ${tag2_int16}     1021
    ${test_node_id}    ${test_node_name}                        ${group}                                                             input reg    uint16    ${tag1_uint16}    1018                     ${tag2_uint16}    1019
    ${test_node_id}    ${test_node_name}                        ${group}                                                             input reg    int32     ${tag1_int32}     66126834                 ${tag2_int32}     66191360
    ${test_node_id}    ${test_node_name}                        ${group}                                                             input reg    uint32    ${tag1_uint32}    66585593                 ${tag2_uint32}    66650112
    ${test_node_id}    ${test_node_name}                        ${group}                                                             input reg    float     ${tag1_float}     7.346839692639297e-40    ${tag2_float}     6.428596834936531e-40

Read multiple points in the coil area, the point address includes continuous and non-continuous, and the data as bit type, it should return success
    ${tag1_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!000001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag2_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!000002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag3_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!000010", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag4_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!000012", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag5_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!000107", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag6_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!000108", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag7_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!001022", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag8_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!001020", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}

    Should Not Be Equal As Integers    ${tag1_id}    -1
    Should Not Be Equal As Integers    ${tag2_id}    -1
    Should Not Be Equal As Integers    ${tag3_id}    -1
    Should Not Be Equal As Integers    ${tag4_id}    -1
    Should Not Be Equal As Integers    ${tag5_id}    -1
    Should Not Be Equal As Integers    ${tag6_id}    -1
    Should Not Be Equal As Integers    ${tag7_id}    -1
    Should Not Be Equal As Integers    ${tag8_id}    -1

    Sleep       1s 500ms
    ${res} =    Read Tags    ${test_node_name}    ${group}

    Compare Tag Value As Int    ${res}[tags]    tag1    1
    Compare Tag Value As Int    ${res}[tags]    tag2    0
    Compare Tag Value As Int    ${res}[tags]    tag3    0
    Compare Tag Value As Int    ${res}[tags]    tag4    0
    Compare Tag Value As Int    ${res}[tags]    tag5    0
    Compare Tag Value As Int    ${res}[tags]    tag6    0
    Compare Tag Value As Int    ${res}[tags]    tag7    0
    Compare Tag Value As Int    ${res}[tags]    tag8    0

    Write Tags    ${test_node_name}    ${group}    tag2     1
    Write Tags    ${test_node_name}    ${group}    tag3     1
    Write Tags    ${test_node_name}    ${group}    tag4     1
    Write Tags    ${test_node_name}    ${group}    tag5     1
    Write Tags    ${test_node_name}    ${group}    tag7     1

    Sleep       1s 500ms
    ${res} =    Read Tags    ${test_node_name}    ${group}

    Compare Tag Value As Int    ${res}[tags]    tag1    1
    Compare Tag Value As Int    ${res}[tags]    tag2    1
    Compare Tag Value As Int    ${res}[tags]    tag3    1
    Compare Tag Value As Int    ${res}[tags]    tag4    1
    Compare Tag Value As Int    ${res}[tags]    tag5    1
    Compare Tag Value As Int    ${res}[tags]    tag6    0
    Compare Tag Value As Int    ${res}[tags]    tag7    1
    Compare Tag Value As Int    ${res}[tags]    tag8    0

    [Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id}

Read multiple points in the input area, the point address includes continuous and non-continuous, and the data as bit type, it should return success
    ${tag1_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag2_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!100002", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag3_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!100010", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag4_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!100012", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag5_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!100107", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag6_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!100108", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag7_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!101022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag8_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!101020", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}

    Should Not Be Equal As Integers    ${tag1_id}    -1
    Should Not Be Equal As Integers    ${tag2_id}    -1
    Should Not Be Equal As Integers    ${tag3_id}    -1
    Should Not Be Equal As Integers    ${tag4_id}    -1
    Should Not Be Equal As Integers    ${tag5_id}    -1
    Should Not Be Equal As Integers    ${tag6_id}    -1
    Should Not Be Equal As Integers    ${tag7_id}    -1
    Should Not Be Equal As Integers    ${tag8_id}    -1

    Sleep       1s 500ms
    ${res} =    Read Tags    ${test_node_name}    ${group}

    Compare Tag Value As Int    ${res}[tags]    tag1    0
    Compare Tag Value As Int    ${res}[tags]    tag2    1
    Compare Tag Value As Int    ${res}[tags]    tag3    1
    Compare Tag Value As Int    ${res}[tags]    tag4    1
    Compare Tag Value As Int    ${res}[tags]    tag5    0
    Compare Tag Value As Int    ${res}[tags]    tag6    1
    Compare Tag Value As Int    ${res}[tags]    tag7    1
    Compare Tag Value As Int    ${res}[tags]    tag8    1

    [Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id}

Read multiple points in the input reg area, the point address includes continuous and non-continuous, and the data includes int16/uint16/int32/uint32/float type, it should return success
    ${tag1_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!300001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    ${tag2_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!300002", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    ${tag3_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!300010", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}
    ${tag4_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!300012", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}
    ${tag5_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!300107", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT32}}
    ${tag6_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!300110", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    ${tag7_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!301018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    ${tag8_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!301020", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}


    Should Not Be Equal As Integers    ${tag1_id}    -1
    Should Not Be Equal As Integers    ${tag2_id}    -1
    Should Not Be Equal As Integers    ${tag3_id}    -1
    Should Not Be Equal As Integers    ${tag4_id}    -1
    Should Not Be Equal As Integers    ${tag5_id}    -1
    Should Not Be Equal As Integers    ${tag6_id}    -1
    Should Not Be Equal As Integers    ${tag7_id}    -1
    Should Not Be Equal As Integers    ${tag8_id}    -1

    Sleep       1s 500ms
    ${res} =    Read Tags    ${test_node_name}    ${group}

    Compare Tag Value As Int      ${res}[tags]    tag1    0
    Compare Tag Value As Int      ${res}[tags]    tag2    1
    Compare Tag Value As Int      ${res}[tags]    tag3    9
    Compare Tag Value As Int      ${res}[tags]    tag4    11
    Compare Tag Value As Int      ${res}[tags]    tag5    6946923
    Compare Tag Value As Int      ${res}[tags]    tag6    7143534
    Compare Tag Value As Int      ${res}[tags]    tag7    66651130
    Compare Tag Value As Float    ${res}[tags]    tag8    1.475336887045722e-36

    [Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id}

Read multiple points in the hold reg area, the point address includes continuous and non-continuous, and the data includes int16/uint16/int32/uint32/float type, it should return success
    ${tag1_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
    ${tag2_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
    ${tag3_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!400010", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    ${tag4_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!400012", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    ${tag5_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!400107", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    ${tag6_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!400110", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
    ${tag7_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!401018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
    ${tag8_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!401020", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

    Should Not Be Equal As Integers    ${tag1_id}    -1
    Should Not Be Equal As Integers    ${tag2_id}    -1
    Should Not Be Equal As Integers    ${tag3_id}    -1
    Should Not Be Equal As Integers    ${tag4_id}    -1
    Should Not Be Equal As Integers    ${tag5_id}    -1
    Should Not Be Equal As Integers    ${tag6_id}    -1
    Should Not Be Equal As Integers    ${tag7_id}    -1
    Should Not Be Equal As Integers    ${tag8_id}    -1

    Sleep       1s 500ms
    ${res} =    Read Tags    ${test_node_name}    ${group}

    Compare Tag Value As Int      ${res}[tags]    tag1    123
    Compare Tag Value As Int      ${res}[tags]    tag2    456
    Compare Tag Value As Int      ${res}[tags]    tag3    0
    Compare Tag Value As Int      ${res}[tags]    tag4    0
    Compare Tag Value As Int      ${res}[tags]    tag5    0
    Compare Tag Value As Int      ${res}[tags]    tag6    0
    Compare Tag Value As Int      ${res}[tags]    tag7    0
    Compare Tag Value As Float    ${res}[tags]    tag8    0.0

    Write Tags    ${test_node_name}    ${group}         tag1          1
    Write Tags    ${test_node_name}    ${group}         tag2          2
    Write Tags    ${test_node_name}    ${group}         tag3          10
    Write Tags    ${test_node_name}    ${group}         tag4          12
    Write Tags    ${test_node_name}    ${group}         tag5          6946924
    Write Tags    ${test_node_name}    ${group}         tag6          7143536
    Write Tags    ${test_node_name}    ${group}         tag7          66651131
    Write Tags    ${test_node_name}    ${group}         tag8          11.234

    Sleep       1s 500ms
    ${res} =    Read Tags    ${test_node_name}    ${group}

    Compare Tag Value As Int      ${res}[tags]    tag1    1
    Compare Tag Value As Int      ${res}[tags]    tag2    2
    Compare Tag Value As Int      ${res}[tags]    tag3    10
    Compare Tag Value As Int      ${res}[tags]    tag4    12
    Compare Tag Value As Int      ${res}[tags]    tag5    6946924
    Compare Tag Value As Int      ${res}[tags]    tag6    7143536
    Compare Tag Value As Int      ${res}[tags]    tag7    66651131
    Compare Tag Value As Float    ${res}[tags]    tag8    11.234

    [Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id}

Read multiple points belonging to different areas(coil/input/input reg/hold reg), the point address includes continuous and non-continuous, and the data include bit/int16/uint16/int32/uint32/float type, it should return success
    ${tag1_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!000008", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag2_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!000009", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag3_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!000011", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag4_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!100008", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag5_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!300008", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    ${tag6_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!300009", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    ${tag7_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!300012", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
    ${tag8_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!400009", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    ${tag9_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag9", "address": "1!400020", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    ${tag10_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag10", "address": "1!400015", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

    Should Not Be Equal As Integers    ${tag1_id}     -1
    Should Not Be Equal As Integers    ${tag2_id}     -1
    Should Not Be Equal As Integers    ${tag3_id}     -1
    Should Not Be Equal As Integers    ${tag4_id}     -1
    Should Not Be Equal As Integers    ${tag5_id}     -1
    Should Not Be Equal As Integers    ${tag6_id}     -1
    Should Not Be Equal As Integers    ${tag7_id}     -1
    Should Not Be Equal As Integers    ${tag8_id}     -1
    Should Not Be Equal As Integers    ${tag9_id}     -1
    Should Not Be Equal As Integers    ${tag10_id}    -1

    Sleep       1s 500ms
    ${res} =    Read Tags    ${test_node_name}    ${group}

    Compare Tag Value As Int      ${res}[tags]    tag1     0
    Compare Tag Value As Int      ${res}[tags]    tag2     0
    Compare Tag Value As Int      ${res}[tags]    tag3     0
    Compare Tag Value As Int      ${res}[tags]    tag4     1
    Compare Tag Value As Int      ${res}[tags]    tag5     7
    Compare Tag Value As Int      ${res}[tags]    tag6     524297
    Compare Tag Value As Float    ${res}[tags]    tag7     1.010207273319475e-39
    Compare Tag Value As Int      ${res}[tags]    tag8     0
    Compare Tag Value As Int      ${res}[tags]    tag9     0
    Compare Tag Value As Float    ${res}[tags]    tag10    0.0

    Write Tags    ${test_node_name}    ${group}     tag1        1
    Write Tags    ${test_node_name}    ${group}     tag2        1
    Write Tags    ${test_node_name}    ${group}     tag3        1
    Write Tags    ${test_node_name}    ${group}     tag8        62225
    Write Tags    ${test_node_name}    ${group}     tag9        66651134
    Write Tags    ${test_node_name}    ${group}     tag10       11.123

    Sleep       1s 500ms
    ${res} =    Read Tags    ${test_node_name}    ${group}

    Compare Tag Value As Int      ${res}[tags]    tag1     1
    Compare Tag Value As Int      ${res}[tags]    tag2     1
    Compare Tag Value As Int      ${res}[tags]    tag3     1
    Compare Tag Value As Int      ${res}[tags]    tag4     1
    Compare Tag Value As Int      ${res}[tags]    tag5     7
    Compare Tag Value As Int      ${res}[tags]    tag6     524297
    Compare Tag Value As Float    ${res}[tags]    tag7     1.010207273319475e-39
    Compare Tag Value As Int      ${res}[tags]    tag8     62225
    Compare Tag Value As Int      ${res}[tags]    tag9     66651134
    Compare Tag Value As Float    ${res}[tags]    tag10    11.123

    [Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id}

Read multiple points belonging to different areas(coil/input/input reg/hold reg) and different sites, the point address includes continuous and non-continuous, and the data include int16/uint16/int32/uint32/float type, it should return success
    ${tag1_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!000018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag2_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "2!000018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag3_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "3!000021", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag4_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "4!100018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    ${tag5_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "5!300018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    ${tag6_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "6!300018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    ${tag7_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "7!300022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
    ${tag8_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "8!400018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    ${tag9_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag9", "address": "9!400040", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    ${tag10_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag10", "address": "10!400022", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

    Should Not Be Equal As Integers    ${tag1_id}     -1
    Should Not Be Equal As Integers    ${tag2_id}     -1
    Should Not Be Equal As Integers    ${tag3_id}     -1
    Should Not Be Equal As Integers    ${tag4_id}     -1
    Should Not Be Equal As Integers    ${tag5_id}     -1
    Should Not Be Equal As Integers    ${tag6_id}     -1
    Should Not Be Equal As Integers    ${tag7_id}     -1
    Should Not Be Equal As Integers    ${tag8_id}     -1
    Should Not Be Equal As Integers    ${tag9_id}     -1
    Should Not Be Equal As Integers    ${tag10_id}    -1

    Sleep       1s 500ms
    ${res} =    Read Tags    ${test_node_name}    ${group}

    Compare Tag Value As Int      ${res}[tags]    tag1     0
    Compare Tag Value As Int      ${res}[tags]    tag2     0
    Compare Tag Value As Int      ${res}[tags]    tag3     0
    Compare Tag Value As Int      ${res}[tags]    tag4     1
    Compare Tag Value As Int      ${res}[tags]    tag5     17
    Compare Tag Value As Int      ${res}[tags]    tag6     1114130
    Compare Tag Value As Float    ${res}[tags]    tag7     1.928576247884031e-39
    Compare Tag Value As Int      ${res}[tags]    tag8     0
    Compare Tag Value As Int      ${res}[tags]    tag9     0
    Compare Tag Value As Float    ${res}[tags]    tag10    0.0

    Write Tags    ${test_node_name}    ${group}         tag1            1
    Write Tags    ${test_node_name}    ${group}         tag2            1
    Write Tags    ${test_node_name}    ${group}         tag3            1
    Write Tags    ${test_node_name}    ${group}         tag8            62226
    Write Tags    ${test_node_name}    ${group}         tag9            66651136
    Write Tags    ${test_node_name}    ${group}         tag10           11.789

    Sleep       1s 500ms
    ${res} =    Read Tags    ${test_node_name}    ${group}

    Compare Tag Value As Int      ${res}[tags]    tag1     1
    Compare Tag Value As Int      ${res}[tags]    tag2     1
    Compare Tag Value As Int      ${res}[tags]    tag3     1
    Compare Tag Value As Int      ${res}[tags]    tag4     1
    Compare Tag Value As Int      ${res}[tags]    tag5     17
    Compare Tag Value As Int      ${res}[tags]    tag6     1114130
    Compare Tag Value As Float    ${res}[tags]    tag7     1.928576247884031e-39
    Compare Tag Value As Int      ${res}[tags]    tag8     62226
    Compare Tag Value As Int      ${res}[tags]    tag9     66651136
    Compare Tag Value As Float    ${res}[tags]    tag10    11.789

    [Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id}


# Recv multiple points belonging to different areas(coil/input/input reg/hold reg) and different sites, the point address includes continuous and non-continuous, and the data include int16/uint16/int32/uint32/float type, it should return success
#     Skip If Not MQTT API

#     ${tag1_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!000018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
#     ${tag2_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "2!000018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
#     ${tag3_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "3!000021", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
#     ${tag4_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "4!100018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
#     ${tag5_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "5!300018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
#     ${tag6_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "6!300018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
#     ${tag7_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "7!300022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
#     ${tag8_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "8!400018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
#     ${tag9_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag9", "address": "9!400040", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
#     ${tag10_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag10", "address": "10!400022", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

#     Should Not Be Equal As Integers    ${tag1_id}     -1
#     Should Not Be Equal As Integers    ${tag2_id}     -1
#     Should Not Be Equal As Integers    ${tag3_id}     -1
#     Should Not Be Equal As Integers    ${tag4_id}     -1
#     Should Not Be Equal As Integers    ${tag5_id}     -1
#     Should Not Be Equal As Integers    ${tag6_id}     -1
#     Should Not Be Equal As Integers    ${tag7_id}     -1
#     Should Not Be Equal As Integers    ${tag8_id}     -1
#     Should Not Be Equal As Integers    ${tag9_id}     -1
#     Should Not Be Equal As Integers    ${tag10_id}    -1

#     Subscribe           ${TOPIC_UPLOAD}      qos=0                 timeout=5
#     Sleep               1s 500ms
#     ${res} =            Recv Message         ${TOPIC_UPLOAD}
#     Log                 ${res}[tags]

#     Compare Tag Value As Int      ${res}[tags]    tag1     1
#     Compare Tag Value As Int      ${res}[tags]    tag2     1
#     Compare Tag Value As Int      ${res}[tags]    tag3     1
#     Compare Tag Value As Int      ${res}[tags]    tag4     1
#     Compare Tag Value As Int      ${res}[tags]    tag5     17
#     Compare Tag Value As Int      ${res}[tags]    tag6     1114130
#     Compare Tag Value As Float    ${res}[tags]    tag7     1.928576247884031e-39
#     Compare Tag Value As Int      ${res}[tags]    tag8     62226
#     Compare Tag Value As Int      ${res}[tags]    tag9     66651136
#     Compare Tag Value As Float    ${res}[tags]    tag10    11.789

#     [Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id}

*** Keywords ***
Neuron Context Ready
    Import Neuron API Resource

    Start Simulator    ${MODBUS_TCP_SERVER_SIMULATOR}    ${SIMULATOR_DIR}
    Neuron Ready

    LOGIN

    Add Node        type=${${NODE_DRIVER}}    name=${test_node_name}    plugin_name=modbus-tcp
    ${id} =         Get Node ID               ${NODE_DRIVER}             modbus-tcp-adapter
    ${mqtt_id} =    Get Node ID               ${NODE_MQTT}               mqtt-adapter

    Set Global Variable    ${test_node_id}    ${id}

    Node Setting        ${test_node_id}    ${MODBUS_TCP_CONFIG}
    Add Group Config    ${test_node_id}    ${group}                300
    Subscribe Group     ${test_node_id}    ${mqtt_id}              ${group}

Stop All Processes
    Stop Neuron                remove_persistence_data=True
    sleep                      1s
    Terminate All Processes    kill=false

Read Write A Point In The hold/coil Reg Area Should Success
    [Arguments]       ${node_id}        ${node_name}        ${tag_name}                                                                        ${group}    ${tag}    ${area}    ${type}    ${value}
    Log               Read a point in the ${area} reg area, and the data as ${type}, it should return success
    Log To Console    Read a point in the ${area} reg area, and the data as ${type}, it should return success

    IF          "${type}" == "float"
    ${cmp} =    Set Variable            Compare Tag Value As Float
    ELSE
    ${cmp} =    Set Variable            Compare Tag Value As Int
    END

    ${tag_id} =                        Add Tag And Return ID    ${node_id}    ${group}    ${tag}
    Should Not Be Equal As Integers    ${tag_id}                -1

    Sleep                    1s 500ms
    ${res} =                 Read Tags    ${node_name}      ${group}
    Check Response Status    ${res}       200
  #Check Error Code          ${res}                          ${ERR_SUCCESS}
    Run Keyword              ${cmp}       ${res}[tags]    ${res}[tags][0][name]    0

    ${res} =    Write Tags    ${node_name}    ${group}      ${tag_name}       ${value}

    Sleep                    1s 500ms
    ${res} =                 Read Tags    ${node_name}      ${group}
    Check Response Status    ${res}       200
  #Check Error Code          ${res}                          ${ERR_SUCCESS}
    Run Keyword              ${cmp}       ${res}[tags]    ${res}[tags][0][name]    ${value}


    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read A Point In The Input Or Input Reg Area Should Return Success
    [Arguments]       ${node_id}        ${node_name}                                                                       ${group}    ${area}    ${type}    ${tag1}    ${value1}    ${tag2}    ${value2}
    Log               Read a point in the ${area} area, and the data as ${type}, it should return success
    Log To Console    Read a point in the ${area} area, and the data as ${type}, it should return success

    IF          "${type}" == "float"
    ${cmp} =    Set Variable            Compare Tag Value As Float
    ELSE
    ${cmp} =    Set Variable            Compare Tag Value As Int
    END

    ${tag1_id} =                       Add Tag And Return ID    ${node_id}    ${group}    ${tag1}
    Should Not Be Equal As Integers    ${tag1_id}               -1

    ${tag2_id} =                       Add Tag And Return ID    ${node_id}    ${group}    ${tag2}
    Should Not Be Equal As Integers    ${tag2_id}               -1

    Sleep                    1s 500ms
    ${res} =                 Read Tags    ${node_name}    ${group}
    Check Response Status    ${res}       200
  #Check Error Code          ${res}                          ${ERR_SUCCESS}

    Run Keyword    ${cmp}    ${res}[tags]    ${res}[tags][0][name]    ${value1}
    Run Keyword    ${cmp}    ${res}[tags]    ${res}[tags][1][name]    ${value2}

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id}

Del Tags Check
    [Arguments]              ${node_id}    ${group}          ${tag_id}
    ${res} =                 Del Tags      ${node_id}        ${group}     ${tag_id}
    Check Response Status    ${res}        200
    Check Error Code         ${res}        ${ERR_SUCCESS}
