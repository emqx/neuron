*** Variables ***
${NODE_DRIVER}	1
${NODE_WEB}	2
${NODE_MQTT}	3
${NODE_STREAM_PROCESSOR}	4
${NODE_APP}	5

${NODE_CTL_START}	0
${NODE_CTL_STOP}	1

${NODE_STATE_IDLE}	0
${NODE_STATE_INIT}	1
${NODE_STATE_READY}	2
${NODE_STATE_RUNNING}	3
${NODE_STATE_STOP}	4

${NODE_LINK_STATE_DISCONNECTED}    0
${NODE_LINK_STATE_CONNECTING}      1
${NODE_LINK_STATE_CONNECTED}       2

${PLUGIN_MODBUS_TCP}	modbus-tcp-plugin
${PLUGIN_MQTT}    mqtt-plugin

${MQTT_CONFIG_HOST}    broker.fengzero.com
${MQTT_CONFIG_REQ_TOPIC}	test/mqtt_req
${MQTT_CONFIG_RES_TOPIC}	test/mqtt_res
${MQTT_CONFIG}    {"req-topic": "${MQTT_CONFIG_REQ_TOPIC}", "res-topic": "${MQTT_CONFIG_RES_TOPIC}", "ssl": false, "host": "${MQTT_CONFIG_HOST}", "port": 1883, "username": "", "password": "", "ca-path":"", "ca-file": ""}

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

Node Setting
	[Arguments]	${node_id}	${config}

	POST       /api/v2/node/setting    ${config}
	Integer    response status         200

Get Node State
	[Arguments]	${node_id}

	GET        /api/v2/node/state?node_id=${node_id}
	Integer    response status                          200

	${running}    Integer    $.running
	${link}       Integer    $.link

	[RETURN]	${running}[0]    ${link}[0]

Node Ctl
	[Arguments]	${node_id}	${cmd}

	POST	/api/v2/node/ctl	{"id": ${node_id}, "cmd": ${cmd}}

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
