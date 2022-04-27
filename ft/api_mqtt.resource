*** Settings ***
Library    Collections
Library    Keyword.MQTT.Client      reconnect_retries=5               reconnect_delay=0
Library    REST                     http://127.0.0.1:7001
Resource   common.resource

*** Keywords ***

_Http LOGIN
  POST                              /api/v2/login                     {"name": "admin", "pass":"0000"}
  Integer                           response status                   200

  ${token}                          String                            $.token
  ${jwt} =                          Catenate                          Bearer                                  ${token}[0]
  Set Headers                       {"Authorization":"${jwt}"}

_Http Add Node
  [Arguments]         &{args}
  POST                /api/v2/node            ${args}

  ${res} =            Object  response body
  [RETURN]            ${res}[0]


_Http Get Node ID
  [Arguments]                       ${node_type}                      ${node_name}
  GET                               /api/v2/node?type=${node_type}
  Integer                           response status                   200
  ${tmp}                            Array                             $.nodes
  ${node_id}                        Get Node By Name                  ${tmp}[0]                               ${node_name}
  Should Not Be Equal As Integers   ${node_id}                        0
  [RETURN]                          ${node_id}

_Http Node Setting
  [Arguments]                       ${node_id}                        ${config}
  POST                              /api/v2/node/setting              {"node_id": ${node_id}, "params": ${config}}
  Integer                           response status                   200

_Http Node Ctl
  [Arguments]                       ${node_id}                        ${cmd}
  POST                              /api/v2/node/ctl                  {"id": ${node_id}, "cmd": ${cmd}}
  Integer                           response status                   200

Recv Message
  [Arguments]                       ${topic_res}
  ${msg} =                          Listen                            ${topic_res}                            timeout=10
  Should Not Be True                $msg == None
  ${res} =                          evaluate                          json.loads('''${msg}''')                json

  [RETURN]                          ${res}

Pub And Recv
  [Arguments]                       ${cmd}                            ${payload}                              ${topic_req}                ${topic_res}
  ${payload} =                      evaluate                          json.loads('''${payload}''')            json
  Set To Dictionary                 ${payload}                        uuid      ${MQTT_UUID}                  command                     ${cmd}
  ${payload} =                      evaluate                          json.dumps(${payload})                  json

  Publish                           ${topic_req}                      ${payload}                              qos=0

  # try to receive a message with a matching UUID
  ${res} =                          Wait Until Keyword Succeeds       10s                                     0s                          Recv Message           ${topic_res}
  Should Be Equal As Strings        ${res}[uuid]                      ${MQTT_UUID}

  # strip the `function` and `uuid` field
  Remove From Dictionary            ${res}                            uuid                                    command

  [RETURN]                          ${res}

Check Error Code
  [Arguments]                       ${result}                         ${errcode}
  Should Be Equal As Integers       ${result}[error]                  ${errcode}

Check Response Status
  [Arguments]                       ${result}                         ${status}
  Log To Console                    MQTT API does not need to check status

_Http Check Response Status
  [Arguments]                       ${result}                         ${status}
  Integer                           response status                   ${status}

LOGIN
  ${token} =                        _Http LOGIN
  Add MQTT Node
  ${mqtt_node} =                    _Http Get Node ID                 ${NODE_MQTT}                            mqtt-adapter
  _Http Node Setting                ${mqtt_node}                      ${MQTT_CONFIG}
  _Http Node Ctl                    ${mqtt_node}                      ${NODE_CTL_START}
  Sleep                             1s

  Connect                           ${MQTT_CONFIG_HOST}               ${MQTT_CONFIG_PORT}                     client_id=neuron-ft         clean_session=${true}

  Subscribe                         ${TOPIC_STATUS_RES}               qos=0                                   timeout=5
  Subscribe                         ${TOPIC_READ_RES}                 qos=0                                   timeout=5
  Subscribe                         ${TOPIC_WRITE_RES}                qos=0                                   timeout=5

LOGOUT
  _Http LOGOUT
  Unsubscribe All And Clear Messages
  Disconnect

Add MQTT Node
    _Http Add Node    type=${${NODE_MQTT}}      name=mqtt-adapter          plugin_name=mqtt

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

Get Node ID
  [Arguments]                       ${node_type}                      ${node_name}
  GET                               /api/v2/node?type=${node_type}
  Integer                           response status                   200
  ${tmp}                            Array                             $.nodes
  ${node_id}                        Get Node By Name                  ${tmp}[0]                               ${node_name}
  Should Not Be Equal As Integers   ${node_id}                        0
  [RETURN]                          ${node_id}

Node Setting
  [Arguments]                       ${node_id}                        ${config}
  POST                              /api/v2/node/setting              {"node_id": ${node_id}, "params": ${config}}
  Integer                           response status                   200

Node Ctl
  [Arguments]         ${node_id}  ${cmd}
  POST                /api/v2/node/ctl        {"id": ${node_id}, "cmd": ${cmd}}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Add Group Config
  [Arguments]         ${node_id}              ${grp_config_name}            ${interval}
  POST                /api/v2/gconfig         {"name": "${grp_config_name}", "node_id": ${node_id}, "interval": ${interval}}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Update Group Config
  [Arguments]         ${node_id}              ${grp_config_name}            ${interval}
  PUT                 /api/v2/gconfig         {"node_id": ${node_id}, "name": "${grp_config_name}", "interval": ${interval}}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Subscribe Group
  [Arguments]         ${driver_node_id}       ${app_node_id}                ${group_config_name}
  POST                /api/v2/subscribe  {"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "${group_config_name}"}
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

Add Node And Return ID
  [Arguments]         ${node_type}            ${node_name}                  ${plugin_name}
  ${res} =            Add Node          type=${${node_type}}          name=${node_name}             plugin_name=${plugin_name}

  Log   ${res}
  _Http Check Response Status                       ${res}                        200
  Check Error Code                                  ${res}                        ${ERR_SUCCESS}

  ${node_id} =        Get Node ID             ${node_type}                  ${node_name}
  Should Not Be Equal As Integers                   ${node_id}                    0

  [RETURN]            ${node_id}

Ping
  ${res} =                          Pub And Recv                      ${MQTT_CMD_NONE}                        {}                      ${TOPIC_PING_REQ}                   ${TOPIC_STATUS_RES}

  [Return]                          ${res}

Read Tags
  [Arguments]                       ${node_name}                        ${group}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_NONE}                          {"node_name": "${node_name}", "group_name": "${group}"}                      ${TOPIC_READ_REQ}                   ${TOPIC_READ_RES}

  [Return]                          ${res}

Write Tags
  [Arguments]                       ${node_name}                        ${group}                                  ${tag_name}       ${value}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_NONE}                          {"node_name": "${node_name}", "group_name": "${group}", "tag_name": "${tag_name}", "value": ${value}}                      ${TOPIC_WRITE_REQ}                   ${TOPIC_WRITE_RES}

  [Return]                          ${res}
