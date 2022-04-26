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
  Should Be Equal As Strings        ${res}[uuid]                      ${MQTT_UUID}
  [RETURN]                          ${res}

Pub And Recv
  [Arguments]                       ${cmd}                            ${payload}                              ${topic_req}                ${topic_res}
  ${payload} =                      evaluate                          json.loads('''${payload}''')            json
  Set To Dictionary                 ${payload}                        uuid      ${MQTT_UUID}                  command                     ${cmd}
  ${payload} =                      evaluate                          json.dumps(${payload})                  json

  Publish                           ${topic_req}                      ${payload}                              qos=0

  # try to receive a message with a matching UUID
  ${res} =                          Wait Until Keyword Succeeds       10s                                     0s                          Recv Message           ${topic_res}
  # Should Be Equal As Strings        ${res}[command]                   ${cmd}

  # strip the `function` and `uuid` field
  Remove From Dictionary            ${res}                            uuid                                    command

  [RETURN]                          ${res}

Check Error Code
  [Arguments]                       ${result}                         ${errcode}
  Should Be Equal As Integers       ${result}[error]                  ${errcode}

Check Response Status
  [Arguments]                       ${result}                         ${status}
  Log To Console                    MQTT API does not need to check status

LOGIN
  ${token} =                        _Http LOGIN
  Add MQTT Node
  ${mqtt_node} =                    _Http Get Node ID                 ${NODE_MQTT}                            mqtt-adapter
  _Http Node Setting                ${mqtt_node}                      ${MQTT_CONFIG}
  _Http Node Ctl                    ${mqtt_node}                      ${NODE_CTL_START}
  Sleep                             1s

  Connect                           ${MQTT_CONFIG_HOST}               ${MQTT_CONFIG_PORT}                     client_id=neuron-ft         clean_session=${true}

  Subscribe                         ${TOPIC_STATUS_RES}               qos=0                                   timeout=5
  Subscribe                         ${TOPIC_NODE_RES}                 qos=0                                   timeout=5
  Subscribe                         ${TOPIC_GCONFIG_RES}              qos=0                                   timeout=5
  Subscribe                         ${TOPIC_TAGS_RES}                 qos=0                                   timeout=5
  Subscribe                         ${TOPIC_PLUGIN_RES}               qos=0                                   timeout=5
  Subscribe                         ${TOPIC_SUBSCRIBE_RES}            qos=0                                   timeout=5
  Subscribe                         ${TOPIC_READ_RES}                 qos=0                                   timeout=5
  Subscribe                         ${TOPIC_WRITE_RES}                qos=0                                   timeout=5
  Subscribe                         ${TOPIC_TTYS_RES}                 qos=0                                   timeout=5
  Subscribe                         ${TOPIC_SCHEMA_RES}               qos=0                                   timeout=5
  Subscribe                         ${TOPIC_SETTING_RES}              qos=0                                   timeout=5
  Subscribe                         ${TOPIC_CTR_RES}                  qos=0                                   timeout=5
  Subscribe                         ${TOPIC_STATE_RES}                qos=0                                   timeout=5

LOGOUT
  Unsubscribe All And Clear Messages
  Disconnect

Add MQTT Node
    _Http Add Node    type=${${NODE_MQTT}}      name=mqtt-adapter          plugin_name=mqtt

Add Node
  [Arguments]                       &{args}
  ${payload} =                      evaluate                          json.dumps(${args})                     json
  ${res} =                          Pub And Recv                      ${MQTT_CMD_ADD}                         ${payload}                 ${TOPIC_NODE_REQ}                      ${TOPIC_NODE_RES}

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
  ${res} =                          Pub And Recv                      ${MQTT_CMD_GET}                         {"node_type": ${node_type} }                      ${TOPIC_NODE_REQ}                   ${TOPIC_NODE_RES}

  [RETURN]                          ${res}

Del Node
  [Arguments]                       ${node_id}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_DELETE}                      {"id": ${node_id}}                      ${TOPIC_NODE_REQ}                   ${TOPIC_NODE_RES}

  [RETURN]                          ${res}

Get Node ID
  [Arguments]                       ${node_type}                      ${node_name}
  ${res} =                          Get Nodes                         ${node_type}
  ${node_id}                        Get Node By Name                  ${res}[nodes]                           ${node_name}

  [RETURN]                          ${node_id}

Node Setting
  [Arguments]                       ${node_id}                        ${config}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_SET}                         {"node_id": ${node_id}, "params": ${config}}                      ${TOPIC_SETTING_REQ}                   ${TOPIC_SETTING_RES} 

  [RETURN]                          ${res}

Get Node Setting
  [Arguments]                       ${node_id}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_GET}                         {"node_id": ${node_id}}                      ${TOPIC_SETTING_REQ}                   ${TOPIC_SETTING_RES}

  [RETURN]                          ${res}

Get Node State
  [Arguments]                       ${node_id}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_GET}                         {"node_id": ${node_id}}                      ${TOPIC_STATE_REQ}                   ${TOPIC_STATE_RES}

  [RETURN]                          ${res}[running]                   ${res}[link]

Node Ctl
  [Arguments]                       ${node_id}                        ${cmd}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_NONE}                        {"id": ${node_id}, "cmd": ${cmd}}                      ${TOPIC_CTR_REQ}                   ${TOPIC_CTR_RES}

  [RETURN]                          ${res}

Ping
  ${res} =                          Pub And Recv                      ${MQTT_CMD_NONE}                        {}                      ${TOPIC_PING_REQ}                   ${TOPIC_STATUS_RES}

Add Group Config
  [Arguments]                       ${node_id}                        ${grp_config_name}                      ${interval}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_ADD}                         {"name": "${grp_config_name}", "node_id": ${node_id}, "interval": ${interval}}                      ${TOPIC_GCONFIG_REQ}                   ${TOPIC_GCONFIG_RES}

  [RETURN]                          ${res}

Get Group Config
  [Arguments]                       ${node_id}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_GET}                         {"node_id": ${node_id}}                      ${TOPIC_GCONFIG_REQ}                   ${TOPIC_GCONFIG_RES}

  [RETURN]                          ${res}

Update Group Config
  [Arguments]                       ${node_id}                        ${grp_config_name}                       ${interval}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_UPDATE}                       {"name": "${grp_config_name}", "node_id": ${node_id}, "interval": ${interval}}                      ${TOPIC_GCONFIG_REQ}                   ${TOPIC_GCONFIG_RES}

  [RETURN]                          ${res}

Del Group Config
  [Arguments]                       ${node_id}                        ${grp_config_name}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_DELETE}                        {"name": "${grp_config_name}", "node_id": ${node_id}}                      ${TOPIC_GCONFIG_REQ}                   ${TOPIC_GCONFIG_RES}

  [RETURN]                          ${res}

Subscribe Group
  [Arguments]                       ${driver_node_id}                 ${app_node_id}                            ${group_config_name}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_ADD}                           {"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "${group_config_name}"}                      ${TOPIC_SUBSCRIBE_REQ}                   ${TOPIC_SUBSCRIBE_RES}

  [RETURN]                          ${res}

Get Subscribe Group
  [Arguments]                       ${node_id}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_GET}                           {"node_id": ${node_id}}                      ${TOPIC_SUBSCRIBE_REQ}                   ${TOPIC_SUBSCRIBE_RES}

  [RETURN]                          ${res}

Unsubscribe Group
  [Arguments]                       ${driver_node_id}                 ${app_node_id}                            ${group_config_name}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_DELETE}                        {"src_node_id": ${driver_node_id}, "dst_node_id": ${app_node_id}, "name": "${group_config_name}"}                      ${TOPIC_SUBSCRIBE_REQ}                   ${TOPIC_SUBSCRIBE_RES}

  [RETURN]                          ${res}

Add Tags
  [Arguments]                       ${node_id}                        ${group}                                  ${tags}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_ADD}                           {"node_id": ${node_id}, "group_config_name": "${group}", "tags": [${tags}]}                      ${TOPIC_TAGS_REQ}                   ${TOPIC_TAGS_RES}

  [RETURN]                          ${res}

Add Tag And Return ID
  [Arguments]                       ${node_id}                        ${group}                                  ${tag}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_ADD}                           {"node_id": ${node_id}, "group_config_name": "${group}", "tags": [${tag}]}                      ${TOPIC_TAGS_REQ}                   ${TOPIC_TAGS_RES}

  Check Error Code                  ${res}                            ${ERR_SUCCESS}
  ${tmp} =                          evaluate                          json.loads('''${tag}''')                  json

  Run Keyword And Return            Get Tag ID                        ${node_id}                                ${group}                  ${tmp}[name]

Get Tags
  [Arguments]                       ${node_id}                        ${group}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_GET}                           {"node_id": ${node_id}, "group_config_name": "${group}"}                      ${TOPIC_TAGS_REQ}                   ${TOPIC_TAGS_RES}

  [Return]                          ${res}

Update Tags
  [Arguments]                       ${node_id}                        ${group}                                  ${tags}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_UPDATE}                        {"node_id": ${node_id}, "group_config_name": "${group}", "tags": [${tags}]}                      ${TOPIC_TAGS_REQ}                   ${TOPIC_TAGS_RES}

  [Return]                          ${res}

Del Tags
  [Arguments]                       ${node_id}                        ${group}                                  ${ids}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_DELETE}                        {"node_id": ${node_id}, "group_config_name": "${group}", "ids": [${ids}]}                      ${TOPIC_TAGS_REQ}                   ${TOPIC_TAGS_RES}

  [Return]                          ${res}

Get Tag ID
  [Arguments]                       ${node_id}                        ${group}                                  ${tag_name}
  ${res} =                          Get Tags                          ${node_id}                                ${group}

  ${id} =                           Tag Find By Name                  ${res}[tags]                              ${tag_name}

  [Return]                          ${id}

Read Tags
  [Arguments]                       ${node_name}                        ${group}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_NONE}                          {"node_name": "${node_name}", "group_name": "${group}"}                      ${TOPIC_READ_REQ}                   ${TOPIC_READ_RES}

  [Return]                          ${res}

Write Tags
  [Arguments]                       ${node_name}                        ${group}                                  ${tag_name}       ${value}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_NONE}                          {"node_name": "${node_name}", "group_name": "${group}", "tag_name": "${tag_name}", "value": ${value}}                      ${TOPIC_WRITE_REQ}                   ${TOPIC_WRITE_RES}

  [Return]                          ${res}
