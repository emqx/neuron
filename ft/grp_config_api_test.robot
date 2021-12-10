*** Settings ***
Resource          api_http.resource
Suite Setup       Neuron Context Ready
Suite Teardown    Neuron Context Stop

*** Variables ***
${test_gconfig}    test_gconfig
${test_node}	test_node
${driver_node}	driver-node
${app_node}	app-node

*** Test Cases ***
Add group config under non-existent node, it should return failure
	POST	/api/v2/gconfig    {"name": "${test_gconfig}", "node_id": 1234, "interval": 1000}

	Integer    response status                              404
	Integer    response body error	${ERR_NODE_NOT_EXIST}

Add group config under existent node, it should return success
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	POST	/api/v2/gconfig    {"name": "${test_gconfig}", "node_id": ${test_node_id}, "interval": 1000}

	Integer    response status    200

Delete a non-existent group config, it should return failure
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	DELETE     /api/v2/gconfig                                   {"node_id": ${test_node_id}, "name": "xxxx"}
	Integer    response status                                   404
	Integer    response body error	${ERR_GRPCONFIG_NOT_EXIST}

Delete a group config from non-existent node, it should return failure
	DELETE    /api/v2/gconfig    {"node_id": 1234, "name": "${test_gconfig}"}

	Integer    response status                                   404
	Integer    response body error	${ERR_GRPCONFIG_NOT_EXIST}

Delete a group config, it should return success
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	DELETE    /api/v2/gconfig    {"node_id": ${test_node_id}, "name": "${test_gconfig}"}

	Integer    response status    200

Update a non-existent group config, it should return failure
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	PUT    /api/v2/gconfig    {"node_id": ${test_node_id}, "name": "xxxx", "interval": 2000}

	Integer    response status                                   404
	Integer    response body error	${ERR_GRPCONFIG_NOT_EXIST}

Update a group config from non-existent node, it should return failure
	PUT    /api/v2/gconfig    {"node_id": 1234, "name": "${test_gconfig}", "interval": 2000}

	Integer    response status                                   404
	Integer    response body error	${ERR_GRPCONFIG_NOT_EXIST}

Add a group config to the node, then get the group config from this node, it should return the group config
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	POST	/api/v2/gconfig    {"name": "${test_gconfig}", "node_id": ${test_node_id}, "interval": 1000}

	Integer    response status    200

	GET	/api/v2/gconfig?node_id=${test_node_id}

	${tmp}     Array	$.group_configs
	${size}    Group Config Size	${tmp}

	should be equal as integers	${size}                             1
	${interval}	Get Interval In Group Config	${tmp}	test_gconfig
	should be equal as integers	${interval}	1000

Update the interval of the test group config, it should return success
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	PUT    /api/v2/gconfig    {"node_id": ${test_node_id}, "name": "${test_gconfig}", "interval": 2000}

	Integer    response status    200

	GET	/api/v2/gconfig?node_id=${test_node_id}

	${tmp}    Array	$.group_configs

	${interval}	Get Interval In Group Config	${tmp}	test_gconfig
	should be equal as integers	${interval}	2000

North APP subscribe non-existent group config, it should return failure
	${app_node_id}       Add Node    ${NODE_APP}       ${app_node}       ${PLUGIN_MQTT}
	${driver_node_id}    Add Node    ${NODE_DRIVER}    ${driver_node}    ${PLUGIN_MODBUS_TCP}

	POST	/api/v2/subscribe	{"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "grp_config"}

	Integer    response status                                   404
	Integer    response body error	${ERR_GRPCONFIG_NOT_EXIST}

North APP subscribe group config, it should return success
	${app_node_id}	Get Node ID       ${NODE_APP}                     ${app_node}
	${driver_node_id}	Get Node ID    ${NODE_DRIVER}                  ${driver_node}
	Add Group Config                 ${driver_node_id}	grp_config

	POST	/api/v2/subscribe	{"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "grp_config"}

	Integer    response status                       200
	Integer    response body error	${ERR_SUCCESS}

Query the subscribers of the group config, it should return all nodes subscribed to this group
	${app_node_id}	Get Node ID        ${NODE_APP}       ${app_node}
	${driver_node_id1}	Get Node ID    ${NODE_DRIVER}    ${driver_node}

	GET    /api/v2/subscribe?node_id=${app_node_id}

	Integer    response status    200

	${result}	To Array                             $.groups
	${groups_len}	Array Len	${result}
	should be equal as integers	${groups_len}	1

	${check_ret}	Subscribe_Check    ${result}	${driver_node_id1}	grp_config

	should be equal as integers    ${check_ret}    0

	${driver_node_id2}    Add Node    ${NODE_DRIVER}    driver-node2    ${PLUGIN_MODBUS_TCP}

	Add Group Config    ${driver_node_id2}	grp_config
	Subscribe Group     ${driver_node_id2}               ${app_node_id}	grp_config

	GET    /api/v2/subscribe?node_id=${app_node_id}

	Integer    response status    200

	${result}	To Array                             $.groups
	${groups_len}	Array Len	${result}
	should be equal as integers	${groups_len}	2

	${check_ret}	Subscribe_Check    ${result}	${driver_node_id1}	grp_config

	should be equal as integers    ${check_ret}    0

	${check_ret}	Subscribe_Check    ${result}	${driver_node_id2}	grp_config

	should be equal as integers    ${check_ret}    0

Delete the subscribe group config, it should return failure
	${driver_node_id1}	Get Node ID    ${NODE_DRIVER}    ${driver_node}

	DELETE    /api/v2/gconfig    {"node_id": ${driver_node_id1}, "name": "grp_config"}

	Integer    response status                                 412
	Integer    response body error	${ERR_GRP_CONFIG_IN_USE}

North APP unsubscribe non-existent group config, it should return failure
	${app_node_id}	Get Node ID       ${NODE_APP}       ${app_node}
	${driver_node_id}	Get Node ID    ${NODE_DRIVER}    ${driver_node}

	DELETE    /api/v2/subscribe	{"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "xxgrp_config"}

	Integer    response status                                   404
	Integer    response body error	${ERR_GRPCONFIG_NOT_EXIST}

North APP unsubscribe group config, it should return success
	${app_node_id}	Get Node ID        ${NODE_APP}       ${app_node}
	${driver_node_id}	Get Node ID     ${NODE_DRIVER}    ${driver_node}
	${driver_node_id2}	Get Node ID    ${NODE_DRIVER}    driver-node2

	DELETE    /api/v2/subscribe	{"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "grp_config"}

	Integer    response status                       200
	Integer    response body error	${ERR_SUCCESS}

	GET    /api/v2/subscribe?node_id=${app_node_id}

	Integer    response status    200

	${result}	To Array                             $.groups
	${groups_len}	Array Len	${result}
	should be equal as integers	${groups_len}	1

	${check_ret}	Subscribe_Check    ${result}	${driver_node_id2}	grp_config

	should be equal as integers    ${check_ret}    0


*** Keywords ***
Neuron Context Ready
	Neuron Ready

	${token} =    LOGIN
	${jwt} =      Catenate    Bearer    ${token}

    Set Headers    {"Authorization":"${jwt}"}

	Add Node	${NODE_DRIVER}    ${test_node}	${PLUGIN_MODBUS_TCP}

Neuron Context Stop
    LOGOUT
    Stop Neuron
