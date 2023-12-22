*** Settings ***
Resource 	resource/api.resource
Resource	resource/common.resource
Resource	resource/error.resource
Suite Setup	Start Neuronx
Suite Teardown	Stop Neuronx

*** Variables ***
${tag1}             {"name": "tag1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}
${tag2}             {"name": "tag2", "address": "1!00001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${tag3}             {"name": "tag3", "address": "1!00001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_UINT16}}
${tag4}             {"name": "tag4", "address": "1!00031", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${tag1update}       {"name": "tag1", "address": "1!400002", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT32}}
${tag_desc}         {"name": "tag_desc", "description": "description info", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}
${tag1_static}               {"name": "tag1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_STATIC_RW}, "type": ${TAG_TYPE_INT16}, "value": 1}
${tag1_static_no_value}      {"name": "tag1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_STATIC_RW}, "type": ${TAG_TYPE_INT16}}
${tag_decimal}               {"name": "tag_decimal", "address": "1!400003", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_INT16}, "decimal": 0.1}


*** Test Cases ***
Add tags to non-existent node, it should return failure
  	${res}=	Add Tags	modbus-node	group	${tag1},${tag2}

  	Check Response Status           ${res}        404
  	Check Error Code                ${res}        ${NEU_ERR_NODE_NOT_EXIST}

Add tags to the non-existent group, it should return success
	Add Node  modbus-node  ${PLUGIN-MODBUS-TCP}
  	${res}=	Add Tags	modbus-node	group	${tag1},${tag2}

  	Check Response Status           ${res}        200
  	Check Error Code                ${res}        ${NEU_ERR_SUCCESS}
	[Teardown]	Del Tags  modbus-node  group  "tag1","tag2"

Add static tag, it should return success.
  	${res}=	Add Tags	modbus-node	group	${tag1_static}

  	Check Response Status           ${res}        200
  	Check Error Code                ${res}        0

  	[Teardown]	Del Tags  modbus-node  group  "tag1"

Add static tag without value, it should fail.
  	${res}=	Add Tags	modbus-node	group	${tag1_static_no_value}

  	Check Response Status           ${res}        400
  	Check Error Code                ${res}        ${NEU_ERR_BODY_IS_WRONG}

  	[Teardown]	Del Tags  modbus-node  group  "tag1"

Add different types of tags to the group, it should return success
	Add Group  modbus-node  group  5000
  	${res}=	Add Tags	modbus-node	group	${tag1},${tag2}

  	Check Response Status           ${res}        200
  	Check Error Code                ${res}        ${NEU_ERR_SUCCESS}
	Should Be Equal As Integers	${res}[index]	2

Add a list of tags with errors, it should return the number of successful additions.
  	${res}=	Add Tags	modbus-node	group	${tag1},${tag2},${tag3},${tag4}

  	Check Response Status           ${res}        409
  	Check Error Code                ${res}        ${NEU_ERR_TAG_NAME_CONFLICT}
	Should Be Equal As Integers	${res}[index]	0

Get tag list from modbus-node, it should return success.
	${res}=		Get Tags  modbus-node  group

  	Check Response Status           ${res}        200
	${len} =	Get Length	${res}[tags]
 	Should Be Equal As Integers	${len}		2
	
	Should Be Equal As Strings	${res}[tags][0][name]		tag1
	Should Be Equal As Strings	${res}[tags][0][address]	1!400001
	Should Be Equal As Strings	${res}[tags][0][attribute]	${TAG_ATTRIBUTE_READ}
	Should Be Equal As Strings	${res}[tags][0][type]		${TAG_TYPE_INT16}

	Should Be Equal As Strings	${res}[tags][1][name]		tag2
	Should Be Equal As Strings	${res}[tags][1][address]	1!00001
	Should Be Equal As Strings	${res}[tags][1][attribute]	${TAG_ATTRIBUTE_RW}
	Should Be Equal As Strings	${res}[tags][1][type]		${TAG_TYPE_BIT}

Update tag to be static, it should return success.
	${res}=		Update Tags  modbus-node  group  ${tag1_static}

  	Check Response Status           ${res}        200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=		Get Tags  modbus-node  group

  	Check Response Status           ${res}        200
	${len} =	Get Length	${res}[tags]
 	Should Be Equal As Integers	${len}		2
	
	Should Be Equal As Strings	${res}[tags][0][name]		tag1
	Should Be Equal As Strings	${res}[tags][0][address]	1!400001
	Should Be Equal As Strings	${res}[tags][0][attribute]	${TAG_ATTRIBUTE_STATIC_RW}
	Should Be Equal As Strings	${res}[tags][0][type]		${TAG_TYPE_INT16}

	Should Be Equal As Strings	${res}[tags][1][name]		tag2
	Should Be Equal As Strings	${res}[tags][1][address]	1!00001
	Should Be Equal As Strings	${res}[tags][1][attribute]	${TAG_ATTRIBUTE_RW}
	Should Be Equal As Strings	${res}[tags][1][type]		${TAG_TYPE_BIT}

Update static tag without value, it should fail.
  	${res}=		Update Tags  modbus-node  group  ${tag1_static_no_value}

  	Check Response Status           ${res}        400
  	Check Error Code                ${res}        ${NEU_ERR_BODY_IS_WRONG}

Update tag, it should return success.
	${res}=		Update Tags  modbus-node  group  ${tag1update}

  	Check Response Status           ${res}        200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=		Get Tags  modbus-node  group

  	Check Response Status           ${res}        200
	${len} =	Get Length	${res}[tags]
 	Should Be Equal As Integers	${len}		2
	
	Should Be Equal As Strings	${res}[tags][0][name]		tag1
	Should Be Equal As Strings	${res}[tags][0][address]	1!400002
	Should Be Equal As Strings	${res}[tags][0][attribute]	${TAG_ATTRIBUTE_RW}
	Should Be Equal As Strings	${res}[tags][0][type]		${TAG_TYPE_INT32}

	Should Be Equal As Strings	${res}[tags][1][name]		tag2
	Should Be Equal As Strings	${res}[tags][1][address]	1!00001
	Should Be Equal As Strings	${res}[tags][1][attribute]	${TAG_ATTRIBUTE_RW}
	Should Be Equal As Strings	${res}[tags][1][type]		${TAG_TYPE_BIT}

Delete tag from non-existent group, it should return success
	${res}=		Del Tags  modbus-node  group1  "tag1"

  	Check Response Status           ${res}        200
  	Check Error Code                ${res}        ${NEU_ERR_SUCCESS}

Update non-existent tag, it should return failure.
	${res}= 	Update Tags  modbus-node  group  ${tag4}

  	Check Response Status           ${res}        404
  	Check Error Code                ${res}        ${NEU_ERR_TAG_NOT_EXIST}

	${res}=		Get Tags  modbus-node  group

  	Check Response Status           ${res}        200
	${len} =	Get Length	${res}[tags]
 	Should Be Equal As Integers	${len}		2
	
	Should Be Equal As Strings	${res}[tags][0][name]		tag1
	Should Be Equal As Strings	${res}[tags][0][address]	1!400002
	Should Be Equal As Strings	${res}[tags][0][attribute]	${TAG_ATTRIBUTE_RW}
	Should Be Equal As Strings	${res}[tags][0][type]		${TAG_TYPE_INT32}

	Should Be Equal As Strings	${res}[tags][1][name]		tag2
	Should Be Equal As Strings	${res}[tags][1][address]	1!00001
	Should Be Equal As Strings	${res}[tags][1][attribute]	${TAG_ATTRIBUTE_RW}
	Should Be Equal As Strings	${res}[tags][1][type]		${TAG_TYPE_BIT}

Delete tags, it should return success
	Del Tags  modbus-node  group  "tag1","tag2"

	${res}=		Get Tags  modbus-node  group

  	Check Response Status           ${res}        200
	${len} =	Get Length	${res}[tags]
 	Should Be Equal As Integers	${len}		0

Add a tag with description, it should return success
  	${res}=	Add Tags	modbus-node	group	${tag_desc}

  	Check Response Status           ${res}        200
  	Check Error Code                ${res}        ${NEU_ERR_SUCCESS}
	Should Be Equal As Integers	${res}[index]	1

	${res}=		Get Tags  modbus-node  group

  	Check Response Status           ${res}        200
	${len} =	Get Length	${res}[tags]
 	Should Be Equal As Integers	${len}		1
	
	Should Be Equal As Strings	${res}[tags][0][name]		tag_desc
	Should Be Equal As Strings	${res}[tags][0][address]	1!400001
	Should Be Equal As Strings	${res}[tags][0][description]	description info
	Should Be Equal As Strings	${res}[tags][0][attribute]	${TAG_ATTRIBUTE_READ}
	Should Be Equal As Strings	${res}[tags][0][type]		${TAG_TYPE_INT16}

	[Teardown]	Del Tags  modbus-node  group  "tag_desc"

Add a tag with decimal, it should return success
  	${res}=	Add Tags	modbus-node	group	${tag_decimal}

  	Check Response Status           ${res}        200
  	Check Error Code                ${res}        ${NEU_ERR_SUCCESS}
	Should Be Equal As Integers	${res}[index]	1

	${res}=		Get Tags  modbus-node  group

  	Check Response Status           ${res}        200
	${len} =	Get Length	${res}[tags]
 	Should Be Equal As Integers	${len}		1
	
	Should Be Equal As Strings	${res}[tags][0][name]		tag_decimal
	Should Be Equal As Strings	${res}[tags][0][address]	1!400003
	Should Be Equal As Strings	${res}[tags][0][decimal]	0.1
	Should Be Equal As Strings	${res}[tags][0][attribute]	${TAG_ATTRIBUTE_RW}
	Should Be Equal As Strings	${res}[tags][0][type]		${TAG_TYPE_INT16}

