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
${input_bit_1_attribute_wrong}    {"name": "input_bit_1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${input_bit_1_type_wrong}   	  {"name": "input_bit_1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}

*** Test Cases ***
# Read test.
# 	${tags} = 	Create List 	${input_register_int16} 	${input_register_uint16} 		

#     Read some ${tags} from ${modbus_node}, it will be success.



Write and Read all type tag with same slave id in hold register, it should be success.
	[Template]	Write and Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	${hold_int16}     hold_int16     ${modbus_node}	Compare Tag Value As Int        120
	${hold_uint16}    hold_uint16    ${modbus_node}	Compare Tag Value As Int        65535
	${hold_int32}     hold_int32     ${modbus_node}	Compare Tag Value As Int        77889912
	${hold_uint32}    hold_uint32    ${modbus_node}	Compare Tag Value As Int        88991232
	${hold_float}     hold_float     ${modbus_node}	Compare Tag Value As Float      12.34
	${hold_string}    hold_string    ${modbus_node}	Compare Tag Value As Strings    "abc123"

Write and Read all type tag with same slave id in coil, it should be success.
	[Template]    Write and Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	${coil_bit_1}    coil_bit_1    ${modbus_node}    Compare Tag Value As Int    1
	${coil_bit_2}    coil_bit_2    ${modbus_node}    Compare Tag Value As Int    0
	${coil_bit_3}    coil_bit_3    ${modbus_node}    Compare Tag Value As Int    0
	${coil_bit_4}    coil_bit_4    ${modbus_node}    Compare Tag Value As Int    1
	${coil_bit_5}    coil_bit_5    ${modbus_node}    Compare Tag Value As Int    1

Read all type tag with same slave id in input register, it should be success.
	[Template]	Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	${input_register_int16}     input_register_int16     ${modbus_node}	Compare Tag Value As Int        0
	${input_register_uint16}    input_register_uint16    ${modbus_node}	Compare Tag Value As Int        0
	${input_register_int32}     input_register_int32     ${modbus_node}	Compare Tag Value As Int        0
	${input_register_uint32}    input_register_uint32    ${modbus_node}	Compare Tag Value As Int        0
	${input_register_float}     input_register_float     ${modbus_node}	Compare Tag Value As Float      0
	${input_register_string}    input_register_string    ${modbus_node}	Compare Tag Value As Strings    ""	

Read all type tag with same slave id in input, it should be success.
	[Template]    Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	${input_bit_1}    input_bit_1    ${modbus_node}    Compare Tag Value As Int    0
	${input_bit_2}    input_bit_2    ${modbus_node}    Compare Tag Value As Int    0
	${input_bit_3}    input_bit_3    ${modbus_node}    Compare Tag Value As Int    0
	${input_bit_4}    input_bit_4    ${modbus_node}    Compare Tag Value As Int    0
	${input_bit_5}    input_bit_5    ${modbus_node}    Compare Tag Value As Int    0

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
