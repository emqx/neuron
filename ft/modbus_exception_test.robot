*** Settings ***
Resource          common.resource
Suite Setup       Neuron Context Ready
Suite Teardown    Stop All Processes

*** Variables ***
${test_node_name}       modbus-tcp-adapter
${test_node_id}
${group}    config_modbus_tcp_sample_2


*** Test Cases ***
The connection between the modbus driver and the modbus simulator is disconnected, and when reconnected, the tag can be write correctly
    Skip If Not Http API

	Start Simulator Arg    ${MODBUS_TCP_SERVER_SIMULATOR}	${SIMULATOR_DIR}	9000

	${tag1_id} =    Add Tag And Return ID    ${test_node_id}    ${group}    {"name": "tag1", "address": "1!000008", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BIT}}

	Should Not Be Equal As Integers    ${tag1_id}    -1

	Sleep         1s 500ms
	Write Tags    ${test_node_name}        ${group}                       tag1      1
	Integer       response body error    ${ERR_PLUGIN_WRITE_FAILURE}

	Node Setting    ${test_node_id}    {"host": "127.0.0.1", "port": 9000, "connection_mode": 0}

	Write Tags    ${test_node_name}        ${group}         tag1        1
	Integer       response body error    ${ERR_SUCCESS}

	[Teardown]	Terminate All Processes    kill=false

*** Keywords ***
Neuron Context Ready
    Import Neuron API Resource

    Neuron Ready

    LOGIN

    Add Node    type=${${NODE_DRIVER}}    name=modbus-tcp-adapter    plugin_name=modbus-tcp
    ${id} =    Get Node ID    ${NODE_DRIVER}    modbus-tcp-adapter

    Set Global Variable    ${test_node_id}    ${id}

    Node Setting           ${test_node_id}    ${MODBUS_TCP_CONFIG}
    Update Group Config    ${test_node_id}    ${group}                300

Stop All Processes
    Stop Neuron                remove_persistence_data=True
    sleep                      1s
    Terminate All Processes    kill=false