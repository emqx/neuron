*** Settings ***
Resource 	resource/api.resource
Resource	resource/common.resource
Resource	resource/error.resource
Resource	resource/driver.resource
Suite Setup	Modbus Test Setup
Suite Teardown	Modbus Test Teardown

*** Variables ***
${modbus_node}	modbus_node
${tag_bit}       {"name": "tag_bit", "address": "1!000001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${tag_int16}     {"name": "tag_int16", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT16}}
${tag_uint16}    {"name": "tag_uint16", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_UINT16}}
${tag_int32}     {"name": "tag_int32", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT32}}
${tag_uint32}    {"name": "tag_uint32", "address": "1!400005", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_UINT32}}
${tag_float}     {"name": "tag_float", "address": "1!400007", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_FLOAT}}
${tag_string}    {"name": "tag_string", "address": "1!400009.10", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_STRING}}

${tag1_bit}       {"name": "tag1", "address": "1!100001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${tag2_bit}       {"name": "tag2", "address": "1!100002", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_BIT}}
${tag1_int16}     {"name": "tag1", "address": "1!301021", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}
${tag2_int16}     {"name": "tag2", "address": "1!301022", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}
${tag1_uint16}    {"name": "tag1", "address": "1!301019", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_UINT16}}
${tag2_uint16}    {"name": "tag2", "address": "1!301020", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_UINT16}}
${tag1_int32}     {"name": "tag1", "address": "1!301010", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT32}}
${tag2_int32}     {"name": "tag2", "address": "1!301011", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT32}}
${tag1_uint32}    {"name": "tag1", "address": "1!301017", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_UINT32}}
${tag2_uint32}    {"name": "tag2", "address": "1!301018", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_UINT32}}
${tag1_float}     {"name": "tag1", "address": "1!300009", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_FLOAT}}
${tag2_float}     {"name": "tag2", "address": "1!300008", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_FLOAT}}

*** Test Cases ***
Write and Read different type tag, it should be success.
	[Template]	Write and Read a ${tag} named ${tag_name} from ${node}, useing ${check} to check the ${value}, it will be success.
	${tag_bit} 	tag_bit		${modbus_node}	Compare Tag Value As Int	1
	${tag_int16} 	tag_int16	${modbus_node}	Compare Tag Value As Int	120
	${tag_uint16} 	tag_uint16	${modbus_node}	Compare Tag Value As Int	65535
	${tag_int32} 	tag_int32	${modbus_node}	Compare Tag Value As Int	77889912
	${tag_uint32} 	tag_uint32	${modbus_node}	Compare Tag Value As Int	88991232
	${tag_float} 	tag_float	${modbus_node}	Compare Tag Value As Float	12.34
	${tag_string} 	tag_string	${modbus_node}	Compare Tag Value As Strings	"abc123"




*** Keywords ***
Modbus Test Setup
	Start Modbus Simulator

	Start Neuronx

	${res}=		Add Node  ${modbus_node}	${PLUGIN-MODBUS-TCP}
	Check Response Status  ${res}  200

	${res}=		Node Setting  ${modbus_node}  ${MODBUS_TCP_CONFIG}
	Check Response Status  ${res}  200

	${res}=		Node Ctl  ${modbus_node}  ${NODE_CTL_START}
	Check Response Status  ${res}  200

    	${state} =    Get Node State    ${modbus_node}
    	Should Be Equal As Integers    ${state}[running]    ${NODE_STATE_RUNNING}
    	Should Be Equal As Integers    ${state}[link]       ${NODE_LINK_STATE_CONNECTING}

Modbus Test Teardown
	Del Node  ${modbus_node}
	Stop Neuronx
    	Terminate All Processes    kill=false
	







