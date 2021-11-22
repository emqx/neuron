*** Settings ***
Library           Keyword.Neuron.Neuron
Library           Keyword.Neuron.Node
Resource          error.robot
Resource          neuron.robot
Library           REST                     http://127.0.0.1:7001
Suite Setup       Neuron Ready
Suite Teardown    Stop Neuron

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
    GET    /api/v2/node?type=6

    Integer    response status    200
    Object     response body      {}

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


#Get setting from non-existent node, it should return failure
    #GET    /api/v2/node/setting?node_id=999

    #Integer    response status        404
    #Integer    response body error    ${ERR_NODE_NOT_EXIST}

#Get setting from a node that has never been set, it should return failure
	#${driver_node_id}    Add Node    ${NODE_DRIVER}    driver-node1    ${PLUGIN_MODBUS}

    #GET    /api/v2/node/setting?node_id=${driver_node_id}

    #Integer    response status        404
    #Integer    response body error    ${ERR_NODE_SETTING_NOT_EXIST}

    #Del Node    ${driver_node_id}

Add the correct settings to the node, it should return success
	${driver_node_id}    Add Node    ${NODE_DRIVER}    driver-node    ${PLUGIN_MODBUS}

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


#Start an unconfigured node, it should return failure

#Start the configured node, it should return success

#Start the stopped node, it should return success

#Start the running node, it should restart

#Stop node that is not running, it should return failure

#Stop the running node, it should return success

#When setting up an IDLE node, the node status should become READY

#When setting up a READY/RUNNING/STOPED node, the node status will not change