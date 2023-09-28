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

North APP update subscribe nonexistent group, it should return success
	${res}=     Update Subscribe    mqtt        modbus-node   group1  topic=/neuron

	Check Response Status           ${res}      404
	Check Error Code                ${res}      ${NEU_ERR_GROUP_NOT_SUBSCRIBE}

North APP update subscribe existent group, it should return success
	${res}=     Update Subscribe    mqtt        modbus-node   group   topic=/neuron

	Check Response Status           ${res}      200
	Check Error Code                ${res}      ${NEU_ERR_SUCCESS}

North APP subscribe multiple nonexistent groups, it should fail.
	${g1} =     Evaluate            {"driver":"modbus-node", "group": "group1"}
	${g2} =     Evaluate            {"driver": "modbus-node", "group": "group2", "params":{"topic":"/neuron"}}

	${res}=     Subscribe Groups    mqtt        ${g1}         ${g2}

	Check Response Status           ${res}      404
	Check Error Code                ${res}      ${NEU_ERR_GROUP_NOT_EXIST}

North MQTT APP subscribe with empty topic, it should fail.
	${g1} =     Evaluate            {"driver": "modbus-node", "group": "group1", "params":{"topic":""}}

	${res}=     Subscribe Groups    mqtt        ${g1}

	Check Response Status           ${res}      200
	Check Error Code                ${res}      ${NEU_ERR_MQTT_SUBSCRIBE_FAILURE}

North APP subscribe multiple groups, it should return success
	Add Group   modbus-node         group1      1000
	Add Group   modbus-node         group2      1000
	${g1} =     Evaluate            {"driver":"modbus-node", "group": "group1"}
	${g2} =     Evaluate            {"driver": "modbus-node", "group": "group2", "params":{"topic":"/neuron"}}

	${res}=     Subscribe Groups    mqtt        ${g1}         ${g2}

	Check Response Status           ${res}      200
	Check Error Code                ${res}      ${NEU_ERR_SUCCESS}

  [Teardown]  Run Keywords        Del Group   modbus-node   group1
  ...         AND                 Del Group   modbus-node   group2

Query the subscribers of the group, it should return all nodes subscribed to this group
	${res} =                  Get Subscribe Group		mqtt
	Check Response Status     ${res}                          200

	${len} =                  Get Length                      ${res}[groups]
 	Should Be Equal As Integers	${len}		1
	Should Be Equal As Strings	${res}[groups][0][driver]	modbus-node
	Should Be Equal As Strings	${res}[groups][0][group]	group
	Should Be Equal As Strings	${res}[groups][0][params][topic]	/neuron

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