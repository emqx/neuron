*** Settings ***
Library    Keyword.Neuron.Read

*** Variables ***
${NODE_UNKNOWN}	0
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

${TAG_ATTRIBUTE_READ}	1
${TAG_ATTRIBUTE_WRITE}	2
${TAG_ATTRIBUTE_SUBSCRIBE}	4
${TAG_ATTRIBUTE_RW}	3

${TAG_DATA_TYPE_BYTE}	2
${TAG_DATA_TYPE_INT8}	3
${TAG_DATA_TYPE_INT16}	4
${TAG_DATA_TYPE_INT32}	5
${TAG_DATA_TYPE_INT64}	6
${TAG_DATA_TYPE_UINT8}	7
${TAG_DATA_TYPE_UINT16}	8
${TAG_DATA_TYPE_UINT32}	9
${TAG_DATA_TYPE_UINT64}	10
${TAG_DATA_TYPE_FLOAT}	11
${TAG_DATA_TYPE_DOUBLE}	12
${TAG_DATA_TYPE_BOOL}	13
${TAG_DATA_TYPE_BIT}	14
${TAG_DATA_TYPE_STRING}	15

${PLUGIN_MODBUS_TCP}	modbus-tcp-plugin
${PLUGIN_MQTT}    mqtt-plugin

${MQTT_CONFIG_HOST}    broker.fengzero.com
${MQTT_CONFIG_REQ_TOPIC}	test/mqtt_req
${MQTT_CONFIG_RES_TOPIC}	test/mqtt_res
${MQTT_CONFIG}    {"req-topic": "${MQTT_CONFIG_REQ_TOPIC}", "res-topic": "${MQTT_CONFIG_RES_TOPIC}", "ssl": false, "host": "${MQTT_CONFIG_HOST}", "port": 1883, "username": "", "password": "", "ca-path":"", "ca-file": ""}

${MODBUS_TCP_CONFIG}	{"host": "127.0.0.1", "port": 60502, "connection_mode": 0}
${OPCUA_CONFIG}    {"host": "127.0.0.1", "port": 4840, "username": "admin", "password": "0000"}

*** Keywords ***
To Array
	[Arguments]	${str_array}
	${tmp}                      Array    ${str_array}
	[RETURN]	${tmp}[0]

Neuron Ready
    Start Neuron
    Sleep           3

LOGIN
	POST    /api/v2/login	{"name": "admin", "pass":"0000"}

    Integer    response status	200

	${token}	String         $.token
	[Return]	${token}[0]

LOGOUT
	POST	/api/v2/logout
	Integer	response status	200

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

	POST       /api/v2/node/setting    {"node_id": ${node_id}, "params": ${config}}
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

Add Tags
	[Arguments]	${node_id}	${group}	${tags}

	POST	/api/v2/tags	{"node_id": ${node_id}, "group_config_name": "${group}", "tags": [${tags}]}

	Integer    response status    200

Del Tags
	[Arguments]	${node_id}	${group}	${ids}

	DELETE    /api/v2/tags    {"node_id": ${node_id}, "group_config_name": "${group}", "ids": [${ids}]}

	Integer    response status    200

Get Tag ID
	[Arguments]	${node_id}	${group}	${tag_name}

	GET    /api/v2/tags?node_id=${node_id}&group_config_name=${group}

	Integer    response status    200

	${result} =	To Array    $.tags

	${id} =     Tag Find By Name	${result}	${tag_name}
	[Return]    ${id}

Read Tags
	[Arguments]	${node_id}	${group}

	POST	/api/v2/read    {"node_id": ${node_id}, "group_config_name": "${group}"}

	Integer       response status    200
	output        $
	[Return]	$

Write Tags
	[Arguments]	${node_id}	${group}	${tags}

	POST    /api/v2/write    {"node_id": ${node_id}, "group_config_name": "${group}", "tags":[${tags}]}

	Integer    response status    200

Compare Tag Value As Int
	[Arguments]	${tags}    ${id}    ${value}

	${val} =	To Array                 ${tags}
	${ret} =	Compare Tag Value Int    ${val}	${id}	${value}

	should be equal as integers	${ret}    0

Compare Tag Value As Float
	[Arguments]	${tags}    ${id}	${value}

	${val} =	To Array                   ${tags}
	${ret} =	Compare Tag Value Float    ${val}	${id}	${value}

	should be equal as integers	${ret}    0

Compare Tag Value As String
	[Arguments]	${tags}    ${id}	${value}

	${val} =	To Array                    ${tags}
	${ret} =	Compare Tag Value String    ${val}	${id}	${value}

	should be equal as integers	${ret}    0
