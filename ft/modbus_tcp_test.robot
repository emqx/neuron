*** Settings ***
Library           Keyword.Neuron.Neuron
Library           Keyword.Neuron.Tool
Library           Keyword.Neuron.Node
Library           Keyword.Neuron.GrpConfig
Library           Keyword.Neuron.Subscribe
Library           Keyword.Neuron.Tag
Resource          error.robot
Resource          neuron.robot
Resource          simulator.robot
Library           REST                        http://127.0.0.1:7001
Suite Setup       Neuron Context Ready
Suite Teardown    Stop All Processes

*** Variables ***
${node_id}
${group}	config_modbus_tcp_sample_2

*** Test Cases ***
Read unsubscribed point, it should return failure
	Add Tags    ${node_id}    xx	{"name": "tag1", "address": "1!40001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}

	Sleep    3s

	POST    /api/v2/read    {"node_id": ${node_id}, "group_config_name": "xx"}

	Integer    response status    404
	Integer    $.error            ${ERR_GRP_NOT_SUBSCRIBE}

	[Teardown]	Del Group Config	${node_id}	xx

Read a point in the hold reg area, and the data as int16, it should return success
	Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!40001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}

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
	Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!40002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}

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

Read a point in the hold reg area, and the data as int32, it should return success
	Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!40003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}

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
	Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!40005", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}

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
	Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!40007", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}

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
	Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!00001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}

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
	Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!10001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}
	Add Tags    ${node_id}    ${group}	{"name": "tag2", "address": "1!10002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}

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
	Add Tags    ${node_id}    ${group}	{"name": "tag1", "address": "1!40001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}

	${tag1_id} =	Get Tag ID            ${node_id}       ${group}    tag1
	should not be equal as integers    ${tag1_id}	-1
	Sleep                              4s

	${tags} =	Read Tags         ${node_id}      ${group}
	Compare Tag Value As Int    ${tags}.tags    ${tag1_id}    123

	[Teardown]	Del Tags	${node_id}	${group}	${tag1_id}

#Read a point in the input reg area, and the data as uint16, it should return success
#Read a point in the input reg area, and the data as uint32, it should return success
#Read a point in the input reg area, and the data as int32, it should return success
#Read a point in the input reg area, and the data as float, it should return success
#Read multiple points in the coil area, the point address includes continuous and non-continuous, and the data as bit type, it should return success
#Read multiple points in the input area, the point address includes continuous and non-continuous, and the data as bit type, it should return success
#Read multiple points in the input reg area, the point address includes continuous and non-continuous, and the data includes int16/uint16/int32/uint32/float type, it should return success
#Read multiple points in the hold reg area, the point address includes continuous and non-continuous, and the data includes int16/uint16/int32/uint32/float type, it should return success
#Read multiple points belonging to different areas(coil/input/input reg/hold reg), the point address includes continuous and non-continuous, and the data include int16/uint16/int32/uint32/float type, it should return success
#Read multiple points belonging to different areas(coil/input/input reg/hold reg) and different sites, the point address includes continuous and non-continuous, and the data include int16/uint16/int32/uint32/float type, it should return success

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
