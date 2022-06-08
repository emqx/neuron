*** Settings ***
Resource 	api.resource
Resource	common.resource
Resource	error.resource
Suite Setup	Start Neuronx
Suite Teardown	Stop Neuronx


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

