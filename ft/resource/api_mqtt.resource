*** Settings ***
Library    Collections
Library    Keyword.MQTT.Client      reconnect_retries=5               reconnect_delay=0
Library    REST                     http://127.0.0.1:7000
Resource   common.resource

*** Keywords ***
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

#Check Error Code
  #[Arguments]                       ${result}                         ${errcode}
  #Should Be Equal As Integers       ${result}[error]                  ${errcode}

#Check Response Status
  #[Arguments]                       ${result}                         ${status}
  #Log To Console                    MQTT API does not need to check status

#_Http Check Response Status
  #[Arguments]                       ${result}                         ${status}
  #Integer                           response status                   ${status}

#LOGIN
  #${token} =                        _Http LOGIN
  #Add MQTT Node
  #${mqtt_node} =                    _Http Get Node ID                 ${NODE_APP}                            mqtt-adapter
  #_Http Node Setting                ${mqtt_node}                      ${MQTT_CONFIG}
  #_Http Node Ctl                    ${mqtt_node}                      ${NODE_CTL_START}
  #Sleep                             1s

  #Connect                           ${MQTT_CONFIG_HOST}               ${MQTT_CONFIG_PORT}                     client_id=neuron-ft         clean_session=${true}

  #Subscribe                         ${TOPIC_STATUS_RES}               qos=0                                   timeout=5
  #Subscribe                         ${TOPIC_READ_RES}                 qos=0                                   timeout=5
  #Subscribe                         ${TOPIC_WRITE_RES}                qos=0                                   timeout=5

#LOGOUT
  #_Http LOGOUT
  #Unsubscribe All And Clear Messages
  #Disconnect

#Ping
  #${res} =                          Pub And Recv                      ${MQTT_CMD_NONE}                        {}                      ${TOPIC_PING_REQ}                   ${TOPIC_STATUS_RES}

  #[Return]                          ${res}

Read Tags
  [Arguments]                       ${node_name}                        ${group}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_NONE}                          {"node_name": "${node_name}", "group_name": "${group}"}                      ${TOPIC_READ_REQ}                   ${TOPIC_READ_RES}

  [Return]                          ${res}

Write Tags
  [Arguments]                       ${node_name}                        ${group}                                  ${tag_name}       ${value}
  ${res} =                          Pub And Recv                      ${MQTT_CMD_NONE}                          {"node_name": "${node_name}", "group_name": "${group}", "tag_name": "${tag_name}", "value": ${value}}                      ${TOPIC_WRITE_REQ}                   ${TOPIC_WRITE_RES}

  [Return]                          ${res}
