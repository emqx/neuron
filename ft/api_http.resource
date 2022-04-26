*** Settings ***
Library    REST                     http://127.0.0.1:7001
Resource   common.resource

*** Keywords ***

Check Error Code
  [Arguments]         ${result}               ${errcode}
  Integer             response body error     ${errcode}

Check Response Status
  [Arguments]         ${result}               ${status}
  Integer             response status         ${status}

LOGIN
  POST                /api/v2/login           {"name": "admin", "pass":"0000"}

  Integer             response status         200

  ${token}            String                  $.token
  ${jwt} =            Catenate                Bearer                          ${token}[0]
  Set Headers         {"Authorization":"${jwt}"}
  Add MQTT Node

  [Return]            ${token}[0]

Add MQTT Node
    Add Node    type=${${NODE_MQTT}}      name=mqtt-adapter          plugin_name=mqtt

Add Plugin
  [Arguments]         ${node_type}            ${name}                      ${kind}                      ${lib_name}
  POST                /api/v2/plugin          {"node_type": ${node_type}, "name": ${name}, "kind": ${kind}, "lib_name": ${lib_name}}

  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Add Node
  [Arguments]         &{args}
  POST                /api/v2/node            ${args}

  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Add Node And Return ID
  [Arguments]         ${node_type}            ${node_name}                  ${plugin_name}
  ${res} =            Add Node                type=${${node_type}}          name=${node_name}             plugin_name=${plugin_name}

  Check Response Status                       ${res}                        200
  Check Error Code                            ${res}                        ${ERR_SUCCESS}

  ${node_id} =        Get Node ID             ${node_type}                  ${node_name}
  Should Not Be Equal As Integers             ${node_id}                    0

  [RETURN]            ${node_id}

Get Nodes
  [Arguments]         ${node_type}
  GET                 /api/v2/node?type=${node_type}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Del Node
  [Arguments]         ${node_id}
  Delete              /api/v2/node            {"id": ${node_id}}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Get Node ID
  [Arguments]         ${node_type}            ${node_name}
  ${res} =            Get Nodes               ${node_type}
  Check Response Status                       ${res}                        200

  ${node_id}          Get Node By Name        ${res}[nodes]                 ${node_name}

  Should Not Be Equal As Integers             ${node_id}                    0

  [RETURN]            ${node_id}

Node Setting
  [Arguments]         ${node_id}              ${config}
  POST                /api/v2/node/setting    {"node_id": ${node_id}, "params": ${config}}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Get Node Setting
  [Arguments]         ${node_id}
  GET                 /api/v2/node/setting?node_id=${node_id}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Get Node State
  [Arguments]         ${node_id}
  GET                 /api/v2/node/state?node_id=${node_id}
  ${running} =        Integer                 $.running
  ${link} =           Integer                 $.link

  [RETURN]            ${running}[0]           ${link}[0]

Node Ctl
  [Arguments]         ${node_id}  ${cmd}
  POST                /api/v2/node/ctl        {"id": ${node_id}, "cmd": ${cmd}}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Ping
  POST                /api/v2/ping
  Integer             response status         200
  Object              response body           {}

Add Group Config
  [Arguments]         ${node_id}              ${grp_config_name}            ${interval}
  POST                /api/v2/gconfig         {"name": "${grp_config_name}", "node_id": ${node_id}, "interval": ${interval}}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Get Group Config
  [Arguments]         ${node_id}
  GET                 /api/v2/gconfig?node_id=${node_id}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Update Group Config
  [Arguments]         ${node_id}              ${grp_config_name}            ${interval}
  PUT                 /api/v2/gconfig         {"node_id": ${node_id}, "name": "${grp_config_name}", "interval": ${interval}}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Del Group Config
  [Arguments]         ${node_id}              ${grp_config_name}
  DELETE              /api/v2/gconfig         {"node_id": ${node_id}, "name": "${grp_config_name}"}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Subscribe Group
  [Arguments]         ${driver_node_id}       ${app_node_id}                ${group_config_name}
  POST                /api/v2/subscribe  {"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "${group_config_name}"}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Get Subscribe Group
  [Arguments]         ${node_id}
  GET                 /api/v2/subscribe?node_id=${node_id}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Unsubscribe Group
  [Arguments]         ${driver_node_id}       ${app_node_id}                ${group_config_name}
  DELETE              /api/v2/subscribe       {"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "${group_config_name}"}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Add Tags
  [Arguments]         ${node_id}              ${group}                      ${tags}
  POST                /api/v2/tags            {"node_id": ${node_id}, "group_config_name": "${group}", "tags": [${tags}]}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Add Tag And Return ID
  [Arguments]         ${node_id}              ${group}                      ${tag}
  ${res} =            Add Tags                ${node_id}                    ${group}            ${tag}
  Check Response Status                       ${res}                        200
  Check Error Code                            ${res}                        ${ERR_SUCCESS}
  ${tmp} =            evaluate                json.loads('''${tag}''')      json

  Run Keyword And Return                      Get Tag ID                    ${node_id}          ${group}        ${tmp}[name]

Get Tags
  [Arguments]         ${node_id}              ${group}
  GET                 /api/v2/tags?node_id=${node_id}&group_config_name=${group}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Update Tags
  [Arguments]         ${node_id}              ${group}                      ${tags}
  Put                 /api/v2/tags            {"node_id": ${node_id}, "group_config_name": "${group}", "tags": [${tags}]}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Del Tags
  [Arguments]         ${node_id}              ${group}                      ${ids}
  DELETE              /api/v2/tags            {"node_id": ${node_id}, "group_config_name": "${group}", "ids": [${ids}]}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Get Tag ID
  [Arguments]         ${node_id}              ${group}                      ${tag_name}
  ${res} =            Get Tags                ${node_id}                    ${group}
  Check Response Status                       ${res}                        200

  ${id} =             Tag Find By Name        ${res}[tags]                  ${tag_name}
  Should Not Be Equal As Integers             ${id}                         0

  [Return]            ${id}

Read Tags
  [Arguments]         ${node_name}              ${group}
  POST                /api/v2/read            {"node_name": "${node_name}", "group_name": "${group}"}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Write Tags
  [Arguments]         ${node_name}              ${group}                      ${tag_name}     ${value}
  POST                /api/v2/write           {"node_name": "${node_name}", "group_name": "${group}", "tag_name": "${tag_name}", "value": ${value}}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]
