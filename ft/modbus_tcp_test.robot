*** Settings ***
Resource          api_http.resource
Resource          common.resource
Suite Setup       Neuron Context Ready
Suite Teardown    Stop All Processes

*** Variables ***
${MODBUS_PLUS_TCP_CONFIG}    {"host": "127.0.0.1", "port": 60502, "connection_mode": 0, "timeout": 3000}
${test_node_id}
${test_node_name}    modbus-plus-tcp-adapter
${group}             config_modbus_tcp_sample_2

${tag_bit}       {"name": "tag_bit", "address": "1!000001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
${tag_int16}     {"name": "tag_int16", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
${tag_uint16}    {"name": "tag_uint16", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
${tag_int32}     {"name": "tag_int32", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
${tag_uint32}    {"name": "tag_uint32", "address": "1!400005", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
${tag_float}     {"name": "tag_float", "address": "1!400007", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}
${tag_string}    {"name": "tag_string", "address": "1!400009.10", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}

${tag1_bit}       {"name": "tag1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
${tag2_bit}       {"name": "tag2", "address": "1!100002", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
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

#Read a point in the input or input reg area, and the data as bit/int16/uint16/int32/uint32/float/string type, it should return success
    #[Template]         Read A Point In The Input Or Input Reg Area Should Return Success
    #${test_node_id}    ${test_node_name}                                                    ${group}    input        bit       ${tag1_bit}       0      ${tag2_bit}       0
    #${test_node_id}    ${test_node_name}                                                    ${group}    input reg    int16     ${tag1_int16}     0      ${tag2_int16}     0
    #${test_node_id}    ${test_node_name}                                                    ${group}    input reg    uint16    ${tag1_uint16}    0      ${tag2_uint16}    0
    #${test_node_id}    ${test_node_name}                                                    ${group}    input reg    int32     ${tag1_int32}     0      ${tag2_int32}     0
    #${test_node_id}    ${test_node_name}                                                    ${group}    input reg    uint32    ${tag1_uint32}    0      ${tag2_uint32}    0
    #${test_node_id}    ${test_node_name}                                                    ${group}    input reg    float     ${tag1_float}     0.0    ${tag2_float}     0.0

#Read/Write multiple points in the coil area, the point address includes continuous and non-continuous, and the data as bit type, it should return success
    #${tag1_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!000001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag2_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!000002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag3_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!000010", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag4_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!000012", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag5_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!000107", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag6_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!000108", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag7_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!001022", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag8_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!001020", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag9_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag9", "address": "1!0000001.0", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}

    #Should Not Be Equal As Integers    ${tag1_id}    -1
    #Should Not Be Equal As Integers    ${tag2_id}    -1
    #Should Not Be Equal As Integers    ${tag3_id}    -1
    #Should Not Be Equal As Integers    ${tag4_id}    -1
    #Should Not Be Equal As Integers    ${tag5_id}    -1
    #Should Not Be Equal As Integers    ${tag6_id}    -1
    #Should Not Be Equal As Integers    ${tag7_id}    -1
    #Should Not Be Equal As Integers    ${tag8_id}    -1
    #Should Not Be Equal AS Integers    ${tag9_id}    -1

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int    ${res}[tags]    tag1    1
    #Compare Tag Value As Int    ${res}[tags]    tag2    0
    #Compare Tag Value As Int    ${res}[tags]    tag3    0
    #Compare Tag Value As Int    ${res}[tags]    tag4    0
    #Compare Tag Value As Int    ${res}[tags]    tag5    0
    #Compare Tag Value As Int    ${res}[tags]    tag6    0
    #Compare Tag Value As Int    ${res}[tags]    tag7    0
    #Compare Tag Value As Int    ${res}[tags]    tag8    0
    #Compare Tag Value As Int    ${res}[tags]    tag9    1

    #Write Tags    ${test_node_name}    ${group}    tag2    1
    #Write Tags    ${test_node_name}    ${group}    tag3    1
    #Write Tags    ${test_node_name}    ${group}    tag4    1
    #Write Tags    ${test_node_name}    ${group}    tag5    1
    #Write Tags    ${test_node_name}    ${group}    tag7    1

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int    ${res}[tags]    tag1    1
    #Compare Tag Value As Int    ${res}[tags]    tag2    1
    #Compare Tag Value As Int    ${res}[tags]    tag3    1
    #Compare Tag Value As Int    ${res}[tags]    tag4    1
    #Compare Tag Value As Int    ${res}[tags]    tag5    1
    #Compare Tag Value As Int    ${res}[tags]    tag6    0
    #Compare Tag Value As Int    ${res}[tags]    tag7    1
    #Compare Tag Value As Int    ${res}[tags]    tag8    0
    #Compare Tag Value As Int    ${res}[tags]    tag9    1

    #[Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id}

#Read multiple points in the input area, the point address includes continuous and non-continuous, and the data as bit type, it should return success
    #${tag1_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag2_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!100002", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag3_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!100010", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag4_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!100012", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag5_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!100107", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag6_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!100108", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag7_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!101022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag8_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!101020", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag9_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag9", "address": "1!1000001.1", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}

    #Should Not Be Equal As Integers    ${tag1_id}    -1
    #Should Not Be Equal As Integers    ${tag2_id}    -1
    #Should Not Be Equal As Integers    ${tag3_id}    -1
    #Should Not Be Equal As Integers    ${tag4_id}    -1
    #Should Not Be Equal As Integers    ${tag5_id}    -1
    #Should Not Be Equal As Integers    ${tag6_id}    -1
    #Should Not Be Equal As Integers    ${tag7_id}    -1
    #Should Not Be Equal As Integers    ${tag8_id}    -1
    #Should Not Be Equal AS Integers    ${tag9_id}    -1

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int    ${res}[tags]    tag1    0
    #Compare Tag Value As Int    ${res}[tags]    tag2    0
    #Compare Tag Value As Int    ${res}[tags]    tag3    0
    #Compare Tag Value As Int    ${res}[tags]    tag4    0
    #Compare Tag Value As Int    ${res}[tags]    tag5    0
    #Compare Tag Value As Int    ${res}[tags]    tag6    0
    #Compare Tag Value As Int    ${res}[tags]    tag7    0
    #Compare Tag Value As Int    ${res}[tags]    tag8    0
    #Compare Tag Value As Int    ${res}[tags]    tag9    0

    #[Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id}

#Read multiple points in the input reg area, the point address includes continuous and non-continuous, and the data includes int16/uint16/int32/uint32/float type, it should return success
    #${tag1_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!300001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    #${tag2_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!300002", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    #${tag3_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!300010", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}
    #${tag4_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!300012", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}
    #${tag5_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!300107", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT32}}
    #${tag6_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!300115", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT32}}
    #${tag7_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!300110", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    #${tag8_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!301018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    #${tag9_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag9", "address": "1!301020", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag10_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag10", "address": "1!301022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag11_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag11", "address": "1!301024.10", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_STRING}}
    #${tag12_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag12", "address": "1!301034.10", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_STRING}}
    #${tag13_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag13", "address": "1!301020.0", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag14_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag14", "address": "1!301020.1", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}

    #Should Not Be Equal As Integers    ${tag1_id}     -1
    #Should Not Be Equal As Integers    ${tag2_id}     -1
    #Should Not Be Equal As Integers    ${tag3_id}     -1
    #Should Not Be Equal As Integers    ${tag4_id}     -1
    #Should Not Be Equal As Integers    ${tag5_id}     -1
    #Should Not Be Equal As Integers    ${tag6_id}     -1
    #Should Not Be Equal As Integers    ${tag7_id}     -1
    #Should Not Be Equal As Integers    ${tag8_id}     -1
    #Should Not Be Equal As Integers    ${tag9_id}     -1
    #Should Not Be Equal As Integers    ${tag10_id}    -1
    #Should Not Be Equal As Integers    ${tag11_id}    -1
    #Should Not Be Equal As Integers    ${tag12_id}    -1
    #Should Not Be Equal As Integers    ${tag13_id}    -1
    #Should Not Be Equal As Integers    ${tag14_id}    -1

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int      ${res}[tags]    tag1     0
    #Compare Tag Value As Int      ${res}[tags]    tag2     0
    #Compare Tag Value As Int      ${res}[tags]    tag3     0
    #Compare Tag Value As Int      ${res}[tags]    tag4     0
    #Compare Tag Value As Int      ${res}[tags]    tag5     0
    #Compare Tag Value As Int      ${res}[tags]    tag6     0
    #Compare Tag Value As Int      ${res}[tags]    tag7     0
    #Compare Tag Value As Int      ${res}[tags]    tag8     0
    #Compare Tag Value As Float    ${res}[tags]    tag9     0.0
    #Compare Tag Value As Float    ${res}[tags]    tag10    0
   ## Compare Tag Value As String    ${res}[tags]    tag11    0
   ## Compare Tag Value As String    ${res}[tags]    tag12    0
    #Compare Tag Value As Int      ${res}[tags]    tag13    0
    #Compare Tag Value As Int      ${res}[tags]    tag14    0

    #[Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id},${tag11_id},${tag12_id},${tag13_id},${tag14_id}

#Read/Write multiple points in the hold reg area, the point address includes continuous and non-continuous, and the data includes int16/uint16/int32/uint32/float type, it should return success
    #${tag1_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
    #${tag2_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
    #${tag3_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!400010", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    #${tag4_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!400012", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    #${tag5_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!400107", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    #${tag6_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!400120", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    #${tag7_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!400110", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
    #${tag8_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!401018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
    #${tag9_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag9", "address": "1!401020", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag10_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag10", "address": "1!401022", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag11_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag11", "address": "1!401024.10", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}
    #${tag12_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag12", "address": "1!401034.10", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}
    #${tag13_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag13", "address": "1!401020.0", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag14_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag14", "address": "1!401020.1", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}

    #Should Not Be Equal As Integers    ${tag1_id}     -1
    #Should Not Be Equal As Integers    ${tag2_id}     -1
    #Should Not Be Equal As Integers    ${tag3_id}     -1
    #Should Not Be Equal As Integers    ${tag4_id}     -1
    #Should Not Be Equal As Integers    ${tag5_id}     -1
    #Should Not Be Equal As Integers    ${tag6_id}     -1
    #Should Not Be Equal As Integers    ${tag7_id}     -1
    #Should Not Be Equal As Integers    ${tag8_id}     -1
    #Should Not Be Equal As Integers    ${tag9_id}     -1
    #Should Not Be Equal As Integers    ${tag10_id}    -1
    #Should Not Be Equal As Integers    ${tag11_id}    -1
    #Should Not Be Equal As Integers    ${tag12_id}    -1
    #Should Not Be Equal As Integers    ${tag13_id}    -1
    #Should Not Be Equal As Integers    ${tag14_id}    -1

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int    ${res}[tags]    tag1    123
    #Compare Tag Value As Int    ${res}[tags]    tag2    456
    #Compare Tag Value As Int    ${res}[tags]    tag3    30066
    #Compare Tag Value As Int    ${res}[tags]    tag4    0
    #Compare Tag Value As Int    ${res}[tags]    tag5    0
    #Compare Tag Value As Int    ${res}[tags]    tag6    0

    #Compare Tag Value As Int      ${res}[tags]    tag7     0
    #Compare Tag Value As Int      ${res}[tags]    tag8     0
    #Compare Tag Value As Float    ${res}[tags]    tag9     0.0
    #Compare Tag Value As Float    ${res}[tags]    tag10    0.0
    #Compare Tag Value As Int      ${res}[tags]    tag13    0
    #Compare Tag Value As Int      ${res}[tags]    tag14    0

    #Write Tags    ${test_node_name}    ${group}    tag1     1
    #Write Tags    ${test_node_name}    ${group}    tag2     2
    #Write Tags    ${test_node_name}    ${group}    tag3     10
    #Write Tags    ${test_node_name}    ${group}    tag4     12
    #Write Tags    ${test_node_name}    ${group}    tag5     6946924
    #Write Tags    ${test_node_name}    ${group}    tag6     7143536
    #Write Tags    ${test_node_name}    ${group}    tag7     66651131
    #Write Tags    ${test_node_name}    ${group}    tag8     71651235
    #Write Tags    ${test_node_name}    ${group}    tag9     11.234
    #Write Tags    ${test_node_name}    ${group}    tag10    22.234
    #Write Tags    ${test_node_name}    ${group}    tag11    "Hello"
    #Write Tags    ${test_node_name}    ${group}    tag12    "World"

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int       ${res}[tags]    tag1     1
    #Compare Tag Value As Int       ${res}[tags]    tag2     2
    #Compare Tag Value As Int       ${res}[tags]    tag3     10
    #Compare Tag Value As Int       ${res}[tags]    tag4     12
    #Compare Tag Value As Int       ${res}[tags]    tag5     6946924
    #Compare Tag Value As Int       ${res}[tags]    tag6     7143536
    #Compare Tag Value As Int       ${res}[tags]    tag7     66651131
    #Compare Tag Value As Int       ${res}[tags]    tag8     71651235 
    #Compare Tag Value As Float     ${res}[tags]    tag9     11.234
    #Compare Tag Value As Float     ${res}[tags]    tag10    22.234
    #Compare Tag Value As String    ${res}[tags]    tag11    Hello
    #Compare Tag Value As String    ${res}[tags]    tag12    World
    #Compare Tag Value As Int       ${res}[tags]    tag13    1
    #Compare Tag Value As Int       ${res}[tags]    tag14    0

    #[Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id},${tag11_id},${tag12_id},${tag13_id},${tag14_id}

#Read/Write multiple points belonging to different areas(coil/input/input reg/hold reg), the point address includes continuous and non-continuous, and the data include bit/int16/uint16/int32/uint32/float/string type, it should return success
    #${tag1_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!000008", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag2_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!000009", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag3_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!000011", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag4_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!100008", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag5_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!300008", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    #${tag6_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!300009", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    #${tag7_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!300012", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag8_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!400009", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    #${tag9_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag9", "address": "1!400020", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    #${tag10_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag10", "address": "1!400015", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag11_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag11", "address": "1!400025.10", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}

    #Should Not Be Equal As Integers    ${tag1_id}     -1
    #Should Not Be Equal As Integers    ${tag2_id}     -1
    #Should Not Be Equal As Integers    ${tag3_id}     -1
    #Should Not Be Equal As Integers    ${tag4_id}     -1
    #Should Not Be Equal As Integers    ${tag5_id}     -1
    #Should Not Be Equal As Integers    ${tag6_id}     -1
    #Should Not Be Equal As Integers    ${tag7_id}     -1
    #Should Not Be Equal As Integers    ${tag8_id}     -1
    #Should Not Be Equal As Integers    ${tag9_id}     -1
    #Should Not Be Equal As Integers    ${tag10_id}    -1
    #Should Not Be Equal As Integers    ${tag11_id}    -1

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int      ${res}[tags]    tag1    0
    #Compare Tag Value As Int      ${res}[tags]    tag2    0
    #Compare Tag Value As Int      ${res}[tags]    tag3    0
    #Compare Tag Value As Int      ${res}[tags]    tag4    0
    #Compare Tag Value As Int      ${res}[tags]    tag5    0
    #Compare Tag Value As Int      ${res}[tags]    tag6    0
    #Compare Tag Value As Float    ${res}[tags]    tag7    0.0

    #Compare Tag Value As Int      ${res}[tags]    tag8     20069
    #Compare Tag Value As Int      ${res}[tags]    tag9     0
    #Compare Tag Value As Float    ${res}[tags]    tag10    0.0

    #Write Tags    ${test_node_name}    ${group}    tag1     1
    #Write Tags    ${test_node_name}    ${group}    tag2     1
    #Write Tags    ${test_node_name}    ${group}    tag3     1
    #Write Tags    ${test_node_name}    ${group}    tag8     62225
    #Write Tags    ${test_node_name}    ${group}    tag9     66651134
    #Write Tags    ${test_node_name}    ${group}    tag10    11.123
    #Write Tags    ${test_node_name}    ${group}    tag11    "Hello"

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int       ${res}[tags]    tag1     1
    #Compare Tag Value As Int       ${res}[tags]    tag2     1
    #Compare Tag Value As Int       ${res}[tags]    tag3     1
    #Compare Tag Value As Int       ${res}[tags]    tag4     0
    #Compare Tag Value As Int       ${res}[tags]    tag5     0
    #Compare Tag Value As Int       ${res}[tags]    tag6     0
    #Compare Tag Value As Float     ${res}[tags]    tag7     0.0
    #Compare Tag Value As Int       ${res}[tags]    tag8     62225
    #Compare Tag Value As Int       ${res}[tags]    tag9     66651134
    #Compare Tag Value As Float     ${res}[tags]    tag10    11.123
    #Compare Tag Value As String    ${res}[tags]    tag11    Hello

    #[Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id},${tag11_id}

#Read/Write multiple points belonging to different areas(coil/input/input reg/hold reg) and different sites, the point address includes continuous and non-continuous, and the data include int16/uint16/int32/uint32/float/string type, it should return success
    #${tag1_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!000018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag2_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "2!000018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag3_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "3!000021", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag4_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!100018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    #${tag5_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "2!300018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    #${tag6_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "3!300018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    #${tag7_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!300022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag8_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "2!400018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    #${tag9_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag9", "address": "3!400040", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    #${tag10_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag10", "address": "1!400022", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag11_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag11", "address": "2!300022.10", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_STRING}}
    #${tag12_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag12", "address": "3!400022.10", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}

    #Should Not Be Equal As Integers    ${tag1_id}     -1
    #Should Not Be Equal As Integers    ${tag2_id}     -1
    #Should Not Be Equal As Integers    ${tag3_id}     -1
    #Should Not Be Equal As Integers    ${tag4_id}     -1
    #Should Not Be Equal As Integers    ${tag5_id}     -1
    #Should Not Be Equal As Integers    ${tag6_id}     -1
    #Should Not Be Equal As Integers    ${tag7_id}     -1
    #Should Not Be Equal As Integers    ${tag8_id}     -1
    #Should Not Be Equal As Integers    ${tag9_id}     -1
    #Should Not Be Equal As Integers    ${tag10_id}    -1
    #Should Not Be Equal As Integers    ${tag11_id}    -1
    #Should Not Be Equal As Integers    ${tag12_id}    -1

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int      ${res}[tags]    tag1     0
    #Compare Tag Value As Int      ${res}[tags]    tag2     0
    #Compare Tag Value As Int      ${res}[tags]    tag3     0
    #Compare Tag Value As Int      ${res}[tags]    tag4     0
    #Compare Tag Value As Int      ${res}[tags]    tag5     0
    #Compare Tag Value As Int      ${res}[tags]    tag6     0
    #Compare Tag Value As Float    ${res}[tags]    tag7     0.0
    #Compare Tag Value As Int      ${res}[tags]    tag8     0
    #Compare Tag Value As Int      ${res}[tags]    tag9     0
    #Compare Tag Value As Float    ${res}[tags]    tag10    0.0
   ## Compare Tag Value As String    ${res}[tags]    tag11    0
   ## Compare Tag Value As String    ${res}[tags]    tag12    0

    #Write Tags    ${test_node_name}    ${group}    tag1     1
    #Write Tags    ${test_node_name}    ${group}    tag2     1
    #Write Tags    ${test_node_name}    ${group}    tag3     1
    #Write Tags    ${test_node_name}    ${group}    tag8     62226
    #Write Tags    ${test_node_name}    ${group}    tag9     66651136
    #Write Tags    ${test_node_name}    ${group}    tag10    11.789
    #Write Tags    ${test_node_name}    ${group}    tag12    "World"

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int       ${res}[tags]    tag1     1
    #Compare Tag Value As Int       ${res}[tags]    tag2     1
    #Compare Tag Value As Int       ${res}[tags]    tag3     1
    #Compare Tag Value As Int       ${res}[tags]    tag4     0
    #Compare Tag Value As Int       ${res}[tags]    tag5     0
    #Compare Tag Value As Int       ${res}[tags]    tag6     0
    #Compare Tag Value As Float     ${res}[tags]    tag7     0.0
    #Compare Tag Value As Int       ${res}[tags]    tag8     62226
    #Compare Tag Value As Int       ${res}[tags]    tag9     66651136
    #Compare Tag Value As Float     ${res}[tags]    tag10    11.789
   ## Compare Tag Value As String    ${res}[tags]    tag11    Hello
    #Compare Tag Value As String    ${res}[tags]    tag12    World

    #[Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id},${tag11_id},${tag12_id}

#Read/Write multiple points in the hold reg area with different endian and different types(int16/uint16/int32/uint32/float/string), it should return success
    #${tag1_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
    #${tag2_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag2", "address": "1!400001#B", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
    #${tag3_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag3", "address": "1!400001#L", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}

    #${tag4_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag4", "address": "1!400011#BB", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    #${tag5_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag5", "address": "1!400011#BL", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    #${tag6_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag6", "address": "1!400011#LB", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
    #${tag7_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag7", "address": "1!400011#LL", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}

    #${tag8_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag8", "address": "1!400021#BB", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag9_id} =     Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag9", "address": "1!400021#BL", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag10_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag10", "address": "1!400021#LB", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}
    #${tag11_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag11", "address": "1!400021#LL", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

    #${tag12_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag12", "address": "1!400031.10", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}
    #${tag13_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag13", "address": "1!400031.10#B", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}
    #${tag14_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag14", "address": "1!400031.10#L", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}

    #Should Not Be Equal As Integers    ${tag1_id}     -1
    #Should Not Be Equal As Integers    ${tag2_id}     -1
    #Should Not Be Equal As Integers    ${tag3_id}     -1
    #Should Not Be Equal As Integers    ${tag4_id}     -1
    #Should Not Be Equal As Integers    ${tag5_id}     -1
    #Should Not Be Equal As Integers    ${tag6_id}     -1
    #Should Not Be Equal As Integers    ${tag7_id}     -1
    #Should Not Be Equal As Integers    ${tag8_id}     -1
    #Should Not Be Equal As Integers    ${tag9_id}     -1
    #Should Not Be Equal As Integers    ${tag10_id}    -1
    #Should Not Be Equal As Integers    ${tag11_id}    -1
    #Should Not Be Equal As Integers    ${tag12_id}    -1
    #Should Not Be Equal As Integers    ${tag13_id}    -1
    #Should Not Be Equal As Integers    ${tag14_id}    -1

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int      ${res}[tags]    tag1     1
    #Compare Tag Value As Int      ${res}[tags]    tag2     256
    #Compare Tag Value As Int      ${res}[tags]    tag3     1
    #Compare Tag Value As Int      ${res}[tags]    tag4     201354863
    #Compare Tag Value As Int      ${res}[tags]    tag5     814958
    #Compare Tag Value As Int      ${res}[tags]    tag6     1869479948
    #Compare Tag Value As Int      ${res}[tags]    tag7     1852771328
    #Compare Tag Value As Float    ${res}[tags]    tag8     -4.373957407653893e+37
    #Compare Tag Value As Float    ${res}[tags]    tag9     1.497254271224983e-36
    #Compare Tag Value As Float    ${res}[tags]    tag10    -0.3105773329734802
    #Compare Tag Value As Float    ${res}[tags]    tag11    -8.088835712738108e-20

    #Write Tags    ${test_node_name}    ${group}    tag1     123
    #Write Tags    ${test_node_name}    ${group}    tag4     65540
    #Write Tags    ${test_node_name}    ${group}    tag8     1.123
    #Write Tags    ${test_node_name}    ${group}    tag12    "Neuron"

    #Sleep       1s 500ms
    #${res} =    Read Tags    ${test_node_name}    ${group}

    #Compare Tag Value As Int    ${res}[tags]    tag1    123
    #Compare Tag Value As Int    ${res}[tags]    tag2    31488
    #Compare Tag Value As Int    ${res}[tags]    tag3    123

    #Compare Tag Value As Int    ${res}[tags]    tag4    65540
    #Compare Tag Value As Int    ${res}[tags]    tag5    16778240
    #Compare Tag Value As Int    ${res}[tags]    tag6    67109120
    #Compare Tag Value As Int    ${res}[tags]    tag7    262145

    #Compare Tag Value As Float    ${res}[tags]    tag8     1.123
    #Compare Tag Value As Float    ${res}[tags]    tag9     -9.440088562527097e-30
    #Compare Tag Value As Float    ${res}[tags]    tag10    7.730013898977952e+33
    #Compare Tag Value As Float    ${res}[tags]    tag11    -0.2414533942937851

    #Compare Tag Value As String    ${res}[tags]    tag12    Neuron
    #Compare Tag Value As String    ${res}[tags]    tag13    Neuron
    #Compare Tag Value As String    ${res}[tags]    tag14    Neuron

    #[Teardown]    Del Tags    ${test_node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id},${tag11_id},${tag12_id},${tag13_id},${tag14_id}

*** Keywords ***
Neuron Context Ready
    Start Simulator Arg    ${MODBUS_TCP_SERVER_SIMULATOR}    ${SIMULATOR_DIR} 

    Neuron Ready
    LOGIN

    Add Node    type=${${NODE_DRIVER}}    name=${test_node_name}    plugin_name=modbus-tcp
    ${id} =     Get Node ID               ${NODE_DRIVER}            ${test_node_name}

    Set Global Variable    ${test_node_id}    ${id}

    Node Setting    ${test_node_id}    ${MODBUS_PLUS_TCP_CONFIG}
    Node Ctl        ${test_node_id}    ${NODE_CTL_START}

    Add Group Config    ${test_node_id}    ${group}    300
    Subscribe Group     ${test_node_id}    1           ${group}

Stop All Processes
    Stop Neuron                remove_persistence_data=True
    sleep                      1s
    Terminate All Processes    kill=false

Read Write A Point In The hold/coil Reg Area Should Success
    [Arguments]       ${node_id}                                                                                 ${node_name}    ${tag_name}    ${group}    ${tag}    ${area}    ${type}    ${value}
    Log               Read a point in the ${area} reg area, and the data as ${type}, it should return success
    Log To Console    Read a point in the ${area} reg area, and the data as ${type}, it should return success

    ${val} =    Set Variable             ${value}
    IF          "${type}" == "float"
    ${cmp} =    Set Variable             Compare Tag Value As Float
    ELSE IF     "${type}" == "string"
    ${cmp} =    Set Variable             Compare Tag Value As String
    ${val} =    Set Variable             "${value}"
    ELSE
    ${cmp} =    Set Variable             Compare Tag Value As Int
    END

    ${tag_id} =                        Add Tag And Return ID    ${node_id}    ${group}    ${tag}
    Should Not Be Equal As Integers    ${tag_id}                -1

    Sleep                    1s 500ms
    ${res} =                 Read Tags    ${node_name}    ${group}
    Check Response Status    ${res}       200

    ${res} =    Write Tags    ${node_name}    ${group}    ${tag_name}    ${val}

    Sleep                    1s 500ms
    ${res} =                 Read Tags    ${node_name}    ${group}
    Check Response Status    ${res}       200
    Run Keyword              ${cmp}       ${res}[tags]    ${res}[tags][0][name]    ${value}

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag_id}

Read A Point In The Input Or Input Reg Area Should Return Success
    [Arguments]       ${node_id}                                                                             ${node_name}    ${group}    ${area}    ${type}    ${tag1}    ${value1}    ${tag2}    ${value2}
    Log               Read a point in the ${area} area, and the data as ${type}, it should return success
    Log To Console    Read a point in the ${area} area, and the data as ${type}, it should return success

    IF          "${type}" == "float"
    ${cmp} =    Set Variable             Compare Tag Value As Float
    ELSE IF     "${type}" == "string"
    ${cmp} =    Set Variable             Compare Tag Value As String
    ELSE
    ${cmp} =    Set Variable             Compare Tag Value As Int
    END

    ${tag1_id} =                       Add Tag And Return ID    ${node_id}    ${group}    ${tag1}
    Should Not Be Equal As Integers    ${tag1_id}               -1

    ${tag2_id} =                       Add Tag And Return ID    ${node_id}    ${group}    ${tag2}
    Should Not Be Equal As Integers    ${tag2_id}               -1

    Sleep                    1s 500ms
    ${res} =                 Read Tags    ${node_name}    ${group}
    Check Response Status    ${res}       200

    Run Keyword    ${cmp}    ${res}[tags]    ${res}[tags][0][name]    ${value1}
    Run Keyword    ${cmp}    ${res}[tags]    ${res}[tags][1][name]    ${value2}

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id}

Del Tags Check
    [Arguments]              ${node_id}    ${group}          ${tag_id}
    ${res} =                 Del Tags      ${node_id}        ${group}     ${tag_id}
    Check Response Status    ${res}        200
    Check Error Code         ${res}        ${ERR_SUCCESS}
