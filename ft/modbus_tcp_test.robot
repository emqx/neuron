*** Settings ***
Resource 	resource/api.resource
Resource	resource/common.resource
Resource	resource/error.resource
Resource	resource/driver.resource
Suite Setup	Modbus Test Setup
Suite Teardown	Modbus Test Teardown

*** Variables ***
${modbus_node}	modbus_node

${MODBUS_TCP_CONFIG_WRONG} 	{"host": "127.0.0.1", "port": 60502}

${hold_bit}       {"name": "hold_bit", "address": "1!400001.15", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${hold_int16}     {"name": "hold_int16", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT16}}
${hold_uint16}    {"name": "hold_uint16", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_UINT16}}
${hold_int32}     {"name": "hold_int32", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT32}}
${hold_uint32}    {"name": "hold_uint32", "address": "1!400015", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_UINT32}}
${hold_float}     {"name": "hold_float", "address": "1!400017", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_FLOAT}}
${hold_string}    {"name": "hold_string", "address": "1!400020.10", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_STRING}}

${coil_bit_1}    {"name": "coil_bit_1", "address": "1!000001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${coil_bit_2}    {"name": "coil_bit_2", "address": "1!000002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${coil_bit_3}    {"name": "coil_bit_3", "address": "1!000003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${coil_bit_4}    {"name": "coil_bit_4", "address": "1!000005", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${coil_bit_5}    {"name": "coil_bit_5", "address": "1!000010", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}

${input_bit_1}    {"name": "input_bit_1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${input_bit_2}    {"name": "input_bit_2", "address": "1!100002", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${input_bit_3}    {"name": "input_bit_3", "address": "1!100003", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${input_bit_4}    {"name": "input_bit_4", "address": "1!100005", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${input_bit_5}    {"name": "input_bit_5", "address": "1!100010", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}

${input_register_bit}       {"name": "input_register_bit", "address": "1!30101.15", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${input_register_int16}     {"name": "input_register_int16", "address": "1!30101", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}
${input_register_uint16}    {"name": "input_register_uint16", "address": "1!30102", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_UINT16}}
${input_register_int32}     {"name": "input_register_int32", "address": "1!30103", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT32}}
${input_register_uint32}    {"name": "input_register_uint32", "address": "1!30115", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_UINT32}}
${input_register_float}     {"name": "input_register_float", "address": "1!30117", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_FLOAT}}
${input_register_string}    {"name": "input_register_string", "address": "1!30120.10", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_STRING}}

${hold_int16_address_update} 	{"name": "hold_int16", "address": "1!400100", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT16}}
${hold_int16_type_update} 		{"name": "hold_int16", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT32}}

${input_bit_1_name_wrong}   	  {"name": "input_bit", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${input_bit_1_attribute_unmatch}    {"name": "input_bit_1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${input_bit_1_type_unmatch}   	  {"name": "input_bit_1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}

*** Test Cases ***
Set a node with right settings, it should be success.
	[Template]	Set a ${node} with right ${node_settings}, it will be success.
	${modbus_node}	${MODBUS_TCP_CONFIG}

Set a node with wrong settings, it should be failure.
	[Template]	Set a ${node} with wrong ${node_settings}, it will be failure.
	${modbus_node}	${MODBUS_TCP_CONFIG_WRONG}

Set tags with multiple address and types and attribute, it should be success.
	[Template]	Add a ${tag} with right address and type and attribute from ${node}, it will be success.
	${hold_int16}	${modbus_node}
	${hold_uint16}	${modbus_node}
	${hold_int32}	${modbus_node}
	${hold_uint32}	${modbus_node}
	${hold_float}	${modbus_node}
	${hold_string}	${modbus_node}

	${coil_bit_1}	${modbus_node}
	${coil_bit_2}	${modbus_node}
	${coil_bit_3}	${modbus_node}
	${coil_bit_4}	${modbus_node}
	${coil_bit_5}	${modbus_node}

	${input_register_int16}	${modbus_node}
	${input_register_uint16}	${modbus_node}
	${input_register_int32}	${modbus_node}
	${input_register_uint32}	${modbus_node}
	${input_register_float}	${modbus_node}
	${input_register_string}	${modbus_node}

	${input_bit_1}	${modbus_node}
	${input_bit_2}	${modbus_node}
	${input_bit_3}	${modbus_node}
	${input_bit_4}	${modbus_node}
	${input_bit_5}	${modbus_node}

Set tags with unmatch address, type or attribute, it should be failure.
	[Template]	Set a ${tag} with unmatch address, type or attribute from ${node}, it will be failure and return ${resp_status} and ${error_code}.
	${input_bit_1_type_unmatch}	${modbus_node}	200	${NEU_ERR_TAG_TYPE_NOT_SUPPORT}
	${input_bit_1_attribute_unmatch}	${modbus_node}	200	${NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT}	
	
Write and Read all type tag with same slave id in hold register and coil, it should be success.
	[Template]	Write and Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	${hold_int16}     hold_int16     ${modbus_node}	Compare Tag Value As Int        120
	${hold_uint16}    hold_uint16    ${modbus_node}	Compare Tag Value As Int        65535
	${hold_int32}     hold_int32     ${modbus_node}	Compare Tag Value As Int        77889912
	${hold_uint32}    hold_uint32    ${modbus_node}	Compare Tag Value As Int        88991232
	${hold_float}     hold_float     ${modbus_node}	Compare Tag Value As Float      12.34
	${hold_string}    hold_string    ${modbus_node}	Compare Tag Value As Strings    "abc123"

	${coil_bit_1}    coil_bit_1    ${modbus_node}    Compare Tag Value As Int    1
	${coil_bit_2}    coil_bit_2    ${modbus_node}    Compare Tag Value As Int    0
	${coil_bit_3}    coil_bit_3    ${modbus_node}    Compare Tag Value As Int    0
	${coil_bit_4}    coil_bit_4    ${modbus_node}    Compare Tag Value As Int    1
	${coil_bit_5}    coil_bit_5    ${modbus_node}    Compare Tag Value As Int    1

Read all type tag with same slave id in input register and input, it should be success.
	[Template]	Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	${input_register_int16}     input_register_int16     ${modbus_node}	Compare Tag Value As Int        0
	${input_register_uint16}    input_register_uint16    ${modbus_node}	Compare Tag Value As Int        0
	${input_register_int32}     input_register_int32     ${modbus_node}	Compare Tag Value As Int        0
	${input_register_uint32}    input_register_uint32    ${modbus_node}	Compare Tag Value As Int        0
	${input_register_float}     input_register_float     ${modbus_node}	Compare Tag Value As Float      0
	${input_register_string}    input_register_string    ${modbus_node}	Compare Tag Value As Strings    ""

	${input_bit_1}    input_bit_1    ${modbus_node}    Compare Tag Value As Int    0
	${input_bit_2}    input_bit_2    ${modbus_node}    Compare Tag Value As Int    0
	${input_bit_3}    input_bit_3    ${modbus_node}    Compare Tag Value As Int    0
	${input_bit_4}    input_bit_4    ${modbus_node}    Compare Tag Value As Int    0
	${input_bit_5}    input_bit_5    ${modbus_node}    Compare Tag Value As Int    0	

Read some tags at time, it will be success.
	${input_register_tags}=	Create List	${input_register_int16} 	${input_register_uint16}	${input_register_int32}	${input_register_uint32}	${input_register_float}	${input_register_string}	${input_register_bit}
	Read some ${input_register_tags} from ${modbus_node}, it will be success.

	${hold_tags}=	Create List	${hold_int16}	${hold_uint16}	${hold_int32}	${hold_uint32}	${hold_float}	${hold_string}	${hold_bit}
	Read some ${hold_tags} from ${modbus_node}, it will be success.

	${coil_tags}=	Create List	${coil_bit_1}	${coil_bit_2}	${coil_bit_3}	${coil_bit_4}	${coil_bit_5}
	Read some ${coil_tags} from ${modbus_node}, it will be success.

	${input_tags}=	Create List	${input_bit_1}	${input_bit_2}	${input_bit_3}	${input_bit_4}	${input_bit_5}
	Read some ${input_tags} from ${modbus_node}, it will be success.

Write and read a tag with same name and same address that in different groups, it should be success.
	${groups}=	Create List	group-1	group-2	group-3
	Write and read a ${hold_int16} with same hold_int16 and same address in different ${groups} that from ${modbus_node}, using Compare Tag Value As Int to check the 2, it will be success.

Write and read a tag with same name but different address that in different groups, it should be success.
	${groups}=	Create List	group-1	group-2	group-3
	Write and read ${hold_int16} ${hold_int16_address_update} with same hold_int16 but different address in different ${groups} that from ${modbus_node}, using Compare Tag Value As Int to check the 2, it will be success.

Update a tag with right address and types and attribute, it will be success
	[Template]	Update a ${tag} by a ${tag_update} named ${tag_name} from ${node} , it can read and write, using ${check} to check the ${value}, it will be success.
	${hold_int16}	${hold_int16_address_update}	hold_int16	${modbus_node}	Compare Tag Value As Int  120
	${hold_int16}	${hold_int16_type_update}	hold_int16	${modbus_node}	Compare Tag Value As Int  120

Update a tag with unmatch address and types and attribute, it should be failure.
	[Template]	Update a ${tag} by a ${tag_update} with the unmatch settings from ${node} , it will be failure and return ${resp_status} and ${error_code}.
	${input_bit_1}	${input_bit_1_attribute_unmatch}	${modbus_node}	200	${NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT}
	${input_bit_1}	${input_bit_1_type_unmatch}	${modbus_node}	200	${NEU_ERR_TAG_TYPE_NOT_SUPPORT}
	${input_bit_1}	${input_bit_1_name_wrong}	${modbus_node}	200	${NEU_ERR_TAG_NOT_EXIST}

Write a value to a tag that don't exit in plc or neuron, it will be failure.
	[Template]	Write a ${value} to a ${tag} with named ${tag_name} that don't exist in neuron or plc from ${node}, it will be failure.
	12	${input_bit_1}	input	${modbus_node}

Write value to a tag without write attribute, it will be failure.
	[Template]	Write a ${value} to a ${tag} named ${tag_name} that without write attribute from ${node}, it will be failure.
	1	${input_bit_1}	input_bit_1	${modbus_node}

Write and read a value to a tag when the node is stoped, it will be failure.
	[Template]	Write and read a ${value} to a ${tag} named ${tag_name} when the ${node} is stoped, it will be failure.
	1	${hold_int16}	hold_int16	${modbus_node}

*** Keywords ***
Modbus Test Setup
	Start Modbus Simulator

	Start Neuronx

	${res}=	Add Node  ${modbus_node}	${PLUGIN-MODBUS-TCP}
	Check Response Status  ${res}  200

	${res}=	Node Setting  ${modbus_node}  ${MODBUS_TCP_CONFIG}
	Check Response Status  ${res}  200

	${res}=	Node Ctl  ${modbus_node}  ${NODE_CTL_START}
	Check Response Status  ${res}  200

	${state}=	Get Node State    ${modbus_node}
	Should Be Equal As Integers    ${state}[running]    ${NODE_STATE_RUNNING}
	Should Be Equal As Integers    ${state}[link]       ${NODE_LINK_STATE_CONNECTING}

Modbus Test Teardown
	Del Node  ${modbus_node}
	Stop Neuronx
	Terminate All Processes 	kill=false
