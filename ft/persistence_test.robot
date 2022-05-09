*** Settings ***
Resource          common.resource
Suite Setup       Neuron Context Ready 
Suite Teardown    Neuron Context Stop

*** Variables ***
${test_gconfig1}    test_gconfig1
${test_gconfig2}    test_gconfig2
${test_gconfig3}    test_gconfig3
${modbus-tcp-1}     modbus-tcp-1
${modbus-tcp-2}     modbus-tcp-2
${modbus-tcp-3}     modbus-tcp-3
${modbus-tcp-4}     modbus-tcp-4
${mqtt-1}           mqtt-1
${tag1}             {"name": "tag1", "address": "1!400001", "attribute": 1, "type": 4}
${tag2}             {"name": "tag2", "address": "1!00001", "attribute": 3, "type": 14}
${tag3}             {"name": "tag3", "address": "2!400003", "attribute": 1, "type": 5}
${tag4}             {"name": "tag4", "address": "2!400005", "attribute": 1, "type": 4}
${tag5}             {"name": "tag5", "address": "2!400006", "attribute": 1, "type": 4}
${tag6}             {"name": "tag6", "address": "2!400007", "attribute": 1, "type": 4}
${tag7}             {"name": "tag7", "address": "2!400008", "attribute": 1, "type": 4}
${tag8}             {"name": "tag8", "address": "2!400009", "attribute": 1, "type": 4}

*** Test Cases ***
Get the newly added adapters after stopping and restarting Neuron, it should return success
    Skip If Not Http API

    ${process} =    Neuron Start Running

    Add Node    type=${${NODE_DRIVER}}    name=${${modbus-tcp-1}}    plugin_name=modbus-tcp
    Add Node    type=${${NODE_DRIVER}}    name=${${modbus-tcp-2}}    plugin_name=modbus-tcp
    Add Node    type=${${NODE_DRIVER}}    name=${${modbus-tcp-3}}    plugin_name=modbus-tcp
    Add Node    type=${${NODE_DRIVER}}    name=${${modbus-tcp-4}}    plugin_name=modbus-tcp

    ${res} =                 Add Node    type=${${NODE_MQTT}}    name=${${mqtt-1}}    plugin_name=mqtt
    Check Response Status    ${res}      200
    Check Error Code         ${res}      ${ERR_SUCCESS}

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running 

    ${res} =                       Get Nodes        ${NODE_DRIVER}
    Check Response Status          ${res}           200
    Node With Name Should Exist    ${res}[nodes]    ${modbus-tcp-1}
    Node With Name Should Exist    ${res}[nodes]    ${modbus-tcp-2}
    Node With Name Should Exist    ${res}[nodes]    ${modbus-tcp-3}
    Node With Name Should Exist    ${res}[nodes]    ${modbus-tcp-4}

    ${res} =                       Get Nodes        ${NODE_MQTT}
    Check Response Status          ${res}           200
    Node With Name Should Exist    ${res}[nodes]    ${mqtt-1}

    [Teardown]    Neuron Stop Running    ${process}

Get the deleted adapter after stopping and restarting Neuron, it should return failure
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp4_id} =    Get Node ID          ${NODE_DRIVER}    ${modbus-tcp-4}
    Del Node               ${modbus_tcp4_id}

    ${res} =                       Get Nodes            ${NODE_DRIVER}
    ${modbus_tcp4_id} =            Get Node By Name     ${res}[nodes]     ${modbus-tcp-4}
    Should Be Equal As Integers    ${modbus_tcp4_id}    0

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running    

    ${res} =                       Get Nodes            ${NODE_DRIVER}
    ${modbus_tcp4_id} =            Get Node By Name     ${res}[nodes]     ${modbus-tcp-4}
    Should Be Equal As Integers    ${modbus_tcp4_id}    0

    Node With Name Should Exist        ${res}[nodes]    ${modbus-tcp-1}
    Node With Name Should Exist        ${res}[nodes]    ${modbus-tcp-2}
    Node With Name Should Exist        ${res}[nodes]    ${modbus-tcp-3}
    Node With Name Should Not Exist    ${res}[nodes]    ${modbus-tcp-4}

    ${res} =                       Get Nodes        ${NODE_MQTT}
    Node With Name Should Exist    ${res}[nodes]    ${mqtt-1}

    [Teardown]    Neuron Stop Running    ${process}

Get the setted adapters information after stopping and restarting Neuron, it should return the correct information
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp1_id} =    Get Node ID          ${NODE_DRIVER}                                              ${modbus-tcp-1}
    Node Setting           ${modbus_tcp1_id}    {"host": "127.0.0.1", "port": 502, "connection_mode": 0}

    ${modbus_tcp2_id} =    Get Node ID          ${NODE_DRIVER}                                              ${modbus-tcp-2}
    Node Setting           ${modbus_tcp2_id}    {"host": "127.0.0.1", "port": 502, "connection_mode": 0}

    ${mqtt_id} =    Get Node ID    ${NODE_MQTT}                                                                                                                                           mqtt-1
    Node Setting    ${mqtt_id}     {"client-id":"upload123", "upload-topic":"", "format": 0, "ssl":false,"host":"127.0.0.1","port":1883,"username":"admin","password":"0000","ca":"", "cert":"", "key":""}

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running

    ${res} =                       Get Node Setting              ${modbus_tcp1_id}
    ${params} =                    Set Variable                  ${res}[params]
    Check Response Status          ${res}                        200
    Should Be Equal As Integers    ${res}[node_id]               ${modbus_tcp1_id}
    Should Be Equal As Integers    ${params}[port]               502
    Should Be Equal As Integers    ${params}[connection_mode]    0
    Should Be Equal As Strings     ${params}[host]               127.0.0.1

    ${res} =                       Get Node Setting              ${modbus_tcp2_id}
    ${params} =                    Set Variable                  ${res}[params]
    Check Response Status          ${res}                        200
    Should Be Equal As Integers    ${res}[node_id]               ${modbus_tcp2_id}
    Should Be Equal As Integers    ${params}[port]               502
    Should Be Equal As Integers    ${params}[connection_mode]    0
    Should Be Equal As Strings     ${params}[host]               127.0.0.1

    ${res} =                       Get Node Setting        ${mqtt_id}
    ${params} =                    Set Variable            ${res}[params]
    Check Response Status          ${res}                  200
    Should Be Equal As Integers    ${res}[node_id]         ${mqtt_id}
    Should Be Equal As Integers    ${params}[port]         1883
    Should Be Equal As Strings     ${params}[client-id]    upload123
    Should Not Be True             ${params}[ssl]          
    Should Be Equal As Strings     ${params}[host]         127.0.0.1
    Should Be Equal As Strings     ${params}[username]     admin
    Should Be Equal As Strings     ${params}[password]     0000
    Should Be Empty                ${params}[ca]      

    [Teardown]    Neuron Stop Running    ${process}

Get the updated adapters settings information after stopping and restarting Neuron, it should return the correct information
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp2_id} =    Get Node ID    ${NODE_DRIVER}    ${modbus-tcp-2}

    Node Setting    ${modbus_tcp2_id}    {"host": "127.0.0.1", "port": 502, "connection_mode": 1}

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running

    ${res} =       Get Node Setting    ${modbus_tcp2_id}
    ${params} =    Set Variable        ${res}[params]

    Check Response Status          ${res}                        200
    Should Be Equal As Integers    ${res}[node_id]               ${modbus_tcp2_id}
    Should Be Equal As Integers    ${params}[port]               502
    Should Be Equal As Integers    ${params}[connection_mode]    1
    Should Be Equal As Strings     ${params}[host]               127.0.0.1

    [Teardown]    Neuron Stop Running    ${process}

Get the newly add group_configs after stopping and restarting Neuron, it should return success
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp1_id} =    Get Node ID    ${NODE_DRIVER}    ${modbus-tcp-1}

    Add Group Config    ${modbus_tcp1_id}    ${test_gconfig1}    1500
    Add Group Config    ${modbus_tcp1_id}    ${test_gconfig2}    1500
    Add Group Config    ${modbus_tcp1_id}    ${test_gconfig3}    1500

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running

    ${res} =                 Get Group Config    ${modbus_tcp1_id}
    Check Response Status    ${res}              200

    ${ret} =                       Group Config Check    ${res}[group_configs]    ${test_gconfig1}    1500
    Should Be Equal As Integers    ${ret}                0
    ${ret} =                       Group Config Check    ${res}[group_configs]    ${test_gconfig2}    1500
    Should Be Equal As Integers    ${ret}                0
    ${ret} =                       Group Config Check    ${res}[group_configs]    ${test_gconfig3}    1500
    Should Be Equal As Integers    ${ret}                0

    [Teardown]    Neuron Stop Running    ${process}

Get the deleted group_configs after stopping and restarting Neuron, it should return failure
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp1_id} =    Get Node ID    ${NODE_DRIVER}    ${modbus-tcp-1}

    Del Group Config    ${modbus_tcp1_id}    ${test_gconfig3}

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running

    ${res} =                 Get Group Config    ${modbus_tcp1_id}
    Check Response Status    ${res}              200

    ${ret} =                       Group Config Check    ${res}[group_configs]    ${test_gconfig1}    1500
    Should Be Equal As Integers    ${ret}                0
    ${ret} =                       Group Config Check    ${res}[group_configs]    ${test_gconfig2}    1500
    Should Be Equal As Integers    ${ret}                0

    ${ret} =                       Get Group Config By Name    ${res}[group_configs]    ${test_gconfig3}
    Should Be Equal As Integers    ${ret}                      0

    [Teardown]    Neuron Stop Running    ${process}

Get the updated group_configs after stopping and restarting Neuron, it should return the correct configurations
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp1_id} =    Get Node ID          ${NODE_DRIVER}      ${modbus-tcp-1}
    Update Group Config    ${modbus_tcp1_id}    ${test_gconfig1}    1000

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running

    ${res} =    Get Group Config    ${modbus_tcp1_id}

    ${ret} =                       Group Config Check    ${res}[group_configs]    ${test_gconfig1}    1000
    Should Be Equal As Integers    ${ret}                0
    ${ret} =                       Group Config Check    ${res}[group_configs]    ${test_gconfig2}    1500
    Should Be Equal As Integers    ${ret}                0

    [Teardown]    Neuron Stop Running    ${process}

Get the newly added tag_id duplicated with the tag_id in the loaded persistence file, it should return a failure
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${node_id} =                       Get Node ID    ${NODE_DRIVER}    modbus-tcp-1
    Should Not Be Equal As Integers    ${node_id}     0

    ${res} =                 Add Tags    ${node_id}    ${test_gconfig1}    ${tag4},${tag5},${tag6}
    Check Response Status    ${res}      200

    ${res} =                       Get Tags      ${node_id}    ${test_gconfig1}
    Check Response Status          ${res}        200
    ${tag4_id} =                   Get Tag ID    ${node_id}    ${test_gconfig1}    tag4
    Should Be Equal As Integers    ${tag4_id}    1
    ${tag5_id} =                   Get Tag ID    ${node_id}    ${test_gconfig1}    tag5
    Should Be Equal As Integers    ${tag5_id}    2
    ${tag6_id} =                   Get Tag ID    ${node_id}    ${test_gconfig1}    tag6
    Should Be Equal As Integers    ${tag6_id}    3

    ${res} =    Del Tags    ${node_id}    ${test_gconfig1}    ${tag4_id}

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running

    ${res} =                 Add Tags    ${node_id}    ${test_gconfig1}    ${tag7},${tag8}
    Check Response Status    ${res}      200

    ${res} =                       Get Tags      ${node_id}    ${test_gconfig1}
    Check Response Status          ${res}        200
    ${tag5_id} =                   Get Tag ID    ${node_id}    ${test_gconfig1}    tag5
    Should Be Equal As Integers    ${tag5_id}    2
    ${tag6_id} =                   Get Tag ID    ${node_id}    ${test_gconfig1}    tag6
    Should Be Equal As Integers    ${tag6_id}    3
    ${tag7_id} =                   Get Tag ID    ${node_id}    ${test_gconfig1}    tag7
    Should Be Equal As Integers    ${tag7_id}    1
    ${tag8_id} =                   Get Tag ID    ${node_id}    ${test_gconfig1}    tag8
    Should Be Equal As Integers    ${tag8_id}    4


    [Teardown]    Neuron Stop Running    ${process}

Get the newly add tags after stopping and restarting Neuron, it should return success
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp1_id} =    Get Node ID    ${NODE_DRIVER}    ${modbus-tcp-1}

    Add Tags    ${modbus_tcp1_id}    ${test_gconfig1}    ${tag1},${tag2},${tag3}

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running

    ${res} =                 Get Tags    ${modbus_tcp1_id}    ${test_gconfig1}
    Check Response Status    ${res}      200

    ${ret} =                       Tag Check    ${res}[tags]    tag1    4     ${test_gconfig1}    1    1!400001
    Should Be Equal As Integers    ${ret}       0
    ${ret} =                       Tag Check    ${res}[tags]    tag2    14    ${test_gconfig1}    3    1!00001
    Should Be Equal As Integers    ${ret}       0
    ${ret} =                       Tag Check    ${res}[tags]    tag3    5     ${test_gconfig1}    1    2!400003
    Should Be Equal As Integers    ${ret}       0

    [Teardown]    Neuron Stop Running    ${process} 

Get the deleted tags after stopping and restarting Neuron, it should return success
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp1_id} =    Get Node ID    ${NODE_DRIVER}    ${modbus-tcp-1}

    ${tag3_id} =    Get Tag ID    ${modbus_tcp1_id}    ${test_gconfig1}    tag3

    ${res} =                 Del Tags    ${modbus_tcp1_id}    ${test_gconfig1}    ${tag3_id}
    Check Response Status    ${res}      200

    Neuron Stop Running    ${process}
    ${process}             Neuron Start Running

    ${res} =    Get Tags    ${modbus_tcp1_id}    ${test_gconfig1}

    ${ret} =                       Tag Check    ${res}[tags]    tag1    4     ${test_gconfig1}    1    1!400001
    Should Be Equal As Integers    ${ret}       0
    ${ret} =                       Tag Check    ${res}[tags]    tag2    14    ${test_gconfig1}    3    1!00001
    Should Be Equal As Integers    ${ret}       0

    ${id} =                        Tag Find By Name    ${res}[tags]    tag3
    Should Be Equal As Integers    ${id}               -1

    [Teardown]    Neuron Stop Running    ${process}

Get the update tags after stopping ande restarting Neuron, it should return the correct information
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp1_id} =    Get Node ID    ${NODE_DRIVER}    ${modbus-tcp-1}

    ${tag1_id} =                       Get Tag ID    ${modbus_tcp1_id}    ${test_gconfig1}    tag1
    Should Not Be Equal As Integers    ${tag1_id}    0

    ${res} =                 Update Tags    ${modbus_tcp1_id}    ${test_gconfig1}    {"id": ${tag1_id}, "name": "tag1", "type": 4, "attribute": 1, "address": "1!400002"}
    Check Response Status    ${res}         200

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running

    ${res} =                 Get Tags    ${modbus_tcp1_id}    ${test_gconfig1}
    Check Response Status    ${res}      200

    ${ret} =                       Tag Check    ${res}[tags]    tag1    4     ${test_gconfig1}    1    1!400002
    Should Be Equal As Integers    ${ret}       0
    ${ret} =                       Tag Check    ${res}[tags]    tag2    14    ${test_gconfig1}    3    1!00001
    Should Be Equal As Integers    ${ret}       0

    [Teardown]    Neuron Stop Running    ${process} 

Get the subscribed groups after stopping and restarting Neuron, it should return success
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp1_id} =    Get Node ID    ${NODE_DRIVER}    ${modbus-tcp-1}
    ${mqtt_id} =           Get Node ID    ${NODE_MQTT}      ${mqtt-1}

    ${res} =                 Subscribe Group    ${modbus_tcp1_id}    ${mqtt_id}    ${test_gconfig1}
    Check Response Status    ${res}             200
    ${res} =                 Subscribe Group    ${modbus_tcp1_id}    ${mqtt_id}    ${test_gconfig2}
    Check Response Status    ${res}             200

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running

    ${res} =                 Get Subscribe Group    ${mqtt_id}
    Check Response Status    ${res}                 200

    ${ret} =                       Subscribe Check    ${res}[groups]    ${modbus_tcp1_id}    ${test_gconfig1}
    Should Be Equal As Integers    ${ret}             0

    ${ret} =                       Subscribe Check    ${res}[groups]    ${modbus_tcp1_id}    ${test_gconfig2}
    Should Be Equal As Integers    ${ret}             0

    [Teardown]    Neuron Stop Running    ${process} 

Get the unsubscribed groups after stopping ande restarting Neuron, it should return failure
    Skip If Not Http API

    ${process} =    Neuron Start Running

    ${modbus_tcp1_id} =    Get Node ID    ${NODE_DRIVER}    ${modbus-tcp-1}
    ${mqtt_id} =           Get Node ID    ${NODE_MQTT}      ${mqtt-1}

    ${res} =                 Unsubscribe Group    ${modbus_tcp1_id}    ${mqtt_id}    ${test_gconfig2}
    Check Response Status    ${res}               200

    Neuron Stop Running    ${process}
    ${process} =           Neuron Start Running

    ${res} =                 Get Subscribe Group    ${mqtt_id}
    Check Response Status    ${res}                 200

    ${ret} =                       Subscribe Check    ${res}[groups]    ${modbus_tcp1_id}    ${test_gconfig1}
    Should Be Equal As Integers    ${ret}             0

    ${ret} =                           Subscribe Check    ${res}[groups]    ${modbus_tcp1_id}    ${test_gconfig2}
    Should Not Be Equal As Integers    ${ret}             0

    [Teardown]    Neuron Stop Running    ${process}

*** Keywords ***
Neuron Context Ready
    Import Neuron API Resource

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