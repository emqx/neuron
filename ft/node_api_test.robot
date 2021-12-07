*** Settings ***
Library           Keyword.Neuron.Neuron
Library           Keyword.Neuron.Node
Resource          error.robot
Resource          neuron.robot
Library           REST                     http://127.0.0.1:7001
Suite Setup       Neuron Context Ready
Suite Teardown    Neuron Context Stop

*** Variables ***

*** Test Cases ***
POST ping, status can be ok
    POST       /api/v2/ping
    Integer    response status    200
    Object     response body      {}

Use the wrong body to crate a node, it should return failure
    POST    /api/v2/node    {"type": ${NODE_DRIVER}, "namexx": "test-node", "plugin_name": "test-plugin-name"}

    Integer    response status        400
    Integer    response body error    ${ERR_REQUEST_BODY_INVALID}

Use a plugin name that does not exist to create a node, it should return failure
    POST    /api/v2/node    {"type": ${NODE_DRIVER}, "name": "test-node", "plugin_name": "test-plugin-name"}

    Integer    response status        404
    Integer    response body error    ${ERR_PLUGIN_NAME_NOT_FOUND}

Use a invalid node type to create a node, it should return failure
    POST    /api/v2/node    {"type": 10, "name": "test-node", "plugin_name": "modbus-tcp-plugin"}

    Integer    response status        400
    Integer    response body error    ${ERR_NODE_TYPE_INVALID}

Create a node with the correct body, it should return success
    POST    /api/v2/node    {"type": ${NODE_DRIVER}, "name": "test-node", "plugin_name": "modbus-tcp-plugin"}

    Integer    response status        200
    Integer    response body error    ${ERR_SUCCESS} 

Create an existing node, it should return failure
    POST    /api/v2/node    {"type": ${NODE_DRIVER}, "name": "test-node", "plugin_name": "modbus-tcp-plugin"}

    Integer    response status        409
    Integer    response body error    ${ERR_NODE_EXIST}

Get node with wrong parameters, it will return failure
    GET    /api/v2/node?tt=${NODE_DRIVER}

    Integer    response status        400
    Integer    response body error    ${ERR_REQUEST_PARAM_INVALID}

Get DRIVER node, it should return all DRIVER node
    GET    /api/v2/node?type=${NODE_DRIVER}

    Integer    response status    200
    Array      $.nodes
    String     $.nodes[*].name    sample-driver-adapter    modbus-tcp-adapter    opcua-adapter    test-node

Get WEB node, it should return all WEB node
    GET    /api/v2/node?type=${NODE_WEB}

    Integer    response status    200
    Array      $.nodes
    String     $.nodes[*].name    default-dashboard-adapter

Get MQTT node, it should return all mqtt node
    GET    /api/v2/node?type=${NODE_MQTT}

    Integer    response status    200
    Array      $.nodes
    String     $.nodes[*].name    mqtt-adapter

Get STREAM PROCESSOR node, it should return all STREAM PROCESSOR node
    GET    /api/v2/node?type=${NODE_STREAM_PROCESSOR}

    Integer    response status    200
    Object     response body      {}
    #String     $.nodes[*].name    stream-process-adapter

Get APP node, it should return all APP node
    GET    /api/v2/node?type=${NODE_APP}

    Integer    response status    200
    Array      $.nodes
    String     $.nodes[*].name    sample-app-adapter

Get UNKNOWN type node, it should return empty node
    GET    /api/v2/node?type=${NODE_UNKNOWN}

    Integer    response status    200
    Object     response body      {}

Get INVALID type node, it should return failure
    GET    /api/v2/node?type=123456

    Integer    response status    400
    Integer    response body error    ${ERR_REQUEST_PARAM_INVALID}

Delete the existing node, it will return success
    GET    /api/v2/node?type=${NODE_DRIVER}

    Integer                response status     200
    ${tmp}                 Array               $.nodes
    ${nodeId}              Get Node By Name    ${tmp}     test-node
    should not be equal    ${nodeId}           0

    Delete    /api/v2/node    {"id": ${nodeId}}

    Integer    response status    200

    GET    /api/v2/node?type=${NODE_DRIVER}

    Integer    response status    200
    ${tmp}     Array              $.nodes

    ${nodeId}                      Get Node By Name    ${tmp}    test-node
    should be equal as integers    ${nodeId}           0

Delete a non-existent node, it will return failure
    Delete    /api/v2/node    {"id": 1000}

    Integer    response status        404
    Integer    response body error    ${ERR_NODE_NOT_EXIST}

Add setting to non-existent node, it should return failure
    POST    /api/v2/node/setting    {"node_id": 999, "params": {"host": "1.1.1.1", "port": 6677, "connection_mode": 0}}

    Integer    response status        404
    Integer    response body error    ${ERR_NODE_NOT_EXIST}

Get setting from non-existent node, it should return failure
    GET    /api/v2/node/setting?node_id=999

    Integer    response status        404
    Integer    response body error    ${ERR_NODE_NOT_EXIST}

Get setting from a node that has never been set, it should return failure
	${driver_node_id}    Add Node    ${NODE_DRIVER}    driver-nodee    ${PLUGIN_MODBUS_TCP}

    GET    /api/v2/node/setting?node_id=${driver_node_id}

    Integer    response status        200
    Integer    response body error    ${ERR_NODE_SETTING_NOT_EXIST}

    Del Node    ${driver_node_id}

Add the correct settings to the node, it should return success
	${driver_node_id}    Add Node    ${NODE_DRIVER}    driver-node    ${PLUGIN_MODBUS_TCP}

    POST    /api/v2/node/setting    {"node_id": ${driver_node_id}, "params": {"host": "1.1.1.1", "port": 6677, "connection_mode": 0}}

    Integer    response status        200
    Integer    response body error    ${ERR_SUCCESS}

Get the setting of node, it should return to the previous setting
	${driver_node_id}	Get Node ID    ${NODE_DRIVER}    driver-node

    GET    /api/v2/node/setting?node_id=${driver_node_id}

    Integer    response status          200
    Integer    response body node_id    ${driver_node_id}

    Integer    $.params.port               6677
    Integer    $.params.connection_mode    0
    String     $.params.host               "1.1.1.1"


Add wrong settings to node, it should return failure
	${driver_node_id}	Get Node ID    ${NODE_DRIVER}    driver-node

    POST    /api/v2/node/setting    {"node_id": ${driver_node_id}, "params": {"xxhost": "1.1.1.1", "port": 6677, "connection_mode": 0}}

    Integer    response status        400
    Integer    response body error    ${ERR_NODE_SETTING_INVALID}

Update a node that does not exist, it should return failure
    PUT    /api/v2/node    {"id": 99, "name": "test-name"}

    Integer    response status        404
    Integer    response body error    ${ERR_NODE_NOT_EXIST}

Update the name of the node, it should return success
	${app_node_id}    Add Node    ${NODE_APP}    app-node    ${PLUGIN_MQTT}

    PUT    /api/v2/node    {"id": ${app_node_id}, "name": "test-name"}

    Integer    response status    200

    GET    /api/v2/node?type=${NODE_APP}

    Integer    response status    200
    Array      $.nodes
    String     $.nodes[*].name    sample-app-adapter    test-name

    [Teardown]    Del Node    ${app_node_id}


Start an unconfigured node, it should return failure
	${modbus_node_id}    Add Node    ${NODE_DRIVER}    modbus-test-node    ${PLUGIN_MODBUS_TCP}

    Sleep    1s

    ${running_state}    ${link_state}    Get Node State    ${modbus_node_id}    

    should be equal as integers    ${running_state}    ${NODE_STATE_INIT}
    should be equal as integers    ${link_state}       ${NODE_LINK_STATE_DISCONNECTED}


    POST    /api/v2/node/ctl    {"id": ${modbus_node_id}, "cmd": ${NODE_CTL_START}}

    Integer    response status    409
    Integer    $.error            ${ERR_NODE_NOT_READY}

    [Teardown]    Del Node    ${modbus_node_id}

Start the configured node, it should return success
    ${modbus_node_id}    Add Node    ${NODE_DRIVER}    modbus-test-node    ${PLUGIN_MODBUS_TCP}

    Sleep    1s

    Node Setting    ${modbus_node_id}    {"host": "127.0.0.1", "port": 502, "connection_mode": 0}

    ${running_state}    ${link_state}    Get Node State    ${modbus_node_id}    

    should be equal as integers    ${running_state}    ${NODE_STATE_READY}
    should be equal as integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}


    POST    /api/v2/node/ctl    {"id": ${modbus_node_id}, "cmd": ${NODE_CTL_START}}

    Integer    response status    200

    ${running_state}    ${link_state}    Get Node State    ${modbus_node_id}    

    should be equal as integers    ${running_state}    ${NODE_STATE_RUNNING}
    should be equal as integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}


Start the stopped node, it should return success
    ${modbus_node_id}    Get Node ID    ${NODE_DRIVER}    modbus-test-node

    Node Ctl    ${modbus_node_id}    ${NODE_CTL_STOP}

    ${running_state}    ${link_state}    Get Node State    ${modbus_node_id}    

    should be equal as integers    ${running_state}    ${NODE_STATE_STOP}
    should be equal as integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

    POST    /api/v2/node/ctl    {"id": ${modbus_node_id}, "cmd": ${NODE_CTL_START}}

    Integer    response status    200

    ${running_state}    ${link_state}    Get Node State    ${modbus_node_id}    

    should be equal as integers    ${running_state}    ${NODE_STATE_RUNNING}
    should be equal as integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

Start the running node, it should return failure
    ${modbus_node_id}    Get Node ID    ${NODE_DRIVER}    modbus-test-node

    POST    /api/v2/node/ctl    {"id": ${modbus_node_id}, "cmd": ${NODE_CTL_START}}

    Integer    response status    409
    Integer    $.error            ${ERR_NODE_IS_RUNNING}

    [Teardown]    Node Ctl    ${modbus_node_id}    ${NODE_CTL_STOP}

Stop node that is stopped, it should return failure
    ${modbus_node_id}    Get Node ID    ${NODE_DRIVER}    modbus-test-node

    POST    /api/v2/node/ctl    {"id": ${modbus_node_id}, "cmd": ${NODE_CTL_STOP}}

    Integer    response status    409
    Integer    $.error            ${ERR_NODE_IS_STOPED}

    [Teardown]    Del Node    ${modbus_node_id}

Stop node that is ready(init/idle), it should return failure
    ${modbus_node_id}    Add Node    ${NODE_DRIVER}    modbus-test-node    ${PLUGIN_MODBUS_TCP}

    Sleep    1s

    POST    /api/v2/node/ctl    {"id": ${modbus_node_id}, "cmd": ${NODE_CTL_STOP}}

    Integer    response status    409
    Integer    $.error            ${ERR_NODE_NOT_RUNNING}

    Node Setting    ${modbus_node_id}    {"host": "127.0.0.1", "port": 502, "connection_mode": 0}

    POST    /api/v2/node/ctl    {"id": ${modbus_node_id}, "cmd": ${NODE_CTL_STOP}}

    Integer    response status    409
    Integer    $.error            ${ERR_NODE_NOT_RUNNING}

    [Teardown]    Del Node    ${modbus_node_id}

When setting up a READY/RUNNING/STOPED node, the node status will not change
    ${modbus_node_id}    Add Node    ${NODE_DRIVER}    modbus-test-node    ${PLUGIN_MODBUS_TCP}

    Sleep    1s

    Node Setting    ${modbus_node_id}    {"host": "127.0.0.1", "port": 502, "connection_mode": 0}

    ${running_state}    ${link_state}    Get Node State    ${modbus_node_id}    

    should be equal as integers    ${running_state}    ${NODE_STATE_READY}
    should be equal as integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

    Node Setting    ${modbus_node_id}    {"host": "127.0.0.1", "port": 502, "connection_mode": 0}

    ${running_state}    ${link_state}    Get Node State    ${modbus_node_id}    

    should be equal as integers    ${running_state}    ${NODE_STATE_READY}
    should be equal as integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

    Node Ctl    ${modbus_node_id}    ${NODE_CTL_START}

    ${running_state}    ${link_state}    Get Node State    ${modbus_node_id}    

    should be equal as integers    ${running_state}    ${NODE_STATE_RUNNING}
    should be equal as integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

    Node Ctl    ${modbus_node_id}    ${NODE_CTL_STOP}

    ${running_state}    ${link_state}    Get Node State    ${modbus_node_id}    

    should be equal as integers    ${running_state}    ${NODE_STATE_STOP}
    should be equal as integers    ${link_state}       ${NODE_LINK_STATE_CONNECTING}

*** Keywords ***
Neuron Context Ready
	Neuron Ready

    ${token} =     LOGIN
    ${jwt} =       Catenate                      Bearer    ${token}
    Set Headers    {"Authorization":"${jwt}"}

Neuron Context Stop
    LOGOUT
    Stop Neuron
