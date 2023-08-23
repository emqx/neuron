*** Settings ***
Library     Process
Library    OperatingSystem

Resource 	resource/api.resource
Resource	resource/common.resource
Resource	resource/error.resource
Resource	resource/driver.resource
Suite Setup	Modbus Test Setup
Suite Teardown	Modbus Test Teardown

*** Variables ***
${PLUGIN-MODBUS-TCP}   Modbus TCP

${modbus_tcp_node}	modbus_tcp_node

${MODBUS_TCP_CONFIG}   {"connection_mode": 0, "transport_mode": 0, "interval": 0, "host": "127.0.0.1","port": 60502,"timeout": 3000}

${MODBUS_TCP_CONFIG_WRONG} 	{"connection_mode": 0,"host": "127.0.0.1", "port": 60502}

${hold_bit}       {"name": "hold_bit", "address": "1!400001.15", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${hold_int16}     {"name": "hold_int16", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT16}}
${hold_uint16}    {"name": "hold_uint16", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_UINT16}}
${hold_int32}     {"name": "hold_int32", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT32}}
${hold_uint32}    {"name": "hold_uint32", "address": "1!400015", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_UINT32}}
${hold_float}     {"name": "hold_float", "address": "1!400017", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_FLOAT}}
${hold_string}    {"name": "hold_string", "address": "1!400020.10", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_STRING}}
${hold_int16_s}     {"name": "hold_int16_s", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_STATIC_RW}, "type": ${TAG_TYPE_INT16}, "value": 1}
${hold_uint16_s}    {"name": "hold_uint16_s", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_STATIC_RW}, "type": ${TAG_TYPE_UINT16}, "value": 1}
${hold_int32_s}     {"name": "hold_int32_s", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_STATIC_RW}, "type": ${TAG_TYPE_INT32}, "value": 1}
${hold_uint32_s}    {"name": "hold_uint32_s", "address": "1!400015", "attribute": ${TAG_ATTRIBUTE_STATIC_RW}, "type": ${TAG_TYPE_UINT32}, "value": 1}
${hold_float_s}     {"name": "hold_float_s", "address": "1!400017", "attribute": ${TAG_ATTRIBUTE_STATIC_RW}, "type": ${TAG_TYPE_FLOAT}, "value": 1.0}
${hold_string_s}    {"name": "hold_string_s", "address": "1!400020.10", "attribute": ${TAG_ATTRIBUTE_STATIC_RW}, "type": ${TAG_TYPE_STRING}, "value": "a"}

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

${input_bit_1_name_wrong}   	    {"name": "input_bit", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${input_bit_1_attribute_unmatch}    {"name": "input_bit_1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${input_bit_1_type_unmatch}   	    {"name": "input_bit_1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}

${hold_int16_decimal}     {"name": "hold_int16_decimal", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT16}, "decimal": 0.1}
${hold_uint16_decimal}    {"name": "hold_uint16_decimal", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_UINT16}, "decimal": 0.1}
${hold_int32_decimal}     {"name": "hold_int32_decimal", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT32}, "decimal": 0.1}
${hold_uint32_decimal}    {"name": "hold_uint32_decimal", "address": "1!400015", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_UINT32}, "decimal": 0.1}
${hold_float_decimal}     {"name": "hold_float_decimal", "address": "1!400017", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_FLOAT}, "decimal": 0.1}
${hold_double_decimal}    {"name": "hold_double_decimal", "address": "1!400027", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_DOUBLE}, "decimal": 0.1}

${hold_float_precision}      {"name": "hold_float_precision", "address": "1!400030", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_FLOAT}, "precision": 2}
${hold_double_precision}     {"name": "hold_double_precision", "address": "1!400040", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_DOUBLE}, "precision": 2}

${hold_int16_1}     {"name": "hold_int16_1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT16}}
${hold_int16_2}     {"name": "hold_int16_2", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT16}}
${hold_int16_3}     {"name": "hold_int16_3", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT16}}
${hold_string_tags_1}    {"name": "hold_string_tags_1", "address": "1!40001.4", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_STRING}}
${hold_string_tags_2}    {"name": "hold_string_tags_2", "address": "1!40003.4", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_STRING}}
${hold_string_tags_3}    {"name": "hold_string_tags_3", "address": "1!40005.4", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_STRING}}

*** Test Cases ***
Set a node with right settings, it should be success.
	[Template]	Set a ${node} with right ${node_settings}, it will be success.
	${modbus_tcp_node}	${MODBUS_TCP_CONFIG}

Set a node with wrong settings, it should be failure.
	[Template]	Set a ${node} with wrong ${node_settings}, it will be failure.
	${modbus_tcp_node}	${MODBUS_TCP_CONFIG_WRONG}

Set tags with multiple address and types and attribute, it should be success.
	[Template]	Add a ${tag} with right address and type and attribute from ${node}, it will be success.
	${hold_int16}	  ${modbus_tcp_node}
	${hold_uint16}	  ${modbus_tcp_node}
	${hold_int32}	  ${modbus_tcp_node}
	${hold_uint32}	  ${modbus_tcp_node}
	${hold_float}	  ${modbus_tcp_node}
	${hold_string}	  ${modbus_tcp_node}
	${hold_int16_s}	  ${modbus_tcp_node}
	${hold_uint16_s}  ${modbus_tcp_node}
	${hold_int32_s}	  ${modbus_tcp_node}
	${hold_uint32_s}  ${modbus_tcp_node}
	${hold_float_s}	  ${modbus_tcp_node}
	${hold_string_s}  ${modbus_tcp_node}

	${coil_bit_1}	${modbus_tcp_node}
	${coil_bit_2}	${modbus_tcp_node}
	${coil_bit_3}	${modbus_tcp_node}
	${coil_bit_4}	${modbus_tcp_node}
	${coil_bit_5}	${modbus_tcp_node}

	${input_register_int16}	    ${modbus_tcp_node}
	${input_register_uint16}	${modbus_tcp_node}
	${input_register_int32}	    ${modbus_tcp_node}
	${input_register_uint32}	${modbus_tcp_node}
	${input_register_float}	    ${modbus_tcp_node}
	${input_register_string}	${modbus_tcp_node}

	${input_bit_1}	${modbus_tcp_node}
	${input_bit_2}	${modbus_tcp_node}
	${input_bit_3}	${modbus_tcp_node}
	${input_bit_4}	${modbus_tcp_node}
	${input_bit_5}	${modbus_tcp_node}

Set tags with unmatch address, type or attribute, it should be failure.
	[Template]	Set a ${tag} with unmatch address, type or attribute from ${node}, it will be failure and return ${resp_status} and ${error_code}.
	${input_bit_1_type_unmatch}	        ${modbus_tcp_node}	200	 ${NEU_ERR_TAG_TYPE_NOT_SUPPORT}
	${input_bit_1_attribute_unmatch}	${modbus_tcp_node}	200	 ${NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT}	
	
Write and Read all type tag with same slave id in hold register and coil, it should be success.
	[Template]	Write and Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	${hold_int16}     hold_int16     ${modbus_tcp_node}	Compare Tag Value As Int        120
	${hold_uint16}    hold_uint16    ${modbus_tcp_node}	Compare Tag Value As Int        65535
	${hold_int32}     hold_int32     ${modbus_tcp_node}	Compare Tag Value As Int        77889912
	${hold_uint32}    hold_uint32    ${modbus_tcp_node}	Compare Tag Value As Int        88991232
	${hold_float}     hold_float     ${modbus_tcp_node}	Compare Tag Value As Float      12.34
	${hold_string}    hold_string    ${modbus_tcp_node}	Compare Tag Value As Strings    "abc123"

	${coil_bit_1}    coil_bit_1    ${modbus_tcp_node}    Compare Tag Value As Int    1
	${coil_bit_2}    coil_bit_2    ${modbus_tcp_node}    Compare Tag Value As Int    0
	${coil_bit_3}    coil_bit_3    ${modbus_tcp_node}    Compare Tag Value As Int    0
	${coil_bit_4}    coil_bit_4    ${modbus_tcp_node}    Compare Tag Value As Int    1
	${coil_bit_5}    coil_bit_5    ${modbus_tcp_node}    Compare Tag Value As Int    1

Read all type tag with same slave id in input register and input, it should be success.
	[Template]	Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	${input_register_int16}     input_register_int16     ${modbus_tcp_node}	Compare Tag Value As Int        0
	${input_register_uint16}    input_register_uint16    ${modbus_tcp_node}	Compare Tag Value As Int        0
	${input_register_int32}     input_register_int32     ${modbus_tcp_node}	Compare Tag Value As Int        0
	${input_register_uint32}    input_register_uint32    ${modbus_tcp_node}	Compare Tag Value As Int        0
	${input_register_float}     input_register_float     ${modbus_tcp_node}	Compare Tag Value As Float      0
	${input_register_string}    input_register_string    ${modbus_tcp_node}	Compare Tag Value As Strings    ""

	${input_bit_1}    input_bit_1    ${modbus_tcp_node}    Compare Tag Value As Int    0
	${input_bit_2}    input_bit_2    ${modbus_tcp_node}    Compare Tag Value As Int    0
	${input_bit_3}    input_bit_3    ${modbus_tcp_node}    Compare Tag Value As Int    0
	${input_bit_4}    input_bit_4    ${modbus_tcp_node}    Compare Tag Value As Int    0
	${input_bit_5}    input_bit_5    ${modbus_tcp_node}    Compare Tag Value As Int    0	

Read some tags at time, it will be success.
	[Template] 	Read some ${tags} from ${node}, it will be success.
	${input_register_int16},${input_register_uint16},${input_register_int32},${input_register_uint32},${input_register_float},${input_register_string},${input_register_bit} 	${modbus_tcp_node}
	${hold_int16},${hold_uint16},${hold_int32},${hold_uint32},${hold_float},${hold_string},${hold_bit} 	${modbus_tcp_node}
	${coil_bit_1},${coil_bit_2},${coil_bit_3},${coil_bit_4},${coil_bit_5} 	${modbus_tcp_node}
	${input_bit_1},${input_bit_2},${input_bit_3},${input_bit_4},${input_bit_5} 	${modbus_tcp_node}

Write and read a tag with same name and same address that in different groups, it should be success.
	${groups}=	Create List	group-1	group-2	group-3
	Write and read a ${hold_int16} with same hold_int16 and same address in different ${groups} that from ${modbus_tcp_node}, using Compare Tag Value As Int to check the 2, it will be success.

Write and read a tag with same name but different address that in different groups, it should be success.
	${groups}=	Create List	group-1	group-2	group-3
	Write and read ${hold_int16} ${hold_int16_address_update} with same hold_int16 but different address in different ${groups} that from ${modbus_tcp_node}, using Compare Tag Value As Int to check the 2, it will be success.

Update a tag with right address and types and attribute, it will be success
	[Template]	Update a ${tag} by a ${tag_update} named ${tag_name} from ${node} , it can read and write, using ${check} to check the ${value}, it will be success.
	${hold_int16}	${hold_int16_address_update}	hold_int16	${modbus_tcp_node}	Compare Tag Value As Int  120
	${hold_int16}	${hold_int16_type_update}	    hold_int16	${modbus_tcp_node}	Compare Tag Value As Int  120

Write 3 tags 1 time, it should return success
    [Template]  Write and Read ${tag_data1}, ${tag_data2} and ${tag_data3} named ${tag1}, ${tag2} and ${tag3} from ${node}, using ${check} to check the ${value1}, ${value2} and ${value3}, it will be success.
    ${hold_int16_1}     ${hold_int16_2}    ${hold_int16_3}    hold_int16_1    hold_int16_2    hold_int16_3     ${modbus_tcp_node}	  Compare Tag Value As Int        1    2    3
	${coil_bit_1}       ${coil_bit_2}      ${coil_bit_3}      coil_bit_1      coil_bit_2      coil_bit_3       ${modbus_tcp_node}    Compare Tag Value As Int        1    0    1
	${hold_string_tags_1}     ${hold_string_tags_2}    ${hold_string_tags_3}    hold_string_tags_1    hold_string_tags_2    hold_string_tags_3    ${modbus_tcp_node}	  Compare Tag Value As Strings        "a"    "b"    "c"

Add a tag with decimal, it should return success
	[Template]	Add a ${tag} with right address and type and attribute from ${node}, it will be success.
	${hold_int16_decimal}	${modbus_tcp_node}
	${hold_uint16_decimal}	${modbus_tcp_node}
	${hold_int32_decimal}	${modbus_tcp_node}
	${hold_uint32_decimal}	${modbus_tcp_node}
	${hold_float_decimal}	${modbus_tcp_node}
	${hold_double_decimal}	${modbus_tcp_node}

Write and read a value to a tag with decimal, it should return success	
	[Template]	Write and Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	${hold_int16_decimal}     hold_int16_decimal     ${modbus_tcp_node}	  Compare Tag Value As Int        120
	${hold_uint16_decimal}    hold_uint16_decimal    ${modbus_tcp_node}	  Compare Tag Value As Int        120
    ${hold_int32_decimal}     hold_int32_decimal     ${modbus_tcp_node}	  Compare Tag Value As Int        120
	${hold_uint32_decimal}    hold_uint32_decimal    ${modbus_tcp_node}	  Compare Tag Value As Int        120
	${hold_float_decimal}     hold_float_decimal     ${modbus_tcp_node}	  Compare Tag Value As Float      120.5
	${hold_double_decimal}    hold_double_decimal    ${modbus_tcp_node}	  Compare Tag Value As Float      120.5

Add a tag with precision, it should return success
	[Template]	Add a ${tag} with right address and type and attribute from ${node}, it will be success.
	${hold_float_precision}	    ${modbus_tcp_node}
	${hold_double_precision}	${modbus_tcp_node}

Write and read a value to a tag with precision, it should return success	
	[Template]	Write and Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	${hold_float_precision}     hold_float_precision     ${modbus_tcp_node}	Compare Tag Value As Float        120.5
	${hold_double_precision}    hold_double_precision    ${modbus_tcp_node}	Compare Tag Value As Float        1111111111111111111111111111111111111111.1

Update a tag with unmatch address and types and attribute, it should be failure.
	[Template]	Update a ${tag} by a ${tag_update} with the unmatch settings from ${node} , it will be failure and return ${resp_status} and ${error_code}.
	${input_bit_1}	${input_bit_1_attribute_unmatch}	${modbus_tcp_node}	200	 ${NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT}
	${input_bit_1}	${input_bit_1_type_unmatch}	        ${modbus_tcp_node}	200	 ${NEU_ERR_TAG_TYPE_NOT_SUPPORT}
	${input_bit_1}	${input_bit_1_name_wrong}	        ${modbus_tcp_node}	200	 ${NEU_ERR_TAG_NOT_EXIST}

Write a value to a tag that don't exit in plc or neuron, it will be failure.
	[Template]	Write a ${value} to a ${tag} with named ${tag_name} that don't exist in neuron or plc from ${node}, it will be failure.
	12	${input_bit_1}	input	${modbus_tcp_node}

Write value to a tag without write attribute, it will be failure.
	[Template]	Write a ${value} to a ${tag} named ${tag_name} that without write attribute from ${node}, it will be failure.
	1	${input_bit_1}	input_bit_1	${modbus_tcp_node}

Write and read a value to a tag when the node is stoped, it will be failure.
	[Template]	Write and read a ${value} to a ${tag} named ${tag_name} when the ${node} is stoped, it will be failure.
	1	${hold_int16}	hold_int16	${modbus_tcp_node}

*** Keywords ***
Modbus Test Setup
	Start Modbus Simulator

	Start Neuronx

	${res}=	Add Node  ${modbus_tcp_node}	${PLUGIN-MODBUS-TCP}
	Check Response Status  ${res}  200

	${res}=	Node Setting  ${modbus_tcp_node}  ${MODBUS_TCP_CONFIG}
	Check Response Status  ${res}  200

	${state}=	Get Node State    ${modbus_tcp_node}
	Should Be Equal As Integers    ${state}[running]    ${NODE_STATE_RUNNING}
	# Should Be Equal As Integers    ${state}[link]       ${NODE_LINK_STATE_CONNECTED}

Modbus Test Teardown
	Del Node  ${modbus_tcp_node}
	Stop Neuronx
	Terminate All Processes 	kill=false
