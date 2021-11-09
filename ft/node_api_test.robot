*** Settings ***
Library     Keyword.Neuron.Neuron
Library     Keyword.Node.Node
Resource    error.robot
Resource    neuron.robot
Library     REST    http://127.0.0.1:7001
Suite Setup     Neuron Ready
Suite Teardown  Stop Neuron

*** Variables ***

*** Test Cases ***
POST ping, status can be ok
    POST        /api/v2/ping
    Integer     response status     200
    Object      response body       {}

Use the wrong body to crate a node, it should return failure
    POST        /api/v2/node        {"type": 1, "namexx": "test-node", "plugin_name": "test-plugin-name"}
    Integer     response status     400
    Integer     response body error     ${err_request_body_invalid}

Use a plugin name that does not exist to create a node, it should return failure
    POST        /api/v2/node        {"type": 1, "name": "test-node", "plugin_name": "test-plugin-name"}
    Integer     response status     400
    Integer     response body error     ${err_plugin_name_not_found}

Use a invalid node type to create a node, it should return failure
    POST        /api/v2/node        {"type": 10, "name": "test-node", "plugin_name": "modbus-tcp-plugin"}
    Integer     response status     400
    Integer     response body error     ${err_node_type_invalid}

Create a node with the correct body, it should return success
    POST        /api/v2/node        {"type": 1, "name": "test-node", "plugin_name": "modbus-tcp-plugin"}
    Integer     response status     201
    Integer     response body error     ${err_success} 

Create an existing node, it should return failure
    POST        /api/v2/node        {"type": 1, "name": "test-node", "plugin_name": "modbus-tcp-plugin"}
    Integer     response status     400
    Integer     response body error     ${err_node_exist}

Get node with wrong parameters, it will return failure
    GET         /api/v2/node?tt=1
    Integer     response status     400
    Integer     response body error     ${err_request_param_invalid}

Get DRIVER node, it should return all DRIVER node
    GET         /api/v2/node?type=1
    Integer     response status     200
    Array       $.nodes
    String      $.nodes[*].name     sample-driver-adapter   modbus-tcp-adapter  opcua-adapter   test-node

Get WEB node, it should return all WEB node
    GET         /api/v2/node?type=2
    Integer     response status     200
    Array       $.nodes
    String      $.nodes[*].name     sample-app-adapter  mqtt-adapter    default-dashboard-adapter
   # String      $.nodes[*].name     default-dashboard-adapter

Get MQTT node, it should return all mqtt node
    GET         /api/v2/node?type=3
    Integer     response status     200
    Array       $.nodes
    String      $.nodes[*].name     sample-app-adapter  mqtt-adapter    default-dashboard-adapter
   # String      $.nodes[*].name    mqtt-adapter

Get STREAM PROCESSOR node, it should return all STREAM PROCESSOR node
    GET         /api/v2/node?type=4
    Integer     response status     200
    Array       $.nodes
    String      $.nodes[*].name     sample-app-adapter  mqtt-adapter    default-dashboard-adapter
   # String      $.nodes[*].name    stream-process-adapter

Get APP node, it should return all APP node
    GET         /api/v2/node?type=5
    Integer     response status     200
    Array       $.nodes
    String      $.nodes[*].name     sample-app-adapter  mqtt-adapter    default-dashboard-adapter
   # String      $.nodes[*].name    app-adapter

Get UNKNOWN type node, it should return empty node
    GET         /api/v2/node?type=6
    Integer     response status     200
    Array       $.nodes
    String      $.nodes[*].name     sample-app-adapter  mqtt-adapter    default-dashboard-adapter
   # String     response body   {}

Delete the existing node, it will return success
    GET         /api/v2/node?type=1
    Integer     response status     200
    ${tmp}   Array   $.nodes
    ${nodeId}   Get Node By Name    ${tmp}   test-node
    should not be equal     ${nodeId}   0
    Delete      /api/v2/node        {"id": ${nodeId}}
    Integer     response status     200
    GET         /api/v2/node?type=1
    Integer     response status     200
    ${tmp}   Array   $.nodes
    ${nodeId}   Get Node By Name    ${tmp}   test-node
    should be equal as integers     ${nodeId}   0

Delete a non-existent node, it will return failure
    Delete      /api/v2/node        {"id": 1000}
    Integer     response status     400
    Integer     response body error     ${err_node_not_exist}

