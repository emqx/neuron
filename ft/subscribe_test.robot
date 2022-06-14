*** Settings ***
Resource 	resource/api.resource
Resource	resource/common.resource
Resource	resource/error.resource
Suite Setup	Start Neuronx
Suite Teardown	Stop Neuronx


*** Test Cases ***
North APP subscribe non-existent group, it should return failure
	Add Node  modbus-node  ${PLUGIN-MODBUS-TCP}
	Add Node  mqtt  ${PLUGIN-MQTT}

	Sleep	1s
	${res}=		Subscribe Group  mqtt  modbus-node  group

	Check Response Status  ${res}  404
	Check Error Code  ${res}  ${NEU_ERR_GROUP_NOT_EXIST}

North APP subscribe existent group, it should return success
	Add Group  modbus-node  group  1000
	${res}=		Subscribe Group  mqtt  modbus-node  group

	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

Query the subscribers of the group, it should return all nodes subscribed to this group
	${res} =                  Get Subscribe Group		mqtt
	Check Response Status     ${res}                          200

	${len} =                  Get Length                      ${res}[groups]
 	Should Be Equal As Integers	${len}		1
	Should Be Equal As Strings	${res}[groups][0][driver]	modbus-node
	Should Be Equal As Strings	${res}[groups][0][group]	group

	Add Group  modbus-node  group1  1000
	Subscribe Group  mqtt  modbus-node  group1

	${res} =                  Get Subscribe Group		mqtt
	Check Response Status     ${res}                          200

	${len} =                  Get Length                      ${res}[groups]
 	Should Be Equal As Integers	${len}		2
	Should Be Equal As Strings	${res}[groups][0][driver]	modbus-node
	Should Be Equal As Strings	${res}[groups][0][group]	group

	Should Be Equal As Strings	${res}[groups][1][driver]	modbus-node
	Should Be Equal As Strings	${res}[groups][1][group]	group1

Delete the subscribe group config, it should return success 
	${res}=		Del Group  modbus-node  group1

	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res} =                  Get Subscribe Group		mqtt
	Check Response Status     ${res}                          200

	${len} =                  Get Length                      ${res}[groups]
 	Should Be Equal As Integers	${len}		1
	Should Be Equal As Strings	${res}[groups][0][driver]	modbus-node
	Should Be Equal As Strings	${res}[groups][0][group]	group

North APP unsubscribe group config, it should return success
	${res}=		Unsubscribe Group  mqtt  modbus-node  group

	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res} =                  Get Subscribe Group		mqtt
	Check Response Status     ${res}                          200
	${len} =                  Get Length                      ${res}[groups]
 	Should Be Equal As Integers	${len}		0

Delete DRIVER node with the subscribed group, it should return success.
	${res}=		Subscribe Group  mqtt  modbus-node  group
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=		Del Node  modbus-node
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res} =                  Get Subscribe Group		mqtt
	Check Response Status     ${res}                          200
	${len} =                  Get Length                      ${res}[groups]
 	Should Be Equal As Integers	${len}		0

	[Teardown]	Del Node  mqtt