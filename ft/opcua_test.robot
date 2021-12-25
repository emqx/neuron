*** Settings ***
Resource          common.resource
Suite Setup       Neuron Context Ready 
Suite Teardown    Stop All Processes

*** Variables ***
${test_node_id}
${test_group}    config_opcua_sample

${tag_bool}         {"name": "tag_bool", "address": "1!neu.type_bool", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_BOOL}}
${tag_int8}         {"name": "tag_int8", "address": "1!neu.type_int8", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT8}}
${tag_uint8}        {"name": "tag_uint8", "address": "1!neu.type_uint8", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT8}}
${tag_int16}        {"name": "tag_int16", "address": "1!neu.type_int16", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT16}}
${tag_uint16}       {"name": "tag_uint16", "address": "1!neu.type_uint16", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT16}}
${tag_int32}        {"name": "tag_int32", "address": "1!neu.type_int32", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT32}}
${tag_uint32}       {"name": "tag_uint32", "address": "1!neu.type_uint32", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT32}}
${tag_int64}        {"name": "tag_int64", "address": "1!neu.type_int64", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_INT64}}
${tag_uint64}       {"name": "tag_uint64", "address": "1!neu.type_uint64", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_UINT64}}
${tag_float}        {"name": "tag_float", "address": "1!neu.type_float", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_FLOAT}}
${tag_double}       {"name": "tag_double", "address": "1!neu.type_double", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_DOUBLE}}
${tag_string}       {"name": "tag_string", "address": "1!neu.type_cstr", "attribute": ${TAG_ATTRIBUTE_RW}, "type": ${TAG_DATA_TYPE_STRING}}

*** Test Cases ***
Read a point with data type of bool/int8/uint8/int16/uint16/int32/uint32/int64/uint64/float/double/string, it should return success
  [Template]  Read A Point Should Success
  ${test_node_id}                     ${test_group}             bool              ${tag_bool}       true
  ${test_node_id}                     ${test_group}             int8              ${tag_int8}       -10
  ${test_node_id}                     ${test_group}             uint8             ${tag_uint8}      10
  ${test_node_id}                     ${test_group}             int16             ${tag_int16}      -1001
  ${test_node_id}                     ${test_group}             uint16            ${tag_uint16}     1001
  ${test_node_id}                     ${test_group}             int32             ${tag_int32}      -100
  ${test_node_id}                     ${test_group}             uint32            ${tag_uint32}     111111
  ${test_node_id}                     ${test_group}             int64             ${tag_int64}      -10011111111111
  ${test_node_id}                     ${test_group}             uint64            ${tag_uint64}     5678710019198784
  ${test_node_id}                     ${test_group}             float             ${tag_float}      38.9
  ${test_node_id}                     ${test_group}             double            ${tag_double}     100038.9999
  ${test_node_id}                     ${test_group}             string            ${tag_string}     hello world!

Read multiple points, including multiple data types(int8/uint8/int16/uint16/int32/uint32/int64/uint64/float/double/string), it should return success
  ${tag_id_bool} =                    Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_bool}
  ${tag_id_int8} =                    Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_int8}
  ${tag_id_uint8} =                   Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_uint8}
  ${tag_id_int16} =                   Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_int16}
  ${tag_id_uint16} =                  Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_uint16}
  ${tag_id_int32} =                   Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_int32}
  ${tag_id_uint32} =                  Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_uint32}
  ${tag_id_int64} =                   Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_int64}
  ${tag_id_uint64} =                  Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_uint64}
  ${tag_id_float} =                   Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_float}
  ${tag_id_double} =                  Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_double}
  ${tag_id_string} =                  Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_string}

  Should Not Be Equal As Integers     ${tag_id_bool}            -1
  Should Not Be Equal As Integers     ${tag_id_int8}            -1
  Should Not Be Equal As Integers     ${tag_id_uint8}           -1
  Should Not Be Equal As Integers     ${tag_id_int16}           -1
  Should Not Be Equal As Integers     ${tag_id_uint16}          -1
  Should Not Be Equal As Integers     ${tag_id_int32}           -1
  Should Not Be Equal As Integers     ${tag_id_uint32}          -1
  Should Not Be Equal As Integers     ${tag_id_int64}           -1
  Should Not Be Equal As Integers     ${tag_id_uint64}          -1
  Should Not Be Equal As Integers     ${tag_id_float}           -1
  Should Not Be Equal As Integers     ${tag_id_double}          -1
  Should Not Be Equal As Integers     ${tag_id_string}          -1

  ${res} =                            Read Tags                 ${test_node_id}   ${test_group}
  Compare Tag Value As Bool           ${res}[tags]              ${tag_id_bool}    true
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_int8}    -10
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_uint8}   10
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_int16}   -1001
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_uint16}  1001
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_int32}   -100
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_uint32}  111111
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_int64}   -10011111111111
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_uint64}  5678710019198784
  Compare Tag Value As Float          ${res}[tags]              ${tag_id_float}   38.9
  Compare Tag Value As Float          ${res}[tags]              ${tag_id_double}  100038.9999
  Compare Tag Value As String         ${res}[tags]              ${tag_id_string}  hello world!

  ${res} =                            Del Tags                  ${test_node_id}   ${test_group}     ${tag_id_bool},${tag_id_int8},${tag_id_uint8},${tag_id_int16},${tag_id_uint16},${tag_id_int32},${tag_id_uint32},${tag_id_int64},${tag_id_uint64},${tag_id_float},${tag_id_double},${tag_id_string}
  Check Response Status               ${res}                    200
  Check Error Code                    ${res}                    ${ERR_SUCCESS}

Write a point with data type of bool/int8/uint8/int16/uint16/int32/uint32/int64/uint64/float/double/string, it should return success
  [Template]  Write A Point Should Success
  ${test_node_id}                     ${test_group}             bool              ${tag_bool}       false
  ${test_node_id}                     ${test_group}             int8              ${tag_int8}       -9
  ${test_node_id}                     ${test_group}             uint8             ${tag_uint8}      66
  ${test_node_id}                     ${test_group}             int16             ${tag_int16}      -1000
  ${test_node_id}                     ${test_group}             uint16            ${tag_uint16}     999
  ${test_node_id}                     ${test_group}             int32             ${tag_int32}      99
  ${test_node_id}                     ${test_group}             uint32            ${tag_uint32}     222221
  ${test_node_id}                     ${test_group}             int64             ${tag_int64}      10011111111111
  ${test_node_id}                     ${test_group}             uint64            ${tag_uint64}     5678710019198785
  ${test_node_id}                     ${test_group}             float             ${tag_float}      12.345
  ${test_node_id}                     ${test_group}             double            ${tag_double}     100099.88
  ${test_node_id}                     ${test_group}             string            ${tag_string}     Hello Neuron!

Write multiple points, including multiple data types(int8/uint8/int16/uint16/int32/uint32/int64/uint64/float/double/string), it should return success
  ${tag_id_bool} =                    Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_bool}
  ${tag_id_int8} =                    Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_int8}
  ${tag_id_uint8} =                   Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_uint8}
  ${tag_id_int16} =                   Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_int16}
  ${tag_id_uint16} =                  Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_uint16}
  ${tag_id_int32} =                   Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_int32}
  ${tag_id_uint32} =                  Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_uint32}
  ${tag_id_int64} =                   Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_int64}
  ${tag_id_uint64} =                  Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_uint64}
  ${tag_id_float} =                   Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_float}
  ${tag_id_double} =                  Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_double}
  ${tag_id_string} =                  Add Tag And Return ID     ${test_node_id}   ${test_group}     ${tag_string}

  Should Not Be Equal As Integers     ${tag_id_int8}            -1
  Should Not Be Equal As Integers     ${tag_id_uint8}           -1
  Should Not Be Equal As Integers     ${tag_id_bool}            -1
  Should Not Be Equal As Integers     ${tag_id_int16}           -1
  Should Not Be Equal As Integers     ${tag_id_uint16}          -1
  Should Not Be Equal As Integers     ${tag_id_int32}           -1
  Should Not Be Equal As Integers     ${tag_id_uint32}          -1
  Should Not Be Equal As Integers     ${tag_id_int64}           -1
  Should Not Be Equal As Integers     ${tag_id_uint64}          -1
  Should Not Be Equal As Integers     ${tag_id_float}           -1
  Should Not Be Equal As Integers     ${tag_id_double}          -1
  Should Not Be Equal As Integers     ${tag_id_string}          -1

  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_bool}, "value": true}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_int8}, "value": 8}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_uint8}, "value": 7}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_int16}, "value": -999}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_uint16}, "value": 888}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_int32}, "value": 77}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_uint32}, "value": 123456}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_int64}, "value": -10011111111112}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_uint64}, "value": 123456789101112}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_float}, "value": 33.33}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_double}, "value": 123456.7890}
  Write Tags                          ${test_node_id}           ${test_group}     {"id": ${tag_id_string}, "value": "Hello Strangers!!"}

  ${res} =                            Read Tags                 ${test_node_id}   ${test_group}
  Compare Tag Value As Bool           ${res}[tags]              ${tag_id_bool}    true
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_int8}    8
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_uint8}   7
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_int16}   -999
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_uint16}  888
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_int32}   77
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_uint32}  123456
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_int64}   -10011111111112
  Compare Tag Value As Int            ${res}[tags]              ${tag_id_uint64}  123456789101112
  Compare Tag Value As Float          ${res}[tags]              ${tag_id_float}   33.33
  Compare Tag Value As Float          ${res}[tags]              ${tag_id_double}  123456.7890
  Compare Tag Value As String         ${res}[tags]              ${tag_id_string}  Hello Strangers!!

  ${res} =                            Del Tags                  ${test_node_id}   ${test_group}     ${tag_id_bool},${tag_id_int8},${tag_id_uint8},${tag_id_int16},${tag_id_uint16},${tag_id_int32},${tag_id_uint32},${tag_id_int64},${tag_id_uint64},${tag_id_float},${tag_id_double},${tag_id_string}
  Check Response Status               ${res}                    200
  Check Error Code                    ${res}                    ${ERR_SUCCESS}

*** Keywords ***
Neuron Context Ready
  Import Neuron API Resource

  Start Simulator                     ${OPCUA_SERVER_SIMULATOR}                   ${SIMULATOR_DIR}

  Neuron Ready
  LOGIN

  ${id} =                             Get Node ID               ${NODE_DRIVER}     opcua-adapter
  Set Global Variable                 ${test_node_id}           ${id}
  Node Setting                        ${test_node_id}           ${OPCUA_CONFIG}

Stop All Processes
  LOGOUT
  Stop Neuron
  Sleep                               1s
  Terminate All Processes             kill=false

Read A Point Should Success
  [Arguments]                         ${node_id}                ${group}            ${type}         ${tag}        ${value}
  Log                                 Read a point as ${type}, it should return success
  Log To Console                      Read a point as ${type}, it should return success

  IF                                  "${type}" == "float" or "${type}" == "double"
    ${cmp} =                          Set Variable  Compare Tag Value As Float
  ELSE IF                             "${type}" == "string"
    ${cmp} =                          Set Variable  Compare Tag Value As String
  ELSE IF                             "${type}" == "bool"
    ${cmp} =                          Set Variable  Compare Tag Value As Bool
  ELSE
    ${cmp} =                          Set Variable  Compare Tag Value As Int
  END

  ${tag_id} =                         Add Tag And Return ID     ${node_id}          ${group}        ${tag}
  Should Not Be Equal As Integers     ${tag_id}                 -1

  ${res} =                            Read Tags                 ${node_id}          ${group}
  Run Keyword                         ${cmp}                    ${res}[tags]        ${tag_id}       ${value}

  ${res} =                            Del Tags                  ${node_id}          ${group}        ${tag_id}
  Check Response Status               ${res}                    200
  Check Error Code                    ${res}                    ${ERR_SUCCESS}

Write A Point Should Success
  [Arguments]                         ${node_id}                ${group}            ${type}         ${tag}        ${value}
  Log                                 Write a point as ${type}, it should return success
  Log To Console                      Write a point as ${type}, it should return success

  ${val} =                            Set Variable  ${value}
  IF                                  "${type}" == "float" or "${type}" == "double"
    ${cmp} =                          Set Variable  Compare Tag Value As Float
  ELSE IF                             "${type}" == "string"
    ${val} =                          Set Variable  "${value}"
    ${cmp} =                          Set Variable  Compare Tag Value As String
  ELSE IF                             "${type}" == "bool"
    ${cmp} =                          Set Variable  Compare Tag Value As Bool
  ELSE
    ${cmp} =                          Set Variable  Compare Tag Value As Int
  END

  ${tag_id} =                         Add Tag And Return ID     ${node_id}          ${group}        ${tag}
  Should Not Be Equal As Integers     ${tag_id}                 -1

  ${res} =                            Write Tags                ${node_id}          ${group}        {"id": ${tag_id}, "value": ${val}}

  ${res} =                            Read Tags                 ${node_id}          ${group}
  Run Keyword                         ${cmp}                    ${res}[tags]        ${tag_id}       ${value}

  ${res} =                            Del Tags                  ${node_id}          ${group}        ${tag_id}
  Check Response Status               ${res}                    200
  Check Error Code                    ${res}                    ${ERR_SUCCESS}
