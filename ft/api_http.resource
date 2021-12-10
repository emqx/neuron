*** Settings ***
Library    REST                     http://127.0.0.1:7001
Resource   common.resource

*** Keywords ***

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
