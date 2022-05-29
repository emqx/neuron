*** Settings ***
Resource          api_http.resource
Resource          common.resource
Suite Setup       Neuron Context Ready
Suite Teardown    Neuron Context Stop

*** Variables ***

*** Test Cases ***
POST ping, status can be ok
    Ping

Use the wrong body to create a node, it should return failure
    ${res} =    Add Node    type=${${NODE_DRIVER}}    namexx=test-node    plugin_name=test-plugin-name

    Check Response Status    ${res}    400
    Check Error Code         ${res}    ${ERR_REQUEST_BODY_INVALID}


Use a plugin name that does not exist to create a node, it should return failure
    ${res} =    Add Node    type=${${NODE_DRIVER}}    name=test-node    plugin_name=test-plugin-name

    Check Response Status    ${res}    404
    Check Error Code         ${res}    ${ERR_LIBRARY_NOT_FOUND}


Use a invalid node type to create a node, it should return failure
    ${res} =    Add Node    type=${10}    name=test-node    plugin_name=modbus-tcp

    Check Response Status    ${res}    400
    Check Error Code         ${res}    ${ERR_NODE_TYPE_INVALID}

Create a node with the correct body, it should return success
    ${res} =    Add Node    type=${${NODE_DRIVER}}    name=test-node    plugin_name=modbus-tcp

    Check Response Status    ${res}    200
    Check Error Code         ${res}    ${ERR_SUCCESS}

Create an existing node, it should return failure
    ${res} =    Add Node    type=${${NODE_DRIVER}}    name=test-node    plugin_name=modbus-tcp

    Check Response Status    ${res}    409
    Check Error Code         ${res}    ${ERR_NODE_EXIST}

Get node with wrong parameters, it will return failure
    GET        /api/v2/node?tt=${NODE_DRIVER}
    Integer    response status        400
    Integer    response body error    ${ERR_REQUEST_PARAM_INVALID}

    GET        /api/v2/node?name_contains=test-node
    Integer    response status        400
    Integer    response body error    ${ERR_REQUEST_PARAM_INVALID}

Get node by nonexistent id, it should failure
    GET        /api/v2/node?id=123456

    Integer    response status        404
    Integer    response body error    ${ERR_NODE_NOT_EXIST}

Get DRIVER node, it should return all DRIVER node
    ${res} =    Get Nodes    ${NODE_DRIVER}

    Check Response Status    ${res}    200

    Node With Name Should Exist    ${res}[nodes]    test-node

Get DRIVER node by id, it should return the DRIVER node
    ${id} =    Get Node ID   ${NODE_DRIVER}         test-node

    GET        /api/v2/node?id=${id}
    Integer    response status                      200
    Integer    response body nodes 0 id             ${id}
    String     response body nodes 0 name           test-node

Get DRIVER node by name substring, it should return the DRIVER node
    ${id} =    Get Node ID   ${NODE_DRIVER}         test-node

    GET        /api/v2/node?type=${NODE_DRIVER}&name_contains=test-node
    Integer    response status                      200
    Integer    response body nodes 0 id             ${id}
    String     response body nodes 0 name           test-node

Get DRIVER node by id with correct type and name substring, it should return the DRIVER node
    ${id} =    Get Node ID   ${NODE_DRIVER}         test-node

    GET        /api/v2/node?type=${NODE_DRIVER}&id=${id}&name_contains=test-node
    Integer    response status                      200
    Integer    response body nodes 0 id             ${id}
    String     response body nodes 0 name           test-node

Get DRIVER node by id with wrong type, it should return failure
    ${id} =    Get Node ID   ${NODE_DRIVER}         test-node

    GET        /api/v2/node?type=${NODE_WEB}&id=${id}
    Integer    response status                      404
    Integer    response body error                  ${ERR_NODE_NOT_EXIST}

Get DRIVER node by id with wrong name substring, it should return failure
    ${id} =    Get Node ID   ${NODE_DRIVER}         test-node

    GET        /api/v2/node?id=${id}&name_contains=xxx
    Integer    response status                      404
    Integer    response body error                  ${ERR_NODE_NOT_EXIST}

Get WEB node, it should return all WEB node
    ${res} =    Get Nodes    ${NODE_WEB}

    Check Response Status          ${res}           200
    Node With Name Should Exist    ${res}[nodes]    default-dashboard-adapter

Get WEB node by id, it should return the WEB node
    ${id} =    Get Node ID   ${NODE_WEB}            default-dashboard-adapter

    GET        /api/v2/node?id=${id}
    Integer    response status                      200
    Integer    response body nodes 0 id             ${id}
    String     response body nodes 0 name           default-dashboard-adapter

Get WEB node by name substring, it should return the WEB node
    ${id} =    Get Node ID   ${NODE_WEB}            default-dashboard-adapter

    GET        /api/v2/node?type=${NODE_WEB}&name_contains=dashboard
    Integer    response status                      200
    Integer    response body nodes 0 id             ${id}
    String     response body nodes 0 name           default-dashboard-adapter

Get WEB node by id with correct type and name substring, it should return the WEB node
    ${id} =    Get Node ID   ${NODE_WEB}            default-dashboard-adapter

    GET        /api/v2/node?type=${NODE_WEB}&id=${id}&name_contains=dashboard
    Integer    response status                      200
    Integer    response body nodes 0 id             ${id}
    String     response body nodes 0 name           default-dashboard-adapter

Get WEB node by id with wrong type, it should return failure
    ${id} =    Get Node ID   ${NODE_WEB}            default-dashboard-adapter

    GET        /api/v2/node?type=${NODE_DRIVER}&id=${id}
    Integer    response status                      404
    Integer    response body error                  ${ERR_NODE_NOT_EXIST}

Get WEB node by id with wrong name substring, it should return failure
    ${id} =    Get Node ID   ${NODE_WEB}            default-dashboard-adapter

    GET        /api/v2/node?id=${id}&name_contains=xxx
    Integer    response status                      404
    Integer    response body error                  ${ERR_NODE_NOT_EXIST}

Get MQTT node, it should return all mqtt node
    ${res} =    Add Node     type=${${NODE_MQTT}}    name=mqtt-adapter    plugin_name=mqtt
    ${res} =    Get Nodes    ${NODE_MQTT}

    Check Response Status          ${res}           200
    Node With Name Should Exist    ${res}[nodes]    mqtt-adapter

Get MQTT node by id, it should return the MQTT node
    ${id} =    Get Node ID   ${NODE_MQTT}            mqtt-adapter

    GET        /api/v2/node?id=${id}
    Integer    response status                      200
    Integer    response body nodes 0 id             ${id}
    String     response body nodes 0 name           mqtt-adapter

Get MQTT node by name substring, it should return the MQTT node
    ${id} =    Get Node ID   ${NODE_MQTT}            mqtt-adapter

    GET        /api/v2/node?type=${NODE_MQTT}&name_contains=mqtt
    Integer    response status                      200
    Integer    response body nodes 0 id             ${id}
    String     response body nodes 0 name           mqtt-adapter

Get MQTT node by id with correct type and name substring, it should return the MQTT node
    ${id} =    Get Node ID   ${NODE_MQTT}            mqtt-adapter

    GET        /api/v2/node?type=${NODE_MQTT}&id=${id}&name_contains=mqtt
    Integer    response status                      200
    Integer    response body nodes 0 id             ${id}
    String     response body nodes 0 name           mqtt-adapter

Get MQTT node by id with wrong type, it should return failure
    ${id} =    Get Node ID   ${NODE_MQTT}            mqtt-adapter

    GET        /api/v2/node?type=${NODE_WEB}&id=${id}
    Integer    response status                      404
    Integer    response body error                  ${ERR_NODE_NOT_EXIST}

Get MQTT node by id with wrong name substring, it should return failure
    ${id} =    Get Node ID   ${NODE_MQTT}            mqtt-adapter

    GET        /api/v2/node?id=${id}&name_contains=xxx
    Integer    response status                      404
    Integer    response body error                  ${ERR_NODE_NOT_EXIST}


Get UNKNOWN type node, it should return empty node
    ${res} =    Get Nodes    ${NODE_UNKNOWN}

    Check Response Status    ${res}                       200
    Run Keyword If           ${res}[nodes] != @{EMPTY}    Fail    not an empty object

Get INVALID type node, it should return failure
    ${res} =    Get Nodes    ${123456}

    Check Response Status    ${res}    400
    Check Error Code         ${res}    ${ERR_REQUEST_PARAM_INVALID}

Delete the existing node, it will return success
    ${res} =    Get Nodes    ${NODE_DRIVER}

    Check Response Status              ${res}              200
    ${node_id} =                       Get Node By Name    ${res}[nodes]    test-node
    Should Not Be Equal As Integers    ${node_id}          0

    ${res} =                 Del Node    ${node_id}
    Check Response Status    ${res}      200

    ${res} =                       Get Nodes           ${NODE_DRIVER}
    Check Response Status          ${res}              200
    ${node_id} =                   Get Node By Name    ${res}[nodes]     test-node
    Should Be Equal As Integers    ${node_id}          0

Delete a non-existent node, it will return failure
    ${res} =    Del Node    ${1000}

    Check Response Status    ${res}    404
    Check Error Code         ${res}    ${ERR_NODE_NOT_EXIST}

Add setting to non-existent node, it should return failure
    ${res} =    Node Setting    ${999}    {"host": "1.1.1.1", "port": 6677, "timeout": 3000}

    Check Response Status    ${res}    404
    Check Error Code         ${res}    ${ERR_NODE_NOT_EXIST}

Get setting from non-existent node, it should return failure
    ${res} =    Get Node Setting    ${999}

    Check Response Status    ${res}    404
    Check Error Code         ${res}    ${ERR_NODE_NOT_EXIST}

Get setting from a node that has never been set, it should return failure
    ${driver_node_id} =    Add Node And Return ID    ${NODE_DRIVER}       driver-node    ${PLUGIN_MODBUS_TCP}
    ${res} =               Get Node Setting          ${driver_node_id}

    Check Response Status    ${res}    200
    Check Error Code         ${res}    ${ERR_NODE_SETTING_NOT_EXIST}

    [Teardown]    Del Node Check    ${driver_node_id}

Add the correct settings to the node, it should return success
    ${driver_node_id} =    Add Node And Return ID    ${NODE_DRIVER}       driver-node                                                ${PLUGIN_MODBUS_TCP}
    ${res} =               Node Setting              ${driver_node_id}    {"host": "1.1.1.1", "port": 6677, "timeout": 3000}

    Check Response Status    ${res}    200
    Check Error Code         ${res}    ${ERR_SUCCESS}

Get the setting of node, it should return to the previous setting
    ${driver_node_id} =    Get Node ID         ${NODE_DRIVER}       driver-node
    ${res} =               Get Node Setting    ${driver_node_id}
    ${params} =            Set Variable        ${res}[params]

    Check Response Status          ${res}                        200
    Should Be Equal As Integers    ${res}[node_id]               ${driver_node_id}
    Should Be Equal As Integers    ${params}[port]               6677
    Should Be Equal As Strings     ${params}[host]               1.1.1.1
    Should Be Equal As Integers    ${params}[timeout]            3000

Add wrong settings to node, it should return failure
    ${driver_node_id} =    Get Node ID     ${NODE_DRIVER}       driver-node
    ${res} =               Node Setting    ${driver_node_id}    {"xxhost": "1.1.1.1", "port": 6677, "timeout": 3000}

    Check Response Status    ${res}    400
    Check Error Code         ${res}    ${ERR_NODE_SETTING_INVALID}

Start an unconfigured node, it should return failure
    ${modbus_node_id} =    Add Node And Return ID    ${NODE_DRIVER}    modbus-test-node    ${PLUGIN_MODBUS_TCP}

    Sleep    1s

    ${running_state}    ${link_state} =    Get Node State    ${modbus_node_id}

    Should Be Equal As Integers    ${running_state}    ${NODE_STATE_INIT}
    Should Be Equal As Integers    ${link_state}       ${NODE_LINK_STATE_DISCONNECTED}

    ${res} =    Node Ctl    ${modbus_node_id}    ${NODE_CTL_START}

    Check Response Status    ${res}    409
    Check Error Code         ${res}    ${ERR_NODE_NOT_READY}

    [Teardown]    Del Node Check    ${modbus_node_id}

Start the configured node, it should return success
    ${modbus_node_id} =    Add Node And Return ID    ${NODE_DRIVER}    modbus-test-node    ${PLUGIN_MODBUS_TCP}

    Sleep    1s

    Node Setting        ${modbus_node_id}    {"host": "127.0.0.1", "port": 502, "timeout": 3000}
    ${running_state}    ${link_state} =      Get Node State                                              ${modbus_node_id}

    Should Be Equal As Integers    ${running_state}    ${NODE_STATE_READY}
    Should Be Equal As Integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

    ${res} =                 Node Ctl    ${modbus_node_id}    ${NODE_CTL_START}
    Check Response Status    ${res}      200
    Check Error Code         ${res}      ${ERR_SUCCESS}

    ${running_state}    ${link_state} =    Get Node State    ${modbus_node_id}

    Should Be Equal As Integers    ${running_state}    ${NODE_STATE_RUNNING}
    Should Be Equal As Integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

Start the stopped node, it should return success
    ${modbus_node_id} =    Get Node ID    ${NODE_DRIVER}    modbus-test-node

    Node Ctl    ${modbus_node_id}    ${NODE_CTL_STOP}

    ${running_state}    ${link_state} =    Get Node State    ${modbus_node_id}

    Should Be Equal As Integers    ${running_state}    ${NODE_STATE_STOP}
    Should Be Equal As Integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

    ${res} =    Node Ctl    ${modbus_node_id}    ${NODE_CTL_START}

    Check Response Status    ${res}    200
    Check Error Code         ${res}    ${ERR_SUCCESS}

    ${running_state}    ${link_state} =    Get Node State    ${modbus_node_id}

    Should Be Equal As Integers    ${running_state}    ${NODE_STATE_RUNNING}
    Should Be Equal As Integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

Start the running node, it should return failure
    ${modbus_node_id} =    Get Node ID    ${NODE_DRIVER}       modbus-test-node
    ${res} =               Node Ctl       ${modbus_node_id}    ${NODE_CTL_START}

    Check Response Status    ${res}    409
    Check Error Code         ${res}    ${ERR_NODE_IS_RUNNING}

    ${res} =                 Node Ctl    ${modbus_node_id}    ${NODE_CTL_STOP}
    Check Response Status    ${res}      200
    Check Error Code         ${res}      ${ERR_SUCCESS}

Stop node that is stopped, it should return failure
    ${modbus_node_id} =    Get Node ID    ${NODE_DRIVER}    modbus-test-node

    ${res} =                 Node Ctl    ${modbus_node_id}        ${NODE_CTL_STOP}
    Check Response Status    ${res}      409
    Check Error Code         ${res}      ${ERR_NODE_IS_STOPED}

    [Teardown]    Del Node Check    ${modbus_node_id}

Stop node that is ready(init/idle), it should return failure
    ${modbus_node_id} =    Add Node And Return ID    ${NODE_DRIVER}    modbus-test-node    ${PLUGIN_MODBUS_TCP}

    Sleep    1s

    ${res} =                 Node Ctl    ${modbus_node_id}          ${NODE_CTL_STOP}
    Check Response Status    ${res}      409
    Check Error Code         ${res}      ${ERR_NODE_NOT_RUNNING}

    Node Setting    ${modbus_node_id}    {"host": "127.0.0.1", "port": 502, "connection_mode": 0}

    ${res} =    Node Ctl    ${modbus_node_id}    ${NODE_CTL_STOP}

    Check Response Status    ${res}    409
    Check Error Code         ${res}    ${ERR_NODE_NOT_RUNNING}

    [Teardown]    Del Node Check    ${modbus_node_id}

When setting up a READY/RUNNING/STOPED node, the node status will not change
    ${modbus_node_id} =    Add Node And Return ID    ${NODE_DRIVER}    modbus-test-node    ${PLUGIN_MODBUS_TCP}

    Sleep    1s

    Node Setting        ${modbus_node_id}    {"host": "127.0.0.1", "port": 502, "timeout": 3000}
    ${running_state}    ${link_state} =      Get Node State                                              ${modbus_node_id}

    Should Be Equal As Integers    ${running_state}    ${NODE_STATE_READY}
    Should Be Equal As Integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

    Node Setting        ${modbus_node_id}    {"host": "127.0.0.1", "port": 502, "timeout": 3000}
    ${running_state}    ${link_state} =      Get Node State                                              ${modbus_node_id}

    Should Be Equal As Integers    ${running_state}    ${NODE_STATE_READY}
    Should Be Equal As Integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

    Node Ctl            ${modbus_node_id}    ${NODE_CTL_START}
    ${running_state}    ${link_state} =      Get Node State       ${modbus_node_id}

    Should Be Equal As Integers    ${running_state}    ${NODE_STATE_RUNNING}
    Should Be Equal As Integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

    Node Ctl            ${modbus_node_id}    ${NODE_CTL_STOP}
    ${running_state}    ${link_state} =      Get Node State      ${modbus_node_id}

    Should Be Equal As Integers    ${running_state}    ${NODE_STATE_STOP}
    Should Be Equal As Integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

*** Keywords ***
Neuron Context Ready
    Neuron Ready

    LOGIN

Neuron Context Stop
    Stop Neuron    remove_persistence_data=True

Del Node Check
    [Arguments]              ${node_id}
    ${res} =                 Del Node      ${node_id}
    Check Response Status    ${res}        200
    Check Error Code         ${res}        ${ERR_SUCCESS}
