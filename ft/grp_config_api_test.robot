*** Settings ***
Library     Keyword.Neuron.Neuron
Library     Keyword.Node.Node
Library	    Keyword.GrpConfig.GrpConfig
Resource    error.robot
Resource    neuron.robot
Library     REST    http://127.0.0.1:7001
Suite Setup     Neuron Context Ready
Suite Teardown  Stop Neuron

*** Variables ***
${test_gconfig}		"test_gconfig"

*** Test Cases ***
Add group config under non-existent node, it should return failure
	POST	/api/v2/gconfig		{"name": ${test_gconfig}, "node_id": 1234, "interval": 1000}
	Integer		response status		400
	Integer		response body error	${err_node_not_exist}

Add group config under existent node, it should return success
	${test_node_id}		Get Test Node ID
	POST	/api/v2/gconfig		{"name": ${test_gconfig}, "node_id": ${test_node_id}, "interval": 1000}
	Integer		response status		201

Delete a non-existent group config, it should return failure
	${test_node_id}		Get Test Node ID
	DELETE		/api/v2/gconfig		{"node_id": ${test_node_id}, "name": "xxxx"}
	Integer		response status		404
	Integer		response body error	${err_grpconfig_not_exist}

Delete a group config from non-existent node, it should return failure
	DELETE		/api/v2/gconfig		{"node_id": 1234, "name": ${test_gconfig}}
	Integer		response status		404
	Integer		response body error	${err_grpconfig_not_exist}

Delete a group config, it should return success
	${test_node_id}		Get Test Node ID
	DELETE		/api/v2/gconfig		{"node_id": ${test_node_id}, "name": ${test_gconfig}}
	Integer		response status		200

Update a non-existent group config, it should return failure
	${test_node_id}		Get Test Node ID
	PUT		/api/v2/gconfig		{"node_id": ${test_node_id}, "name": "xxxx", "interval": 2000}
	Integer		response status		404
	Integer		response body error	${err_grpconfig_not_exist}

Update a group config from non-existent node, it should return failure
	PUT		/api/v2/gconfig		{"node_id": 1234, "name": ${test_gconfig}, "interval": 2000}
	Integer		response status		404
	Integer		response body error	${err_grpconfig_not_exist}

Add a group config to the node, then get the group config from this node, it should return the group config
	${test_node_id}		Get Test Node ID
	POST	/api/v2/gconfig		{"name": ${test_gconfig}, "node_id": ${test_node_id}, "interval": 1000}
	Integer		response status		201
	GET	/api/v2/gconfig?node_id=${test_node_id}
	${tmp}		Array	$.group_configs
	${size}		Group Config Size	${tmp}
	should be equal as integers	${size}		1
	${interval}	Get Interval In Group Config	${tmp}	test_gconfig
	should be equal as integers	${interval}	1000

Update the interval of the test group config, it should return success
	${test_node_id}		Get Test Node ID
	${test_node_id}		Get Test Node ID
	PUT		/api/v2/gconfig		{"node_id": ${test_node_id}, "name": ${test_gconfig}, "interval": 2000}
	Integer		response status		200
	GET	/api/v2/gconfig?node_id=${test_node_id}
	${tmp}		Array	$.group_configs
	${interval}	Get Interval In Group Config	${tmp}	test_gconfig
	should be equal as integers	${interval}	2000


	
*** Keywords ***
Get Test Node ID
    	GET         /api/v2/node?type=1
    	Integer     response status     200
    	${tmp}   Array   $.nodes
    	${test_node_id} 	Get Node By Name    ${tmp}   test-node
	[RETURN]	${test_node_id}
Add Test Node
	POST        /api/v2/node        {"type": 1, "name": "test-node", "plugin_name": "modbus-tcp-plugin"}
	Integer     response status     201
	Integer     response body error     ${err_success} 
    	${test_node_id} 	Get Test Node ID
    	should not be equal	${test_node_id}   0
Neuron Context Ready
	Neuron Ready
	Add Test Node