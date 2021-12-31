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

_Http LOGOUT
  POST                              /api/v2/logout
  Integer                           response status                   200

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
  ${msg} =                          Listen                            ${MQTT_CONFIG_RES_TOPIC}                timeout=10
  Should Not Be True                $msg == None
  ${res} =                          evaluate                          json.loads('''${msg}''')                json
  Should Be Equal As Strings        ${res}[uuid]                      ${MQTT_UUID}
  [RETURN]                          ${res}

Pub And Recv
  [Arguments]                       ${func}                           ${payload}
  ${payload} =                      evaluate                          json.loads('''${payload}''')            json
  Set To Dictionary                 ${payload}                        function                                ${func}                     uuid      ${MQTT_UUID}
  ${payload} =                      evaluate                          json.dumps(${payload})                  json

  Publish                           ${MQTT_CONFIG_REQ_TOPIC}          ${payload}                              qos=0

  # try to receive a message with a matching UUID
  ${res} =                          Wait Until Keyword Succeeds       10s                                     0s                          Recv Message
  Should Be Equal As Integers       ${res}[function]                  ${func}

  # strip the `function` and `uuid` field
  Remove From Dictionary            ${res}                            function                                uuid

  [RETURN]                          ${res}

Check Error Code
  [Arguments]                       ${result}                         ${errcode}
  Should Be Equal As Integers       ${result}[error]                  ${errcode}

Check Response Status
  [Arguments]                       ${result}                         ${status}
  Log To Console                    MQTT API does not need to check status

LOGIN
  ${token} =                        _Http LOGIN
  ${mqtt_node} =                    _Http Get Node ID                 ${NODE_MQTT}                            mqtt-adapter
  _Http Node Setting                ${mqtt_node}                      ${MQTT_CONFIG}
  _Http Node Ctl                    ${mqtt_node}                      ${NODE_CTL_START}
  _Http LOGOUT
  Sleep                             1s

  Connect                           ${MQTT_CONFIG_HOST}               ${MQTT_CONFIG_PORT}                     client_id=neuron-ft         clean_session=${true}
  Subscribe                         ${MQTT_CONFIG_RES_TOPIC}          qos=0                                   timeout=5

LOGOUT
  Unsubscribe All And Clear Messages
  Disconnect

Add Node
  [Arguments]                       &{args}
  ${payload} =                      evaluate                          json.dumps(${args})                     json
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_ADD_NODES}                  ${payload}

  [RETURN]                          ${res}

Add Node And Return ID
  [Arguments]                       ${node_type}                      ${node_name}                            ${plugin_name}
  ${res} =                          Add Node                          type=${${node_type}}                    name=${node_name}           plugin_name=${plugin_name}
  Should Be Equal As Integers       ${res}[error]                     ${ERR_SUCCESS}

  ${node_id} =                      Get Node ID                       ${node_type}                            ${node_name}
  Should Not Be Equal As Integers   ${node_id}                        0

  [RETURN]                          ${node_id}

Get Nodes
  [Arguments]                       ${node_type}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_GET_NODES}                  {"node_type": ${node_type} }

  [RETURN]                          ${res}

Update Node
  [Arguments]                       ${node_id}                        ${node_name}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_UPDATE_NODES}               {"id": ${node_id}, "name": "${node_name}"}

  [RETURN]                          ${res}

Del Node
  [Arguments]                       ${node_id}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_DELETE_NODES}               {"id": ${node_id}}

  [RETURN]                          ${res}

Get Node ID
  [Arguments]                       ${node_type}                      ${node_name}
  ${res} =                          Get Nodes                         ${node_type}
  ${node_id}                        Get Node By Name                  ${res}[nodes]                           ${node_name}

  [RETURN]                          ${node_id}

Node Setting
  [Arguments]                       ${node_id}                        ${config}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_NODE_SETTING}               {"node_id": ${node_id}, "params": ${config}}

  [RETURN]                          ${res}

Get Node Setting
  [Arguments]                       ${node_id}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_GET_NODE_SETTING}           {"node_id": ${node_id}}

  [RETURN]                          ${res}

Get Node State
  [Arguments]                       ${node_id}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_GET_NODE_STATE}             {"node_id": ${node_id}}

  [RETURN]                          ${res}[running]                   ${res}[link]

Node Ctl
  [Arguments]                       ${node_id}                        ${cmd}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_NODE_CONTROL}               {"id": ${node_id}, "cmd": ${cmd}}

  [RETURN]                          ${res}

Ping
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_PING}                       {}

Add Group Config
  [Arguments]                       ${node_id}                        ${grp_config_name}                      ${interval}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_ADD_GROUP_CONFIG}           {"name": "${grp_config_name}", "node_id": ${node_id}, "interval": ${interval}}

  [RETURN]                          ${res}

Get Group Config
  [Arguments]                       ${node_id}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_GET_GROUP_CONFIG}             {"node_id": ${node_id}}

  [RETURN]                          ${res}

Update Group Config
  [Arguments]                       ${node_id}                        ${grp_config_name}                        ${interval}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_UPDATE_GROUP_CONFIG}          {"name": "${grp_config_name}", "node_id": ${node_id}, "interval": ${interval}}

  [RETURN]                          ${res}

Del Group Config
  [Arguments]                       ${node_id}                        ${grp_config_name}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_DELETE_GROUP_CONFIG}          {"name": "${grp_config_name}", "node_id": ${node_id}}

  [RETURN]                          ${res}

Subscribe Group
  [Arguments]                       ${driver_node_id}                 ${app_node_id}                            ${group_config_name}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_SUBSCRIBE_GROUP_CONFIG}       {"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "${group_config_name}"}

  [RETURN]                          ${res}

Get Subscribe Group
  [Arguments]                       ${node_id}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_GET_SUBSCRIBE_GROUP_CONFIG}   {"node_id": ${node_id}}

  [RETURN]                          ${res}

Unsubscribe Group
  [Arguments]                       ${driver_node_id}                 ${app_node_id}                            ${group_config_name}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_UNSUBSCRIBE_GROUP_CONFIG}     {"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "${group_config_name}"}

  [RETURN]                          ${res}

Add Tags
  [Arguments]                       ${node_id}                        ${group}                                  ${tags}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_ADD_TAGS}                     {"node_id": ${node_id}, "group_config_name": "${group}", "tags": [${tags}]}

  [RETURN]                          ${res}

Add Tag And Return ID
  [Arguments]                       ${node_id}                        ${group}                                  ${tag}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_ADD_TAGS}                     {"node_id": ${node_id}, "group_config_name": "${group}", "tags": [${tag}]}

  Check Error Code                  ${res}                            ${ERR_SUCCESS}
  ${tmp} =                          evaluate                          json.loads('''${tag}''')                  json

  Run Keyword And Return            Get Tag ID                        ${node_id}                                ${group}                  ${tmp}[name]

Get Tags
  [Arguments]                       ${node_id}                        ${group}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_GET_TAGS}                     {"node_id": ${node_id}, "group_config_name": "${group}"}

  [Return]                          ${res}

Update Tags
  [Arguments]                       ${node_id}                        ${group}                                  ${tags}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_UPDATE_TAGS}                  {"node_id": ${node_id}, "group_config_name": "${group}", "tags": [${tags}]}

  [Return]                          ${res}

Del Tags
  [Arguments]                       ${node_id}                        ${group}                                  ${ids}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_DELETE_TAGS}                  {"node_id": ${node_id}, "group_config_name": "${group}", "ids": [${ids}]}

  [Return]                          ${res}

Get Tag ID
  [Arguments]                       ${node_id}                        ${group}                                  ${tag_name}
  ${res} =                          Get Tags                          ${node_id}                                ${group}

  ${id} =                           Tag Find By Name                  ${res}[tags]                              ${tag_name}

  [Return]                          ${id}

Read Tags
  [Arguments]                       ${node_id}                        ${group}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_READ}                         {"node_id": ${node_id}, "group_config_name": "${group}"}

  [Return]                          ${res}

Write Tags
  [Arguments]                       ${node_id}                        ${group}                                  ${tags}
  ${res} =                          Pub And Recv                      ${MQTT_FUNC_WRITE}                        {"node_id": ${node_id}, "group_config_name": "${group}", "tags": [${tags}]}

  [Return]                          ${res}
