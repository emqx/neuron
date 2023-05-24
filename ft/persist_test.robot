*** Settings ***
Resource 	resource/api.resource
Resource	resource/common.resource
Resource	resource/error.resource
Suite Setup       Neuron Context Ready 
Suite Teardown    Neuron Context Stop

*** Variables ***
${test_gconfig1}        test_gconfig1
${test_gconfig2}        test_gconfig2
${test_gconfig3}        test_gconfig3
${modbus-tcp-1}         modbus-tcp-1
${modbus-tcp-2}         modbus-tcp-2
${modbus-tcp-3}         modbus-tcp-3
${modbus-tcp-4}         modbus-tcp-4
${modbus-tcp-1-new}     modbus-tcp-1-new
${mqtt-1}               mqtt-1
${mqtt-1-new}           mqtt-1-new
${tag1}             {"name": "tag1", "address": "1!400001", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}
${tag2}             {"name": "tag2", "address": "1!00001", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_TYPE_BIT}}
${tag3}             {"name": "tag3", "address": "2!400003", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT32}}
${tag4}             {"name": "tag4", "address": "2!400005", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}
${tag5}             {"name": "tag5", "address": "2!400006", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}
${tag6}             {"name": "tag6", "address": "2!400007", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}
${tag7}             {"name": "tag7", "address": "2!400008", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}
${tag8}             {"name": "tag8", "address": "2!400009", "attribute": ${TAG_ATTRIBUTE_READ}, "type": ${TAG_TYPE_INT16}}

*** Test Cases ***
Get the newly added adapters after stopping and restarting Neuron, it should return success
	${process} =    Neuron Start Running

	Add Node	${modbus-tcp-1}		${PLUGIN-MODBUS-TCP} 
	Add Node	${modbus-tcp-2}		${PLUGIN-MODBUS-TCP} 
	Add Node	${modbus-tcp-3}		${PLUGIN-MODBUS-TCP} 
	Add Node	${modbus-tcp-4}		${PLUGIN-MODBUS-TCP} 

	Add Node    ${${mqtt-1}}		${PLUGIN-MQTT} 

	Neuron Stop Running    ${process}
	${process} =           Neuron Start Running 

	${res} =                       Get Nodes        ${NODE_DRIVER}
	Check Response Status          ${res}           200
	Node Should Exist    ${res}[nodes]    ${modbus-tcp-1}
	Node Should Exist    ${res}[nodes]    ${modbus-tcp-2}
	Node Should Exist    ${res}[nodes]    ${modbus-tcp-3}
	Node Should Exist    ${res}[nodes]    ${modbus-tcp-4}

	${res} =                       Get Nodes        ${NODE_APP}
	Check Response Status          ${res}           200
	Node Should Exist    ${res}[nodes]    ${mqtt-1}

	[Teardown]    Neuron Stop Running    ${process}

Get the deleted adapter after stopping and restarting Neuron, it should return failure
	${process} =    Neuron Start Running
	Del Node	${modbus-tcp-4}

    	Neuron Stop Running    ${process}
    	${process} =           Neuron Start Running    

    	${res} =                       Get Nodes            ${NODE_DRIVER}

    	Node Should Exist        ${res}[nodes]    ${modbus-tcp-1}
    	Node Should Exist        ${res}[nodes]    ${modbus-tcp-2}
    	Node Should Exist        ${res}[nodes]    ${modbus-tcp-3}
    	Node Should Not Exist    ${res}[nodes]    ${modbus-tcp-4}

    	${res} =                       Get Nodes        ${NODE_APP}
    	Node Should Exist    ${res}[nodes]    ${mqtt-1}

    	[Teardown]    Neuron Stop Running    ${process}

Get the setted adapters information after stopping and restarting Neuron, it should return the correct information
    	${process} =    Neuron Start Running

    	Node Setting	${modbus-tcp-1}		{"host": "127.0.0.1", "port": 502, "timeout": 3000, "connection_mode": 0, "transport_mode": 0, "interval": 20}
    	Node Setting	${modbus-tcp-2}		{"host": "127.0.0.1", "port": 502, "timeout": 3000, "connection_mode": 0, "transport_mode": 0, "interval": 20}
    	Node Setting    ${mqtt-1}     		{"client-id":"123456", "upload-topic":"/neuron/mqtt-1/upload", "heartbeat-topic": "/neuron/mqtt-1/heartbeat","format": 0,"cache-mem-size": 10,"cache-disk-size": 10,"ssl":false,"host":"127.0.0.1","port":1883,"username":"admin","password":"0000","ca":"", "cert":"", "key":"", "keypass":""}

    	Neuron Stop Running    ${process}
    	${process} =           Neuron Start Running

    	${res} =                       Get Node Setting              ${modbus-tcp-1}
    	${params} =                    Set Variable                  ${res}[params]
    	Check Response Status          ${res}                        200
    	Should Be Equal As Strings     ${res}[node]               	 ${modbus-tcp-1}
    	Should Be Equal As Integers    ${params}[port]               502
    	Should Be Equal As Integers    ${params}[timeout]            3000
    	Should Be Equal As Strings     ${params}[host]               127.0.0.1

    	${res} =                       Get Node Setting              ${modbus-tcp-2}
    	${params} =                    Set Variable                  ${res}[params]
    	Check Response Status          ${res}                        200
    	Should Be Equal As Strings     ${res}[node]                  ${modbus-tcp-2}
    	Should Be Equal As Integers    ${params}[port]               502
    	Should Be Equal As Integers    ${params}[timeout]            3000
    	Should Be Equal As Strings     ${params}[host]               127.0.0.1

    	${res} =                       Get Node Setting        ${mqtt-1}
    	${params} =                    Set Variable            ${res}[params]
    	Check Response Status          ${res}                  200
    	Should Be Equal As Strings     ${res}[node]            ${mqtt-1}
    	Should Be Equal As Strings     ${params}[upload-topic]  /neuron/mqtt-1/upload
    	Should Be Equal As Strings     ${params}[heartbeat-topic]  /neuron/mqtt-1/heartbeat
    	Should Be Equal As Integers    ${params}[format]       0 
    	Should Not Be True             ${params}[ssl]          
    	Should Be Equal As Strings     ${params}[host]         127.0.0.1
    	Should Be Equal As Integers    ${params}[port]         1883
    	Should Be Equal As Strings     ${params}[username]     admin
    	Should Be Equal As Strings     ${params}[password]     0000
    	Should Be Empty                ${params}[ca]      
    	Should Be Empty                ${params}[cert]      
    	Should Be Empty                ${params}[key]      
    	Should Be Empty                ${params}[keypass]      

    	[Teardown]    Neuron Stop Running    ${process}

Get the updated adapters settings information after stopping and restarting Neuron, it should return the correct information
    	${process} =    Neuron Start Running

   	Node Setting    ${modbus-tcp-2}    {"host": "127.0.0.1", "port": 502, "timeout": 3000, "connection_mode": 0, "transport_mode": 0, "interval": 20}

    	Neuron Stop Running    ${process}
    	${process} =           Neuron Start Running

    	${res} =       Get Node Setting    ${modbus-tcp-2}
    	${params} =    Set Variable        ${res}[params]

    	Check Response Status          ${res}                        200
    	Should Be Equal As Strings     ${res}[node]                  ${modbus-tcp-2}
    	Should Be Equal As Integers    ${params}[port]               502
    	Should Be Equal As Integers    ${params}[timeout]            3000
    	Should Be Equal As Strings     ${params}[host]               127.0.0.1

    	[Teardown]    Neuron Stop Running    ${process}

Get the newly add group after stopping and restarting Neuron, it should return success
    	${process} =    Neuron Start Running

    	Add Group     ${modbus-tcp-1}    ${test_gconfig1}    1500
    	Add Group     ${modbus-tcp-1}    ${test_gconfig2}    1500
    	Add Group     ${modbus-tcp-1}    ${test_gconfig3}    1500

    	Neuron Stop Running    ${process}
    	${process} =           Neuron Start Running

    	${res} =                 Get Group	${modbus-tcp-1}
    	Check Response Status    ${res}              200

	Should Be Equal As Strings	${res}[groups][0][interval]	1500
	Should Be Equal As Strings	${res}[groups][1][interval]	1500
	Should Be Equal As Strings	${res}[groups][2][interval]	1500

	[Teardown]    Neuron Stop Running    ${process}

Get the deleted group after stopping and restarting Neuron, it should return failure
    	${process} =    Neuron Start Running

    	Del Group	${modbus-tcp-1}    ${test_gconfig3}

    	Neuron Stop Running    ${process}
    	${process} =           Neuron Start Running

    	${res} =                 Get Group		${modbus-tcp-1}
    	Check Response Status    ${res}              200

	${len} =                  Get Length                      ${res}[groups]
 	Should Be Equal As Integers	${len}		2
	Should Be Equal As Strings	${res}[groups][0][interval]	1500
	Should Be Equal As Strings	${res}[groups][1][interval]	1500


	[Teardown]    Neuron Stop Running    ${process}

Get the newly add tags after stopping and restarting Neuron, it should return success
    	${process} =    Neuron Start Running

    	Add Tags    ${modbus-tcp-1}    ${test_gconfig1}    ${tag1},${tag2},${tag3}

    	Neuron Stop Running    ${process}
   	${process} =           Neuron Start Running

    	${res} =                 Get Tags    ${modbus-tcp-1}    ${test_gconfig1}
    	Check Response Status    ${res}      200

	${len} =	Get Length	${res}[tags]
 	Should Be Equal As Integers	${len}		3

	Should Be Equal As Strings	${res}[tags][0][name]		tag1
	Should Be Equal As Strings	${res}[tags][0][address]	1!400001
	Should Be Equal As Strings	${res}[tags][0][attribute]	${TAG_ATTRIBUTE_READ}
	Should Be Equal As Strings	${res}[tags][0][type]		${TAG_TYPE_INT16}

	Should Be Equal As Strings	${res}[tags][1][name]		tag2
	Should Be Equal As Strings	${res}[tags][1][address]	1!00001
	Should Be Equal As Strings	${res}[tags][1][attribute]	${TAG_ATTRIBUTE_RW}
	Should Be Equal As Strings	${res}[tags][1][type]		${TAG_TYPE_BIT}

	Should Be Equal As Strings	${res}[tags][2][name]		tag3
	Should Be Equal As Strings	${res}[tags][2][address]	2!400003
	Should Be Equal As Strings	${res}[tags][2][attribute]	${TAG_ATTRIBUTE_READ}
	Should Be Equal As Strings	${res}[tags][2][type]		${TAG_TYPE_INT32}

    	[Teardown]    Neuron Stop Running    ${process} 
Get the deleted tags after stopping and restarting Neuron, it should return success
    	${process} =    Neuron Start Running

    	Del Tags    ${modbus-tcp-1}    ${test_gconfig1}    "tag3"

    	Neuron Stop Running    ${process}
    	${process}             Neuron Start Running

    	${res} =                 Get Tags    ${modbus-tcp-1}    ${test_gconfig1}
    	Check Response Status    ${res}      200

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

    	[Teardown]    Neuron Stop Running    ${process}

Get the update tags after stopping ande restarting Neuron, it should return the correct information
    	${process} =    Neuron Start Running

    	${res} =                 Update Tags    ${modbus-tcp-1}    ${test_gconfig1}    {"name": "tag1", "type": ${TAG_TYPE_INT16}, "attribute":${TAG_ATTRIBUTE_READ}, "address": "1!400002"}
    	Check Response Status    ${res}         200

    	Neuron Stop Running    ${process}
    	${process} =           Neuron Start Running

    	${res} =                 Get Tags    ${modbus-tcp-1}    ${test_gconfig1}
    	Check Response Status    ${res}      200

	${len} =	Get Length	${res}[tags]
 	Should Be Equal As Integers	${len}		2

  ${tag1} =   Tag Find By Name  ${res}[tags]  tag1
	Should Be Equal As Strings	${tag1}[name]		tag1
	Should Be Equal As Strings	${tag1}[address]	1!400002
	Should Be Equal As Strings	${tag1}[attribute]	${TAG_ATTRIBUTE_READ}
	Should Be Equal As Strings	${tag1}[type]		${TAG_TYPE_INT16}

  ${tag2} =   Tag Find By Name  ${res}[tags]  tag2
	Should Be Equal As Strings	${tag2}[name]		tag2
	Should Be Equal As Strings	${tag2}[address]	1!00001
	Should Be Equal As Strings	${tag2}[attribute]	${TAG_ATTRIBUTE_RW}
	Should Be Equal As Strings	${tag2}[type]		${TAG_TYPE_BIT}

   	[Teardown]    Neuron Stop Running    ${process}

Get the subscribed groups after stopping and restarting Neuron, it should return success
    	${process} =    Neuron Start Running

    	${res} =                 Subscribe Group    ${mqtt-1}    ${modbus-tcp-1}    ${test_gconfig1}
    	Check Response Status    ${res}             200
    	${res} =                 Subscribe Group    ${mqtt-1}    ${modbus-tcp-1}    ${test_gconfig2}
    	Check Response Status    ${res}             200

    	Neuron Stop Running    ${process}
    	${process} =           Neuron Start Running

    	${res} =                 Get Subscribe Group    ${mqtt-1}

    	Check Response Status    ${res}                 200
	${len} =                  Get Length                      ${res}[groups]
 	Should Be Equal As Integers	${len}		2
	Should Be Equal As Strings	${res}[groups][0][driver]	${modbus-tcp-1}
	Should Be Equal As Strings	${res}[groups][0][group]	${test_gconfig1}

	Should Be Equal As Strings	${res}[groups][1][driver]	${modbus-tcp-1}
	Should Be Equal As Strings	${res}[groups][1][group]	${test_gconfig2}

    	[Teardown]    Neuron Stop Running    ${process} 

Update driver name, it should keep the setting, groups, tags and subscription
    ${process} =                  Neuron Start Running

    ${res} =                      Update Node                 ${modbus-tcp-1}       ${modbus-tcp-1-new}
    Check Response Status         ${res}                      200
    Check Error Code              ${res}                      ${NEU_ERR_SUCCESS}

    Neuron Stop Running           ${process}
    ${process} =                  Neuron Start Running

    ${res} =                      Get Node Setting            ${modbus-tcp-1-new}
    ${params} =                   Set Variable                ${res}[params]
    Check Response Status         ${res}                      200
    Should Be Equal As Strings    ${res}[node]                ${modbus-tcp-1-new}
    Should Be Equal As Integers   ${params}[port]             502
    Should Be Equal As Integers   ${params}[timeout]          3000
    Should Be Equal As Strings    ${params}[host]             127.0.0.1

    ${res} =                      Get Tags                    ${modbus-tcp-1-new}    ${test_gconfig1}
    Check Response Status         ${res}                      200
    ${len} =                      Get Length                  ${res}[tags]
    Should Be Equal As Integers   ${len}                      2
    ${tag1} =   Tag Find By Name  ${res}[tags]                tag1
    Should Be Equal As Strings    ${tag1}[name]               tag1
    Should Be Equal As Strings    ${tag1}[address]            1!400002
    Should Be Equal As Strings    ${tag1}[attribute]          ${TAG_ATTRIBUTE_READ}
    Should Be Equal As Strings    ${tag1}[type]               ${TAG_TYPE_INT16}
    ${tag2} =   Tag Find By Name  ${res}[tags]                tag2
    Should Be Equal As Strings    ${tag2}[name]               tag2
    Should Be Equal As Strings    ${tag2}[address]            1!00001
    Should Be Equal As Strings    ${tag2}[attribute]          ${TAG_ATTRIBUTE_RW}
    Should Be Equal As Strings    ${tag2}[type]               ${TAG_TYPE_BIT}

    ${res} =                      Get Subscribe Group         ${mqtt-1}
    ${len} =                      Get Length                  ${res}[groups]
    Should Be Equal As Integers   ${len}                      2
    Should Be Equal As Strings    ${res}[groups][0][driver]   ${modbus-tcp-1-new}
    Should Be Equal As Strings    ${res}[groups][0][group]    ${test_gconfig1}
    Should Be Equal As Strings    ${res}[groups][1][driver]   ${modbus-tcp-1-new}
    Should Be Equal As Strings    ${res}[groups][1][group]    ${test_gconfig2}

    ${res} =                      Update Node                 ${modbus-tcp-1-new}   ${modbus-tcp-1}
    Check Response Status         ${res}                      200
    Check Error Code              ${res}                      ${NEU_ERR_SUCCESS}

    [Teardown]                    Neuron Stop Running         ${process}

Update app name, it should keep the setting and subscription
    ${process} =                  Neuron Start Running

    ${res} =                      Update Node                 ${mqtt-1}             ${mqtt-1-new}
    Check Response Status         ${res}                      200
    Check Error Code              ${res}                      ${NEU_ERR_SUCCESS}

    Neuron Stop Running           ${process}
    ${process} =                  Neuron Start Running

    ${res} =                      Get Node Setting            ${mqtt-1-new}
    ${params} =                   Set Variable                ${res}[params]
    Check Response Status         ${res}                      200
    Should Be Equal As Strings    ${res}[node]                ${mqtt-1-new}
    Should Be Equal As Strings    ${params}[upload-topic]     /neuron/mqtt-1/upload
    Should Be Equal As Strings    ${params}[heartbeat-topic]  /neuron/mqtt-1/heartbeat
    Should Be Equal As Integers   ${params}[format]           0
    Should Not Be True            ${params}[ssl]
    Should Be Equal As Strings    ${params}[host]             127.0.0.1
    Should Be Equal As Integers   ${params}[port]             1883
    Should Be Equal As Strings    ${params}[username]         admin
    Should Be Equal As Strings    ${params}[password]         0000
    Should Be Empty               ${params}[ca]
    Should Be Empty               ${params}[cert]
    Should Be Empty               ${params}[key]
    Should Be Empty               ${params}[keypass]

    ${res} =                      Get Subscribe Group         ${mqtt-1-new}
    ${len} =                      Get Length                  ${res}[groups]
    Should Be Equal As Integers   ${len}                      2
    Should Be Equal As Strings    ${res}[groups][0][driver]   ${modbus-tcp-1}
    Should Be Equal As Strings    ${res}[groups][0][group]    ${test_gconfig1}
    Should Be Equal As Strings    ${res}[groups][1][driver]   ${modbus-tcp-1}
    Should Be Equal As Strings    ${res}[groups][1][group]    ${test_gconfig2}

    ${res} =                      Update Node                 ${mqtt-1-new}           ${mqtt-1}
    Check Response Status         ${res}                      200
    Check Error Code              ${res}                      ${NEU_ERR_SUCCESS}

    [Teardown]                    Neuron Stop Running         ${process}

Get the unsubscribed groups after stopping ande restarting Neuron, it should return failure
    	${process} =    Neuron Start Running

    	${res} =                 Unsubscribe Group    ${mqtt-1}    ${modbus-tcp-1}    ${test_gconfig2}
    	Check Response Status    ${res}               200

    	Neuron Stop Running    ${process}
    	${process} =           Neuron Start Running

    	${res} =                 Get Subscribe Group    ${mqtt-1}
    	Check Response Status    ${res}                 200

	${len} =                  Get Length                      ${res}[groups]
 	Should Be Equal As Integers	${len}		1
	Should Be Equal As Strings	${res}[groups][0][driver]	${modbus-tcp-1}
	Should Be Equal As Strings	${res}[groups][0][group]	${test_gconfig1}

    	[Teardown]    Neuron Stop Running    ${process}

*** Keywords ***
Neuron Context Ready
    Remove Persistence

Neuron Context Stop
    Remove Persistence

Neuron Start Running
    ${p} =    Start Neuron

    Sleep     3
    LOGIN 

    [Return]    ${p}

Neuron Stop Running
    [Arguments]    ${p}
    Sleep          1
    End Neuron     ${p}