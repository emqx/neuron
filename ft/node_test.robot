*** Settings ***
Resource 	resource/api.resource
Resource	resource/common.resource
Resource	resource/error.resource
Suite Setup	Start Neuronx
Suite Teardown	Stop Neuronx


*** Test Cases ***
POST ping, status can be ok
	POST	/api/v2/ping
	Integer		response status		200
	Object 		response body 		{}

Create a DRIVER node with a plugin that does not exist, it should be fail.
	${res}=		Add Node  modbus-node	modbus-plugin

	Check Response Status  ${res}	404
	Check Error Code  ${res}  ${NEU_ERR_LIBRARY_NOT_FOUND}

Get UNKNOWN type node, it should be fail.
	${res}= 	Get Nodes  3

	Check Response Status  ${res}	400

Create a DRIVER with an existing plugin, it should be success.
	${res}=		Add Node  modbus-node  ${PLUGIN-MODBUS-TCP}

	Check Response Status  ${res}	200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

Get DRIVER node, it should be success.
	${res}= 	Get Nodes  ${NODE_DRIVER}

	Check Response Status  ${res}	200
	Node Should Exist    ${res}[nodes]    modbus-node

Get APP node, it should empty.
	${res}= 	Get Nodes  ${NODE_APP}

	Check Response Status  ${res}	200
	Node Should Not Exist    ${res}[nodes]    modbus-node

Create a node with with the same name, it should be fail.
	${res}=		Add Node  modbus-node  ${PLUGIN-MODBUS-TCP}

	Check Response Status  ${res}	409
	Check Error Code  ${res}  ${NEU_ERR_NODE_EXIST}

Create a APP with an existing plugin, it should be success.
	${res}=		Add Node  mqtt-node  ${PLUGIN-MQTT}

	Check Response Status  ${res}	200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

Get APP node, it should be success.
	${res}= 	Get Nodes  ${NODE_APP}

	Check Response Status  ${res}	200
	Node Should Exist    ${res}[nodes]    mqtt-node

Update node name to empty string, it should fail.
	${res} =    Update Node    modbus-node                ${EMPTY}

	Check Response Status    ${res}    400
	Check Error Code         ${res}    ${NEU_ERR_NODE_NAME_EMPTY}

Update node name to excessive long string, it should fail.
	${name} =   Evaluate       'a'*200
	${res} =    Update Node    modbus-node                ${name}

	Check Response Status    ${res}    400
	Check Error Code         ${res}    ${NEU_ERR_NODE_NAME_TOO_LONG}

Update driver node name, it should fail if the node does not exist
	${res} =    Update Node    modbus-node-nonexistent    mqtt-node

	Check Response Status    ${res}    404
	Check Error Code         ${res}    ${NEU_ERR_NODE_NOT_EXIST}

Update driver node name, it should fail if node name conflicts
	${res} =    Update Node    modbus-node    mqtt-node

	Check Response Status    ${res}    409
	Check Error Code         ${res}    ${NEU_ERR_NODE_EXIST}

Update node name to the same, it should fail as name conflicts
	${res} =    Update Node    modbus-node    modbus-node
	Check Response Status    ${res}    409
	Check Error Code         ${res}    ${NEU_ERR_NODE_EXIST}

	${res} =    Update Node    mqtt-node      mqtt-node
	Check Response Status    ${res}    409
	Check Error Code         ${res}    ${NEU_ERR_NODE_EXIST}

Update driver node name, it should success
	${res} =    Update Node    modbus-node    modbus-node-new
	Check Response Status    ${res}    200
	Check Error Code         ${res}    ${NEU_ERR_SUCCESS}

	${res} =    Update Node    modbus-node-new    modbus-node
	Check Response Status    ${res}    200
	Check Error Code         ${res}    ${NEU_ERR_SUCCESS}

Update app node name, it should success
	${res} =    Update Node    mqtt-node      mqtt-node-new
	Check Response Status    ${res}    200
	Check Error Code         ${res}    ${NEU_ERR_SUCCESS}

	${res} =    Update Node    mqtt-node-new  mqtt-node
	Check Response Status    ${res}    200
	Check Error Code         ${res}    ${NEU_ERR_SUCCESS}

Delete a APP node with an existing name, it should be success.
	${res}=		Del Node  mqtt-node

	Check Response Status  ${res}	200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}= 	Get Nodes  ${NODE_APP}

	Check Response Status  ${res}	200
	Node Should Not Exist    ${res}[nodes]    mqtt-node

Delete a node with an not existing name, it should be fail.
	${res}=		Del Node  mqtt-node

	Check Response Status  ${res}	404
	Check Error Code  ${res}  ${NEU_ERR_NODE_NOT_EXIST}

Delete a DRIVER node with an existing name, it should be success.
	${res}=		Del Node  modbus-node

	Check Response Status  ${res}	200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}= 	Get Nodes  ${NODE_DRIVER}

	Check Response Status  ${res}	200
	Node Should Not Exist    ${res}[nodes]    modbus-node

Add setting to non-existent node, it should return failure
	${res} =    Node Setting    modbus-node    ${MODBUS_TCP_CONFIG}

	Check Response Status    ${res}    404
	Check Error Code         ${res}    ${NEU_ERR_NODE_NOT_EXIST}

Get setting from non-existent node, it should return failure
	${res} =    Get Node Setting    modbus-node

	Check Response Status    ${res}    404
	Check Error Code         ${res}    ${NEU_ERR_NODE_NOT_EXIST}

Get setting from a node that has never been set, it should return failure
	${res} =    Add Node   modbus-node 	${PLUGIN-MODBUS-TCP}

	Check Response Status  ${res}	200
	${res} =               Get Node Setting		modbus-node

    	Check Response Status    ${res}    200
    	Check Error Code         ${res}    ${NEU_ERR_NODE_SETTING_NOT_FOUND}

    	[Teardown]    Del Node		modbus-node

Add the correct settings to the node, it should return success
	${res} =    Add Node   modbus-node 	${PLUGIN-MODBUS-TCP}
	Check Response Status  ${res}	200

	${res} =    Node Setting    modbus-node    ${MODBUS_TCP_CONFIG}
    	Check Response Status    ${res}    200
    	Check Error Code         ${res}    ${NEU_ERR_SUCCESS}

Get the setting of node, it should return to the previous setting
    	${res} =               Get Node Setting    modbus-node
    	${params} =            Set Variable        ${res}[params]

	Check Response Status          ${res}                        200
    	Should Be Equal As Strings    ${res}[node]                  modbus-node
    	Should Be Equal As Integers    ${params}[port]               60502
    	Should Be Equal As Strings     ${params}[host]               127.0.0.1
    	Should Be Equal As Integers    ${params}[timeout]            3000

    	[Teardown]    Del Node		modbus-node