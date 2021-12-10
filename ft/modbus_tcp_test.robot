*** Settings ***
Resource          api_http.resource
Suite Setup       Neuron Context Ready
Suite Teardown    Stop All Processes

*** Variables ***
${node_id}
${group}	config_modbus_tcp_sample_2

*** Test Cases ***
Read unsubscribed point, it should return failure
    Add Tags    ${node_id}    xx	{"name": "tag1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}

    Sleep    3s

    POST    /api/v2/read    {"node_id": ${node_id}, "group_config_name": "xx"}

    Integer    response status    404
    Integer    $.error            ${ERR_GRP_NOT_SUBSCRIBE}

    [Teardown]	Del Group Config	${node_id}	xx

Read a point in the hold reg area, and the data as int16, it should return success
    Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}

    ${tag_id} =	Get Tag ID             ${node_id}      ${group}    tag1
    should not be equal as integers    ${tag_id}	-1
    Sleep                              3s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    0

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 123}

    sleep    3s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    123

    [Teardown]	Del Tags	${node_id}	${group}	${tag_id}

Read a point in the hold reg area, and the data as uint16, it should return success
    Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}

    ${tag_id} =	Get Tag ID             ${node_id}      ${group}    tag1
    should not be equal as integers    ${tag_id}	-1
    Sleep                              3s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    0

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 456}

    sleep    3s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    456

    [Teardown]	Del Tags	${node_id}	${group}	${tag_id}

Read a point in the hold reg area, and the data as int32, it should return success
    Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}

    ${tag_id} =	Get Tag ID             ${node_id}      ${group}    tag1
    should not be equal as integers    ${tag_id}	-1
    Sleep                              3s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    0

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 66778899}

    sleep    3s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    66778899

    [Teardown]	Del Tags	${node_id}	${group}	${tag_id}

Read a point in the hold reg area, and the data as uint32, it should return success
    Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!400005", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}

    ${tag_id} =	Get Tag ID             ${node_id}      ${group}    tag1
    should not be equal as integers    ${tag_id}	-1
    Sleep                              3s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    0

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 667788995}

    sleep    3s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    667788995

    [Teardown]	Del Tags	${node_id}	${group}	${tag_id}

Read a point in the hold reg area, and the data as float, it should return success
    Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!400007", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

    ${tag_id} =	Get Tag ID             ${node_id}      ${group}    tag1
    should not be equal as integers    ${tag_id}	-1
    Sleep                              3s

    ${tags} =	Read Tags           ${node_id}      ${group}
    Compare Tag Value As Float    ${tags}.tags    ${tag_id}    0

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 11.22}

    sleep    3s

    ${tags} =	Read Tags           ${node_id}      ${group}
    Compare Tag Value As Float    ${tags}.tags    ${tag_id}    11.22

    [Teardown]	Del Tags	${node_id}	${group}	${tag_id}

Read a point in the coil area, and the data as bit type, it should return success
    Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!000001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}

    ${tag_id} =	Get Tag ID             ${node_id}      ${group}    tag1
    should not be equal as integers    ${tag_id}	-1
    Sleep                              3s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    0

    Write Tags    ${node_id}    ${group}    {"id": ${tag_id}, "value": 1}

    sleep    3s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag_id}    1

    [Teardown]	Del Tags	${node_id}	${group}	${tag_id}

Read a point in the input area, and the data as bit type, it should return success
    Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}	{"name": "tag2", "address": "1!100002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}

    ${tag1_id} =	Get Tag ID            ${node_id}       ${group}    tag1
    ${tag2_id} =	Get Tag ID            ${node_id}       ${group}    tag2
    should not be equal as integers    ${tag1_id}	-1
    should not be equal as integers    ${tag2_id}	-1
    Sleep                              4s

    ${tags} =	Read Tags         ${node_id}      ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag1_id}    0
    Compare Tag Value As Int    ${tags}.tags    ${tag2_id}    1

    [Teardown]	Del Tags	${node_id}	${group}	${tag1_id},${tag2_id}


Read a point in the input reg area, and the data as int16, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!301021", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!301022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}

    ${tag1_id} =    Get Tag ID    ${node_id}    ${group}    tag1
    ${tag2_id} =    Get Tag ID    ${node_id}    ${group}    tag2

    should not be equal as integers    ${tag1_id}    -1
    should not be equal as integers    ${tag2_id}    -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag1_id}    1020
    Compare Tag Value As Int    ${tags}.tags    ${tag2_id}    1021

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id}

Read a point in the input reg area, and the data as uint16, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!301019", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}

    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!301020", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}

    ${tag1_id} =    Get Tag ID    ${node_id}    ${group}    tag1
    ${tag2_id} =    Get Tag ID    ${node_id}    ${group}    tag2

    should not be equal as integers    ${tag1_id}    -1

    should not be equal as integers    ${tag2_id}    -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag1_id}    1018
    Compare Tag Value As Int    ${tags}.tags    ${tag2_id}    1019

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id}

Read a point in the input reg area, and the data as uint32, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!301017", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!301018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}

    ${tag1_id} =    Get Tag ID    ${node_id}    ${group}    tag1
    ${tag2_id} =    Get Tag ID    ${node_id}    ${group}    tag2

    should not be equal as integers    ${tag1_id}    -1

    should not be equal as integers    ${tag2_id}    -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag1_id}    66585593
    Compare Tag Value As Int    ${tags}.tags    ${tag2_id}    66650112

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id}

Read a point in the input reg area, and the data as int32, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!301010", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!301011", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT32}}

    ${tag1_id} =    Get Tag ID    ${node_id}    ${group}    tag1
    ${tag2_id} =    Get Tag ID    ${node_id}    ${group}    tag2

    should not be equal as integers    ${tag1_id}    -1

    should not be equal as integers    ${tag2_id}    -1
    Sleep                              4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag1_id}    66126834
    Compare Tag Value As Int    ${tags}.tags    ${tag2_id}    66191360

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id}

Read a point in the input reg area, and the data as float, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!300009", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!300008", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}

    ${tag1_id} =    Get Tag ID    ${node_id}    ${group}    tag1
    ${tag2_id} =    Get Tag ID    ${node_id}    ${group}    tag2

    should not be equal as integers    ${tag1_id}    -1

    should not be equal as integers    ${tag2_id}    -1
    Sleep                              4s

    ${tags} =                     Read Tags       ${node_id}    ${group}
    Compare Tag Value As Float    ${tags}.tags    ${tag1_id}    7.346839692639297e-40
    Compare Tag Value As Float    ${tags}.tags    ${tag2_id}    6.428596834936531e-40

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id}

Read multiple points in the coil area, the point address includes continuous and non-continuous, and the data as bit type, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!000001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!000002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag3", "address": "1!000010", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag4", "address": "1!000012", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag5", "address": "1!000107", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag6", "address": "1!000108", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag7", "address": "1!001022", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag8", "address": "1!001020", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}

    ${tag1_id} =    Get Tag ID    ${node_id}    ${group}    tag1
    ${tag2_id} =    Get Tag ID    ${node_id}    ${group}    tag2
    ${tag3_id} =    Get Tag ID    ${node_id}    ${group}    tag3
    ${tag4_id} =    Get Tag ID    ${node_id}    ${group}    tag4
    ${tag5_id} =    Get Tag ID    ${node_id}    ${group}    tag5
    ${tag6_id} =    Get Tag ID    ${node_id}    ${group}    tag6
    ${tag7_id} =    Get Tag ID    ${node_id}    ${group}    tag7
    ${tag8_id} =    Get Tag ID    ${node_id}    ${group}    tag8

    should not be equal as integers    ${tag1_id}    -1
    should not be equal as integers    ${tag2_id}    -1
    should not be equal as integers    ${tag3_id}    -1
    should not be equal as integers    ${tag4_id}    -1
    should not be equal as integers    ${tag5_id}    -1
    should not be equal as integers    ${tag6_id}    -1
    should not be equal as integers    ${tag7_id}    -1
    should not be equal as integers    ${tag8_id}    -1
    Sleep                              4s

    ${tags} =    Read Tags    ${node_id}    ${group}

    Compare Tag Value As Int    ${tags}.tags    ${tag1_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag2_id}    0
    Compare Tag Value As Int    ${tags}.tags    ${tag3_id}    0
    Compare Tag Value As Int    ${tags}.tags    ${tag4_id}    0
    Compare Tag Value As Int    ${tags}.tags    ${tag5_id}    0
    Compare Tag Value As Int    ${tags}.tags    ${tag6_id}    0
    Compare Tag Value As Int    ${tags}.tags    ${tag7_id}    0
    Compare Tag Value As Int    ${tags}.tags    ${tag8_id}    0

    Write Tags    ${node_id}    ${group}    {"id": ${tag2_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag3_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag4_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag5_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag7_id}, "value": 1}
    Sleep         4s

    ${tags} =                   Read Tags       ${node_id}    ${group}
    Compare Tag Value As Int    ${tags}.tags    ${tag1_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag2_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag3_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag4_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag5_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag6_id}    0
    Compare Tag Value As Int    ${tags}.tags    ${tag7_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag8_id}    0

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id}

Read multiple points in the input area, the point address includes continuous and non-continuous, and the data as bit type, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!100002", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag3", "address": "1!100010", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag4", "address": "1!100012", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag5", "address": "1!100107", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag6", "address": "1!100108", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag7", "address": "1!101022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag8", "address": "1!101020", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}

    ${tag1_id} =    Get Tag ID    ${node_id}    ${group}    tag1
    ${tag2_id} =    Get Tag ID    ${node_id}    ${group}    tag2
    ${tag3_id} =    Get Tag ID    ${node_id}    ${group}    tag3
    ${tag4_id} =    Get Tag ID    ${node_id}    ${group}    tag4
    ${tag5_id} =    Get Tag ID    ${node_id}    ${group}    tag5
    ${tag6_id} =    Get Tag ID    ${node_id}    ${group}    tag6
    ${tag7_id} =    Get Tag ID    ${node_id}    ${group}    tag7
    ${tag8_id} =    Get Tag ID    ${node_id}    ${group}    tag8

    should not be equal as integers    ${tag1_id}    -1
    should not be equal as integers    ${tag2_id}    -1
    should not be equal as integers    ${tag3_id}    -1
    should not be equal as integers    ${tag4_id}    -1
    should not be equal as integers    ${tag5_id}    -1
    should not be equal as integers    ${tag6_id}    -1
    should not be equal as integers    ${tag7_id}    -1
    should not be equal as integers    ${tag8_id}    -1
    Sleep                              4s

    ${tags} =    Read Tags    ${node_id}    ${group}

    Compare Tag Value As Int    ${tags}.tags    ${tag1_id}    0
    Compare Tag Value As Int    ${tags}.tags    ${tag2_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag3_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag4_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag5_id}    0
    Compare Tag Value As Int    ${tags}.tags    ${tag6_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag7_id}    1
    Compare Tag Value As Int    ${tags}.tags    ${tag8_id}    1

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id}

Read multiple points in the input reg area, the point address includes continuous and non-continuous, and the data includes int16/uint16/int32/uint32/float type, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!300001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!300002", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag3", "address": "1!300010", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag4", "address": "1!300012", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag5", "address": "1!300107", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag6", "address": "1!300110", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag7", "address": "1!301018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag8", "address": "1!301020", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}

    ${tag1_id} =    Get Tag ID    ${node_id}    ${group}    tag1
    ${tag2_id} =    Get Tag ID    ${node_id}    ${group}    tag2
    ${tag3_id} =    Get Tag ID    ${node_id}    ${group}    tag3
    ${tag4_id} =    Get Tag ID    ${node_id}    ${group}    tag4
    ${tag5_id} =    Get Tag ID    ${node_id}    ${group}    tag5
    ${tag6_id} =    Get Tag ID    ${node_id}    ${group}    tag6
    ${tag7_id} =    Get Tag ID    ${node_id}    ${group}    tag7
    ${tag8_id} =    Get Tag ID    ${node_id}    ${group}    tag8

    should not be equal as integers    ${tag1_id}    -1
    should not be equal as integers    ${tag2_id}    -1
    should not be equal as integers    ${tag3_id}    -1
    should not be equal as integers    ${tag4_id}    -1
    should not be equal as integers    ${tag5_id}    -1
    should not be equal as integers    ${tag6_id}    -1
    should not be equal as integers    ${tag7_id}    -1
    should not be equal as integers    ${tag8_id}    -1
    Sleep                              4s

    ${tags} =    Read Tags    ${node_id}    ${group}

    Compare Tag Value As Int      ${tags}.tags    ${tag1_id}    0
    Compare Tag Value As Int      ${tags}.tags    ${tag2_id}    1
    Compare Tag Value As Int      ${tags}.tags    ${tag3_id}    9
    Compare Tag Value As Int      ${tags}.tags    ${tag4_id}    11
    Compare Tag Value As Int      ${tags}.tags    ${tag5_id}    6946923
    Compare Tag Value As Int      ${tags}.tags    ${tag6_id}    7143534
    Compare Tag Value As Int      ${tags}.tags    ${tag7_id}    66651130
    Compare Tag Value As Float    ${tags}.tags    ${tag8_id}    1.475336887045722e-36

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id}

Read multiple points in the hold reg area, the point address includes continuous and non-continuous, and the data includes int16/uint16/int32/uint32/float type, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag3", "address": "1!400010", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag4", "address": "1!400012", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag5", "address": "1!400107", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag6", "address": "1!400110", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag7", "address": "1!401018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag8", "address": "1!401020", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

    ${tag1_id} =    Get Tag ID    ${node_id}    ${group}    tag1
    ${tag2_id} =    Get Tag ID    ${node_id}    ${group}    tag2
    ${tag3_id} =    Get Tag ID    ${node_id}    ${group}    tag3
    ${tag4_id} =    Get Tag ID    ${node_id}    ${group}    tag4
    ${tag5_id} =    Get Tag ID    ${node_id}    ${group}    tag5
    ${tag6_id} =    Get Tag ID    ${node_id}    ${group}    tag6
    ${tag7_id} =    Get Tag ID    ${node_id}    ${group}    tag7
    ${tag8_id} =    Get Tag ID    ${node_id}    ${group}    tag8

    should not be equal as integers    ${tag1_id}    -1
    should not be equal as integers    ${tag2_id}    -1
    should not be equal as integers    ${tag3_id}    -1
    should not be equal as integers    ${tag4_id}    -1
    should not be equal as integers    ${tag5_id}    -1
    should not be equal as integers    ${tag6_id}    -1
    should not be equal as integers    ${tag7_id}    -1
    should not be equal as integers    ${tag8_id}    -1
    Sleep                              4s

    ${tags} =    Read Tags    ${node_id}    ${group}

    Compare Tag Value As Int      ${tags}.tags    ${tag1_id}    123
    Compare Tag Value As Int      ${tags}.tags    ${tag2_id}    456
    Compare Tag Value As Int      ${tags}.tags    ${tag3_id}    0
    Compare Tag Value As Int      ${tags}.tags    ${tag4_id}    0
    Compare Tag Value As Int      ${tags}.tags    ${tag5_id}    0
    Compare Tag Value As Int      ${tags}.tags    ${tag6_id}    0
    Compare Tag Value As Int      ${tags}.tags    ${tag7_id}    0
    Compare Tag Value As Float    ${tags}.tags    ${tag8_id}    0.0

    Write Tags    ${node_id}    ${group}    {"id": ${tag1_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag2_id}, "value": 2}
    Write Tags    ${node_id}    ${group}    {"id": ${tag3_id}, "value": 10}
    Write Tags    ${node_id}    ${group}    {"id": ${tag4_id}, "value": 12}
    Write Tags    ${node_id}    ${group}    {"id": ${tag5_id}, "value": 6946924}
    Write Tags    ${node_id}    ${group}    {"id": ${tag6_id}, "value": 7143536}
    Write Tags    ${node_id}    ${group}    {"id": ${tag7_id}, "value": 66651131}
    Write Tags    ${node_id}    ${group}    {"id": ${tag8_id}, "value": 11.234}
    Sleep         4s

    ${tags} =    Read Tags    ${node_id}    ${group}

    Compare Tag Value As Int      ${tags}.tags    ${tag1_id}    1
    Compare Tag Value As Int      ${tags}.tags    ${tag2_id}    2
    Compare Tag Value As Int      ${tags}.tags    ${tag3_id}    10
    Compare Tag Value As Int      ${tags}.tags    ${tag4_id}    12
    Compare Tag Value As Int      ${tags}.tags    ${tag5_id}    6946924
    Compare Tag Value As Int      ${tags}.tags    ${tag6_id}    7143536
    Compare Tag Value As Int      ${tags}.tags    ${tag7_id}    66651131
    Compare Tag Value As Float    ${tags}.tags    ${tag8_id}    11.234

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id}

Read multiple points belonging to different areas(coil/input/input reg/hold reg), the point address includes continuous and non-continuous, and the data include bit/int16/uint16/int32/uint32/float type, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!000008", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "1!000009", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag3", "address": "1!000011", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag4", "address": "1!100008", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag5", "address": "1!300008", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag6", "address": "1!300009", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag7", "address": "1!300012", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag8", "address": "1!400009", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag9", "address": "1!400020", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag10", "address": "1!400015", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

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
    Sleep                              4s

    ${tags} =    Read Tags    ${node_id}    ${group}

    Compare Tag Value As Int      ${tags}.tags    ${tag1_id}     0
    Compare Tag Value As Int      ${tags}.tags    ${tag2_id}     0
    Compare Tag Value As Int      ${tags}.tags    ${tag3_id}     0
    Compare Tag Value As Int      ${tags}.tags    ${tag4_id}     1
    Compare Tag Value As Int      ${tags}.tags    ${tag5_id}     7
    Compare Tag Value As Int      ${tags}.tags    ${tag6_id}     524297
    Compare Tag Value As Float    ${tags}.tags    ${tag7_id}     1.010207273319475e-39
    Compare Tag Value As Int      ${tags}.tags    ${tag8_id}     0
    Compare Tag Value As Int      ${tags}.tags    ${tag9_id}     0
    Compare Tag Value As Float    ${tags}.tags    ${tag10_id}    0.0

    Write Tags    ${node_id}    ${group}    {"id": ${tag1_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag2_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag3_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag8_id}, "value": 62225}
    Write Tags    ${node_id}    ${group}    {"id": ${tag9_id}, "value": 66651134}
    Write Tags    ${node_id}    ${group}    {"id": ${tag10_id}, "value": 11.123}
    Sleep         4s

    ${tags} =    Read Tags    ${node_id}    ${group}

    Compare Tag Value As Int      ${tags}.tags    ${tag1_id}     1
    Compare Tag Value As Int      ${tags}.tags    ${tag2_id}     1
    Compare Tag Value As Int      ${tags}.tags    ${tag3_id}     1
    Compare Tag Value As Int      ${tags}.tags    ${tag4_id}     1
    Compare Tag Value As Int      ${tags}.tags    ${tag5_id}     7
    Compare Tag Value As Int      ${tags}.tags    ${tag6_id}     524297
    Compare Tag Value As Float    ${tags}.tags    ${tag7_id}     1.010207273319475e-39
    Compare Tag Value As Int      ${tags}.tags    ${tag8_id}     62225
    Compare Tag Value As Int      ${tags}.tags    ${tag9_id}     66651134
    Compare Tag Value As Float    ${tags}.tags    ${tag10_id}    11.123

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id}

Read multiple points belonging to different areas(coil/input/input reg/hold reg) and different sites, the point address includes continuous and non-continuous, and the data include int16/uint16/int32/uint32/float type, it should return success
    Add Tags    ${node_id}    ${group}    {"name": "tag1", "address": "1!000018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag2", "address": "2!000018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag3", "address": "3!000021", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag4", "address": "4!100018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_BIT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag5", "address": "5!300018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_INT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag6", "address": "6!300018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_UINT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag7", "address": "7!300022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_DATA_TYPE_FLOAT}}
    Add Tags    ${node_id}    ${group}    {"name": "tag8", "address": "8!400018", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
    Add Tags    ${node_id}    ${group}    {"name": "tag9", "address": "9!400040", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
    Add Tags    ${node_id}    ${group}    {"name": "tag10", "address": "10!400022", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

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
    Sleep                              4s

    ${tags} =    Read Tags    ${node_id}    ${group}

    Compare Tag Value As Int      ${tags}.tags    ${tag1_id}     0
    Compare Tag Value As Int      ${tags}.tags    ${tag2_id}     0
    Compare Tag Value As Int      ${tags}.tags    ${tag3_id}     0
    Compare Tag Value As Int      ${tags}.tags    ${tag4_id}     1
    Compare Tag Value As Int      ${tags}.tags    ${tag5_id}     17
    Compare Tag Value As Int      ${tags}.tags    ${tag6_id}     1114130
    Compare Tag Value As Float    ${tags}.tags    ${tag7_id}     1.928576247884031e-39
    Compare Tag Value As Int      ${tags}.tags    ${tag8_id}     0
    Compare Tag Value As Int      ${tags}.tags    ${tag9_id}     0
    Compare Tag Value As Float    ${tags}.tags    ${tag10_id}    0.0

    Write Tags    ${node_id}    ${group}    {"id": ${tag1_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag2_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag3_id}, "value": 1}
    Write Tags    ${node_id}    ${group}    {"id": ${tag8_id}, "value": 62226}
    Write Tags    ${node_id}    ${group}    {"id": ${tag9_id}, "value": 66651136}
    Write Tags    ${node_id}    ${group}    {"id": ${tag10_id}, "value": 11.789}
    Sleep         4s

    ${tags} =    Read Tags    ${node_id}    ${group}

    Compare Tag Value As Int      ${tags}.tags    ${tag1_id}     1
    Compare Tag Value As Int      ${tags}.tags    ${tag2_id}     1
    Compare Tag Value As Int      ${tags}.tags    ${tag3_id}     1
    Compare Tag Value As Int      ${tags}.tags    ${tag4_id}     1
    Compare Tag Value As Int      ${tags}.tags    ${tag5_id}     17
    Compare Tag Value As Int      ${tags}.tags    ${tag6_id}     1114130
    Compare Tag Value As Float    ${tags}.tags    ${tag7_id}     1.928576247884031e-39
    Compare Tag Value As Int      ${tags}.tags    ${tag8_id}     62226
    Compare Tag Value As Int      ${tags}.tags    ${tag9_id}     66651136
    Compare Tag Value As Float    ${tags}.tags    ${tag10_id}    11.789

    [Teardown]    Del Tags    ${node_id}    ${group}    ${tag1_id},${tag2_id},${tag3_id},${tag4_id},${tag5_id},${tag6_id},${tag7_id},${tag8_id},${tag9_id},${tag10_id}

*** Keywords ***
Neuron Context Ready
	Start Simulator    ${MODBUS_TCP_SERVER_SIMULATOR}    ${SIMULATOR_DIR}
	Neuron Ready

	${token} =    LOGIN

    ${jwt} =    Catenate    Bearer    ${token}

    Set Headers    {"Authorization":"${jwt}"}

	${id} =	Get Node ID    ${NODE_DRIVER}	modbus-tcp-adapter

	Set Global Variable	${node_id}	${id}

	Node Setting    ${node_id}    ${MODBUS_TCP_CONFIG}

Stop All Processes
    LOGOUT

	Stop Neuron
	sleep                      1s
	Terminate All Processes    kill=false
