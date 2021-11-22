*** Variables ***
${NODE_DRIVER}	1
${NODE_WEB}	2
${NODE_MQTT}	3
${NODE_STREAM_PROCESSOR}	4
${NODE_APP}	5

${PLUGIN_MODBUS}	modbus-tcp-plugin
${PLUGIN_MQTT}    mqtt-plugin

*** Keywords ***
Neuron Ready
    Start Neuron
    Sleep           3

Get Node ID
    [Arguments]    ${node_type}                      ${node_name}
    GET            /api/v2/node?type=${node_type}
    Integer        response status                   200
    ${tmp}         Array                             $.nodes
    ${node_id}     Get Node By Name                  ${tmp}          ${node_name}

    should not be equal as integers    ${node_id}    0

    [RETURN]	${node_id}

Add Node
	[Arguments]	${node_type}	${node_name}	${plugin_name}

	POST       /api/v2/node           {"type": ${node_type}, "name": "${node_name}", "plugin_name": "${plugin_name}"}
	Integer    response status        200
	Integer    response body error    ${ERR_SUCCESS}

	${node_id}                         Get Node ID    ${node_type}    ${node_name}
	should not be equal as integers    ${node_id}     0
	[RETURN]	${node_id}

Del Node
	[Arguments]	${node_id}

	Delete     /api/v2/node       {"id": ${node_id}}
	Integer    response status    200

Add Group Config
	[Arguments]	${node_id}	${grp_config_name}
	POST	/api/v2/gconfig                         {"name": "${grp_config_name}", "node_id": ${node_id}, "interval": 1000}

	Integer    response status        200
	Integer    response body error    ${ERR_SUCCESS}

Del Group Config
	[Arguments]	${node_id}	${grp_config_name}

	DELETE     /api/v2/gconfig    {"node_id": ${node_id}, "name": "${grp_config_name}"}
	Integer    response status    200

Subscribe Group
	[Arguments]	${driver_node_id}	${app_node_id}    ${group_config_name}

	POST	/api/v2/subscribe	{"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "${group_config_name}"}

	Integer    response status                       200
	Integer    response body error	${ERR_SUCCESS}

To Array
	[Arguments]	${str_array}
	${tmp}                      Array    ${str_array}
	[RETURN]	${tmp}[0]
