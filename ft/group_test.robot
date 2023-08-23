*** Settings ***
Resource 	resource/api.resource
Resource	resource/common.resource
Resource	resource/error.resource
Suite Setup	Start Neuronx
Suite Teardown	Stop Neuronx

*** Variables ***
${g_long_str}       ${{'a'*200}}

*** Test Cases ***
Add group under non-existent node, it should return failure
	${res}=		Add Group  modbus-node1  group  1000

  	Check Response Status     ${res}	404
  	Check Error Code          ${res}	${NEU_ERR_NODE_NOT_EXIST}

Add group under existent node, it should return success
	Add Node  modbus-node  ${PLUGIN-MODBUS-TCP}
	${res}=		Add Group  modbus-node  group  1000

  	Check Response Status     ${res}	200
  	Check Error Code          ${res}	${NEU_ERR_SUCCESS}

Get the group from this node, it should return the group config
	${res}=		Get Group  modbus-node

  	Check Response Status     ${res}	200
  	${len} =	Get Length	${res}[groups]
  	Should Be Equal As Integers	${len}	1

	Should Be Equal As Strings	${res}[groups][0][name]		group
	Should Be Equal As Strings	${res}[groups][0][interval]	1000
	Should Be Equal As Strings	${res}[groups][0][tag_count]	0

Delete a non-existent group, it should return failure
	${res}=		Del Group  modbus-node  group1

  	Check Response Status     ${res}	404
  	Check Error Code          ${res}	${NEU_ERR_GROUP_NOT_EXIST}

Delete a group from non-existent node, it should return failure
	${res}=		Del Group  modbus-node1  group

  	Check Response Status     ${res}	404
  	Check Error Code          ${res}	${NEU_ERR_NODE_NOT_EXIST}

Delete a group, it should return success
	${res}=		Del Group  modbus-node  group

  	Check Response Status     ${res}	200
  	Check Error Code          ${res}	${NEU_ERR_SUCCESS}

Add group with interval < 100 under existent node, it should return failure 
	${res}=		Add Group  modbus-node  group  99

  	Check Response Status     ${res}	400
  	Check Error Code          ${res}	${NEU_ERR_GROUP_PARAMETER_INVALID}
	
	[Teardown]	Del Node  modbus-node

Add group under app node, it should return failure 
	Add Node  mqtt  ${PLUGIN-MQTT}
	${res}=		Add Group  mqtt  group  1000

  	Check Response Status     ${res}	409
  	Check Error Code          ${res}	${NEU_ERR_GROUP_NOT_ALLOW}
	[Teardown]	Del Node  mqtt

Update group with nonexistent node, it should fail.
  ${res} =                      Update Group          modbus-node           group           interval=${1000}
  Check Response Status         ${res}                404
  Check Error Code              ${res}                ${NEU_ERR_NODE_NOT_EXIST}

  [Teardown]                    Del Node              modbus-node

Update group using too long node name, it should fail.
  ${res} =                      Update Group          ${g_long_str}         group           interval=${1000}
  Check Response Status         ${res}                400
  Check Error Code              ${res}                ${NEU_ERR_NODE_NAME_TOO_LONG}

  [Teardown]                    Del Node              modbus-node

Update group with nonexistent group, it should fail.
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}

  ${res} =                      Update Group          modbus-node           group           interval=${1000}
  Check Response Status         ${res}                404
  Check Error Code              ${res}                ${NEU_ERR_GROUP_NOT_EXIST}

  [Teardown]                    Del Node              modbus-node

Update group using to long group name, it should fail.
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}

  ${res} =                      Update Group          modbus-node           ${g_long_str}   interval=${1000}
  Check Response Status         ${res}                400
  Check Error Code              ${res}                ${NEU_ERR_GROUP_NAME_TOO_LONG}

  [Teardown]                    Del Node              modbus-node

Update group with too small interval, it should fail.
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}
  Add Group                     modbus-node           group                 1000

  ${res} =                      Update Group          modbus-node           group         interval=${10}
  Check Response Status         ${res}                400
  Check Error Code              ${res}                ${NEU_ERR_GROUP_PARAMETER_INVALID}

  [Teardown]                    Del Node              modbus-node

Update group with too large interval, it should fail.
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}
  Add Group                     modbus-node           group                 1000

  ${res} =                      Update Group          modbus-node           group         interval=${100000000000}
  Check Response Status         ${res}                400
  Check Error Code              ${res}                ${NEU_ERR_GROUP_PARAMETER_INVALID}

  [Teardown]                    Del Node              modbus-node

Update group interval, it should success.
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}
  Add Group                     modbus-node           group                 1000

  ${res} =                      Update Group          modbus-node           group         interval=${2000}
  Check Response Status         ${res}                200
  Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

  ${res} =                      Get Group             modbus-node
  Set Local Variable            ${groups}             ${res}[groups]
  Should Match Group            ${groups}             group                 ${2000}

  [Teardown]                    Del Node              modbus-node

Update group with empty new name, it should fail.
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}
  Add Group                     modbus-node           group                 1000

  ${res} =                      Update Group          modbus-node           group         new_name=${EMPTY}
  Check Response Status         ${res}                400
  Check Error Code              ${res}                ${NEU_ERR_BODY_IS_WRONG}

  [Teardown]                    Del Node              modbus-node

Update group with too long new name, it should fail.
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}
  Add Group                     modbus-node           group                 1000

  ${res} =                      Update Group          modbus-node           group         new_name=${g_long_str}
  Check Response Status         ${res}                400
  Check Error Code              ${res}                ${NEU_ERR_GROUP_NAME_TOO_LONG}

  [Teardown]                    Del Node              modbus-node

Update group name to conflicting group name, it should fail.
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}
  Add Group                     modbus-node           group                 1000
  Add Group                     modbus-node           grp                   1000

  ${res} =                      Update Group          modbus-node           group         new_name=grp
  Check Response Status         ${res}                409
  Check Error Code              ${res}                ${NEU_ERR_GROUP_EXIST}

  [Teardown]                    Del Node              modbus-node

Update group name, it should success.
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}
  Add Group                     modbus-node           group                 1000

  ${res} =                      Update Group          modbus-node           group         new_name=grp
  Check Response Status         ${res}                200
  Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

  ${res} =                      Get Group             modbus-node
  Set Local Variable            ${groups}             ${res}[groups]
  Should Match Group            ${groups}             grp                   ${1000}

  [Teardown]                    Del Node              modbus-node

Update group name and interval, it should success.
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}
  Add Group                     modbus-node           group                 1000

  ${res} =                      Update Group          modbus-node           group         new_name=grp   interval=${2000}
  Check Response Status         ${res}                200
  Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

  ${res} =                      Get Group             modbus-node
  Set Local Variable            ${groups}             ${res}[groups]
  Should Match Group            ${groups}             grp                   ${2000}

  [Teardown]                    Del Node              modbus-node

Update group name, subscriptions should be updated.
  Add Node                      mqtt-node             ${PLUGIN-MQTT}
  Add Node                      modbus-node           ${PLUGIN-MODBUS-TCP}
  Add Group                     modbus-node           group                 1000
  Subscribe Group               mqtt-node             modbus-node           group

  ${res} =                      Update Group          modbus-node           group         new_name=grp
  Check Response Status         ${res}                200
  Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

  ${res} =                      Get Subscribe Group   mqtt-node
  Check Response Status         ${res}                200
  Set Local Variable            ${groups}             ${res}[groups]
  List Length Should Be         ${groups}             1
  Should Be Equal As Strings    ${groups}[0][driver]  modbus-node
  Should Be Equal As Strings    ${groups}[0][group]   grp

  [Teardown]                    Del Node              modbus-node

*** Keywords ***

Should Match Group
  [Arguments]                   ${groups}             ${name}                       ${interval}
  List Length Should Be         ${groups}             1
  Should Be Equal As Strings    ${name}               ${groups}[0][name]
  Should Be Equal As Integers   ${interval}           ${groups}[0][interval]
