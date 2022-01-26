*** Settings ***
Library    OperatingSystem

Library    Keyword.Neuron.GrpConfig
Library    Keyword.Neuron.Neuron
Library    Keyword.Neuron.Node
Library    Keyword.Neuron.Subscribe
Library    Keyword.Neuron.Read
Library    Keyword.Neuron.Tag
Library    Keyword.Neuron.Tool

Resource    error.resource
Resource    simulator.resource

*** Variables ***
${test_data_dir}     ft/data
${test_pdata_dir}    ${test_data_dir}/persistence

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

${MQTT_FUNC_LOGIN}                         ${1}
${MQTT_FUNC_LOGOUT}                        ${2}
${MQTT_FUNC_READ}                          ${3}
${MQTT_FUNC_WRITE}                         ${4}
${MQTT_FUNC_GET_TTY}                       ${5}
${MQTT_FUNC_ADD_GROUP_CONFIG}              ${6}
${MQTT_FUNC_UPDATE_GROUP_CONFIG}           ${7}
${MQTT_FUNC_DELETE_GROUP_CONFIG}           ${8}
${MQTT_FUNC_GET_GROUP_CONFIG}              ${9}
${MQTT_FUNC_SUBSCRIBE_GROUP_CONFIG}        ${10}
${MQTT_FUNC_UNSUBSCRIBE_GROUP_CONFIG}      ${11}
${MQTT_FUNC_GET_SUBSCRIBE_GROUP_CONFIG}    ${12}
${MQTT_FUNC_ADD_TAGS}                      ${13}
${MQTT_FUNC_UPDATE_TAGS}                   ${14}
${MQTT_FUNC_DELETE_TAGS}                   ${15}
${MQTT_FUNC_GET_TAGS}                      ${16}
${MQTT_FUNC_ADD_NODES}                     ${17}
${MQTT_FUNC_UPDATE_NODES}                  ${18}
${MQTT_FUNC_DELETE_NODES}                  ${19}
${MQTT_FUNC_GET_NODES}                     ${20}
${MQTT_FUNC_NODE_SETTING}                  ${21}
${MQTT_FUNC_GET_NODE_SETTING}              ${22}
${MQTT_FUNC_NODE_CONTROL}                  ${23}
${MQTT_FUNC_GET_NODE_STATE}                ${24}
${MQTT_FUNC_ADD_PLUGIN}                    ${25}
${MQTT_FUNC_UPDATE_PLUGIN}                 ${26}
${MQTT_FUNC_DELETE_PLUGIN}                 ${27}
${MQTT_FUNC_GET_PLUGIN}                    ${28}
${MQTT_FUNC_PING}                          ${29}

${MQTT_UUID}                123456-789abc-123456-aabbcc
${MQTT_CONFIG_HOST}         127.0.0.1
${MQTT_CONFIG_PORT}         ${1883}
${MQTT_CONFIG_REQ_TOPIC}    neuronlite/request
${MQTT_CONFIG_RES_TOPIC}    neuronlite/response
${MQTT_CONFIG}              {"req-topic": "${MQTT_CONFIG_REQ_TOPIC}",
...                         "res-topic": "${MQTT_CONFIG_RES_TOPIC}",
...                         "ssl": false, "host": "${MQTT_CONFIG_HOST}", "port": ${MQTT_CONFIG_PORT},
...                         "username": "", "password": "", "ca-path":"", "ca-file": ""}

${MODBUS_TCP_CONFIG}	{"host": "127.0.0.1", "port": 60502, "connection_mode": 0}
${OPCUA_CONFIG}    {"host": "127.0.0.1", "port": 4840, "username": "user", "password": "123456"}

*** Keywords ***
To Array
	[Arguments]	${str_array}
	${tmp}                      Array    ${str_array}
	[RETURN]	${tmp}[0]

Neuron Ready
    Start Neuron
    Sleep           3

Compare Tag Value As Bool
    [Arguments]    ${tags}    ${id}    ${value}

    ${ret} =    Compare Tag Value Bool    ${tags}    ${id}    ${value}

    Should Be Equal As Integers    ${ret}    0

Compare Tag Value As Int
    [Arguments]    ${tags}    ${id}    ${value}

    ${ret} =    Compare Tag Value Int    ${tags}    ${id}    ${value}

    Should Be Equal As Integers    ${ret}    0

Compare Tag Value As Float
    [Arguments]    ${tags}    ${id}    ${value}

    ${ret} =    Compare Tag Value Float    ${tags}    ${id}    ${value}

    Should Be Equal As Integers    ${ret}    0

Compare Tag Value As String
    [Arguments]    ${tags}    ${id}    ${value}

    ${ret} =    Compare Tag Value String    ${tags}    ${id}    ${value}

    Should Be Equal As Integers    ${ret}    0

Import Neuron API Resource
    ${api} =       Get Variable Value                                                    ${NEURON_API}    http
    IF             $api.lower() not in ["mqtt", "http"]
    Fatal Error    Neuron API should be `http` or `mqtt`, `${NEURON_API}` unsupported
    END

    Set Global Variable    ${NEURON_API}                                                            ${api}
    Set Global Variable    ${NEURON_API_RESOURCE}                                                   api_${api}.resource
    Log To Console         Neuron API is `${NEURON_API}`, load resource `${NEURON_API_RESOURCE}`
    Import Resource        ${NEURON_API_RESOURCE}

Skip If Not Http API
    Skip If    $NEURON_API != "http"    Current API is not `http`, just skip

Skip If Not MQTT API
    Skip If    $NEURON_API != "mqtt"    Current API is not `mqtt`, just skip

Copy Persistence Test Data
    Directory Should Exist         build
    ${rc} =                        Run And Return Rc    cp -r ${test_pdata_dir} build/
    Should Be Equal As Integers    ${rc}                0
