*** Settings ***
Library    REST                     http://127.0.0.1:7000
Resource   common.resource

*** Variables ***
${PLUGIN-MODBUS-TCP}    Modbus TCP
${PLUGIN-MQTT}          MQTT
${PLUGIN-EKUIPER}       eKuiper

${NODE_DRIVER}	1
${NODE_APP}	2

${NODE_STATE_INIT}	1
${NODE_STATE_READY}	2
${NODE_STATE_RUNNING}	3
${NODE_STATE_STOP}	4

${NODE_LINK_STATE_DISCONNECTED}    0
${NODE_LINK_STATE_CONNECTED}       1

${NODE_CTL_START}	0
${NODE_CTL_STOP}	1

${TAG_ATTRIBUTE_READ}	1
${TAG_ATTRIBUTE_WRITE}	2
${TAG_ATTRIBUTE_SUBSCRIBE}	4
${TAG_ATTRIBUTE_STATIC}	8
${TAG_ATTRIBUTE_RW}	3
${TAG_ATTRIBUTE_STATIC_RW}	11

${TAG_TYPE_INT8}	  1
${TAG_TYPE_UINT8}	  2
${TAG_TYPE_INT16}	  3
${TAG_TYPE_UINT16}	4
${TAG_TYPE_INT32}	  5
${TAG_TYPE_UINT32}	6
${TAG_TYPE_INT64}	  7
${TAG_TYPE_UINT64}	8
${TAG_TYPE_FLOAT}	  9
${TAG_TYPE_DOUBLE}	10
${TAG_TYPE_BIT}	    11
${TAG_TYPE_BOOL}	  12
${TAG_TYPE_STRING}	13
${TAG_TYPE_BYTE}	  14

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

  [Return]            ${token}[0]

Add Node
  [Arguments]   ${name}   ${plugin}
  POST          /api/v2/node  {"name": "${name}", "plugin": "${plugin}"}
  ${res}        Object  response body
  [RETURN]      ${res}[0]

Update Node
  [Arguments]   ${name}   ${new_name}
  PUT           /api/v2/node  {"name": "${name}", "new_name": "${new_name}"}
  ${res}        Object  response body
  [RETURN]      ${res}[0]

Del Node
  [Arguments]   ${name}
  DELETE        /api/v2/node  {"name": "${name}"}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Get Nodes
  [Arguments]         ${node_type}
  GET                 /api/v2/node?type=${node_type}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Node Setting
  [Arguments]         ${name}              ${config}
  POST                /api/v2/node/setting    {"node": "${name}", "params": ${config}}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Get Node Setting
  [Arguments]         ${name}
  GET                 /api/v2/node/setting?node=${name}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Get Node State
  [Arguments]         ${name}
  GET                 /api/v2/node/state?node=${name}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Node Ctl
  [Arguments]         ${node}  ${cmd}
  POST                /api/v2/node/ctl        {"node": "${node}", "cmd": ${cmd}}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Add Group
  [Arguments]         ${node}              ${group}            ${interval}
  POST                /api/v2/group         {"group": "${group}", "node": "${node}", "interval": ${interval}}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Update Group
  [Arguments]         ${node}              ${group}            &{updates}
  ${dict} =           Evaluate             {"node":$node, "group":$group, **$updates}
  ${data} =           Evaluate             json.dumps(${dict})            json
  PUT                 /api/v2/group        ${data}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Del Group
  [Arguments]         ${node}              ${group}
  DELETE              /api/v2/group         {"node": "${node}", "group": "${group}"}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Get Group
  [Arguments]         ${node}
  GET                 /api/v2/group?node=${node}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Subscribe Group
  [Arguments]         ${app}       ${driver}                ${group}
  POST                /api/v2/subscribe  {"app": "${app}", "driver": "${driver}", "group": "${group}"}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Unsubscribe Group
  [Arguments]         ${app}       ${driver}                ${group}
  DELETE              /api/v2/subscribe       {"app": "${app}", "driver": "${driver}", "group": "${group}"}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Update Subscribe
  [Arguments]         ${app}       ${driver}                ${group}            &{params}
  ${data} =           Evaluate     json.dumps(${params})    json
  PUT                 /api/v2/subscribe   {"app": "${app}", "driver": "${driver}", "group": "${group}", "params": ${data}}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Subscribe Groups
  [Arguments]         ${app}       @{groups}
  ${data} =           Evaluate     json.dumps(${groups})    json
  POST                /api/v2/subscribes  {"app": "${app}", "groups": ${data}}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Get Subscribe Group
  [Arguments]         ${app}
  GET                 /api/v2/subscribe?app=${app}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Add Tags
  [Arguments]         ${node}   ${group}    ${tags}
  POST                /api/v2/tags            {"node": "${node}", "group": "${group}", "tags": [${tags}]}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Get Tags
  [Arguments]         ${node}              ${group}
  GET                 /api/v2/tags?node=${node}&group=${group}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Update Tags
  [Arguments]         ${node}   ${group}  ${tags}
  Put                 /api/v2/tags            {"node": "${node}", "group": "${group}", "tags": [${tags}]}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Del Tags
  [Arguments]         ${node}              ${group}                      ${tags}
  DELETE              /api/v2/tags            {"node": "${node}", "group": "${group}", "tags": [${tags}]}
  ${res} =            Object  response body

  [RETURN]            ${res}[0]

Read Tags
  [Arguments]         ${node}              ${group}
  POST                /api/v2/read            {"node": "${node}", "group": "${group}"}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Write Tags
  [Arguments]         ${node}              ${group}                      ${tag}     ${value}
  POST                /api/v2/write           {"node": "${node}", "group": "${group}", "tag": "${tag}", "value": ${value}}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Write 3 Tags 1 Time
  [Arguments]         ${node}    ${group}    ${tag1}     ${value1}    ${tag2}     ${value2}    ${tag3}     ${value3}
  POST                /api/v2/write/tags           {"node": "${node}", "group": "${group}", "tags": [{"tag": "${tag1}", "value": ${value1}}, {"tag": "${tag2}", "value": ${value2}}, {"tag": "${tag3}", "value": ${value3}}]}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Add Template
  [Arguments]   ${name}   ${plugin}   ${groups}
  ${data} =     Evaluate              json.dumps(${groups})   json
  POST          /api/v2/template      {"name": "${name}", "plugin": "${plugin}", "groups": ${data} }
  ${res}        Object  response body
  [RETURN]      ${res}[0]

Del Template
  [Arguments]   ${name}
  DELETE        /api/v2/template?name=${name}
  ${res}        Object  response body
  [RETURN]      ${res}[0]

Inst Template
  [Arguments]   ${name}   ${node}
  POST          /api/v2/template/inst      {"name": "${name}", "node": "${node}" }
  ${res}        Object  response body
  [RETURN]      ${res}[0]

Inst Templates
  [Arguments]   @{insts}
  ${data} =     Evaluate                   json.dumps(${insts})          json
  POST          /api/v2/template/instances  {"nodes": ${data}}
  ${res}        Object  response body
  [RETURN]      ${res}[0]

Get Template
  [Arguments]   ${name}
  GET           /api/v2/template?name=${name}
  ${res}        Object  response body
  [RETURN]      ${res}[0]

Get Templates
  GET           /api/v2/template
  ${res}        Object  response body
  [RETURN]      ${res}[0]

Add Template Group
  [Arguments]         ${tmpl}                 ${group}            ${interval}
  POST                /api/v2/template/group  {"template": "${tmpl}", "group": "${group}", "interval": ${interval}}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Put Template Group
  [Arguments]         ${tmpl}                 ${group}            &{updates}
  ${data} =           Evaluate                json.dumps({"template": "${tmpl}", "group": "${group}", **$updates})     json
  PUT                 /api/v2/template/group  ${data}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Del Template Group
  [Arguments]         ${tmpl}                 ${group}
  DELETE              /api/v2/template/group  {"template": "${tmpl}", "group": "${group}"}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Get Template Groups
  [Arguments]         ${tmpl}
  GET                 /api/v2/template/group?name=${tmpl}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Add Template Tags
  [Arguments]         ${tmpl}                 ${group}            @{tags}
  ${data} =           Evaluate                json.dumps(${tags})             json
  POST                /api/v2/template/tag    {"template": "${tmpl}", "group": "${group}", "tags": ${data}}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Get Template Tags
  [Arguments]         ${tmpl}                 ${group}
  GET                 /api/v2/template/tag?template=${tmpl}&group=${group}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Grep Template Tags
  [Arguments]         ${tmpl}                 ${group}            ${pattern}
  GET                 /api/v2/template/tag?template=${tmpl}&group=${group}&name=${pattern}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Put Template Tags
  [Arguments]         ${tmpl}                 ${group}            @{tags}
  ${data} =           Evaluate                json.dumps(${tags})             json
  PUT                 /api/v2/template/tag    {"template": "${tmpl}", "group": "${group}", "tags": ${data}}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Del Template Tags
  [Arguments]         ${tmpl}                 ${group}            @{tags}
  ${data} =           Evaluate                json.dumps(${tags})             json
  DELETE              /api/v2/template/tag    {"template": "${tmpl}", "group": "${group}", "tags": ${data}}
  ${res} =            Object  response body
  [RETURN]            ${res}[0]

Get all metrics
  GET                 /api/v2/metrics
  ${res} =            String  response body
  [RETURN]            ${res}

Get single node metrics
  [Arguments]         ${node_name}            ${category}
  GET                 /api/v2/metrics?node=${node_name}&category=${category}
  ${res} =            String  response body
  [RETURN]            ${res}

Get metrics lines
  ${lines}=    Get all metrics
  ${lines_str}=    Convert To String  ${lines}
  ${lines_str}   Evaluate       str(${lines_str}).strip('[]')
  ${lines_str}   Evaluate       str(${lines_str}).strip(\"'\")
  ${nlines}=    Split String   ${lines_str}    \n
  Remove From List   ${nlines}   -1
  [RETURN]            ${nlines}

Get parameter lines
  [Arguments]         ${nlines}
  ${description_lines}    Create List
  ${parameter_lines}      Create List
  FOR    ${line}    IN    @{nlines}
      ${first_char}=    Get Substring    ${line}    0    1
      IF    "${first_char}"=="#"
          Append To List   ${description_lines}   ${line}
      ELSE
          Append To List   ${parameter_lines}     ${line}
      END
  END
  [RETURN]            ${parameter_lines}

Check endswith isdigit
  [Arguments]         ${parameter_lines}
  FOR    ${line}    IN    @{parameter_lines}
      ${last_char}=   Get Substring   ${line}   -1
      Should Be True   '${last_char}'.isdigit()
  END

Check node state
  [Arguments]         ${parameter_lines}    ${node}
  ${link_state}    Create List
  ${running_state}    Create List
  ${node_state}=    Get Node State    ${node}
  
  FOR    ${line}    IN    @{parameter_lines}
      ${param}=    Get Substring    ${line}    0    10
      IF    "${param}"=="link_state"
          Append To List   ${link_state}    ${line}
      END
      IF    "${param}"=="running_st"
          Append To List   ${running_state}    ${line}
      END
  END
  
  ${metrics_link}=   Get Substring   ${link_state}[0]   -1
  ${metrics_link}=    Convert To Integer    ${metrics_link}
  Should Be Equal As Integers    ${metrics_link}    ${node_state}[link]

  ${metrics_running}=   Get Substring   ${running_state}[0]   -1
  ${metrics_running}=    Convert To Integer    ${metrics_running}
  Should Be Equal As Integers    ${metrics_running}    ${node_state}[running]