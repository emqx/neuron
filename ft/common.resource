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

${NODE_UNKNOWN}	0
${NODE_DRIVER}	1
${NODE_WEB}	2
${NODE_MQTT}	3
${NODE_DRIVERX}	4
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

${PLUGIN_MODBUS_TCP}	modbus-tcp
${PLUGIN_MQTT}    mqtt

${MQTT_CMD_NONE}                             

${MQTT_CLIENTID}                            123456
${TOPIC_PING_REQ}                           neuron/${MQTT_CLIENTID}/ping
${TOPIC_READ_REQ}                           neuron/${MQTT_CLIENTID}/read/req
${TOPIC_WRITE_REQ}                          neuron/${MQTT_CLIENTID}/write/req

${TOPIC_STATUS_RES}                         neuron/${MQTT_CLIENTID}/status
${TOPIC_READ_RES}                           neuron/${MQTT_CLIENTID}/read/resp
${TOPIC_WRITE_RES}                          neuron/${MQTT_CLIENTID}/write/resp
${TOPIC_UPLOAD}                             neuron/${MQTT_CLIENTID}/upload

${MQTT_UUID}                                123456-789abc-123456-aabbcc
${MQTT_CONFIG_HOST}                         127.0.0.1
${MQTT_CONFIG_PORT}                         ${1883}
${MQTT_CONFIG}                              {"client-id": "${MQTT_CLIENTID}", "upload-topic": "", "format": 1, 
...                                          "ssl": false, "host": "${MQTT_CONFIG_HOST}", "port": ${MQTT_CONFIG_PORT},
...                                          "username": "", "password": "", "ca":"", "cert":"", "key":""}

${MODBUS_TCP_CONFIG}	                    {"host": "127.0.0.1", "port": 60502, "connection_mode": 0}

*** Keywords ***
To Array
	[Arguments]	${str_array}
	${tmp}                      Array    ${str_array}
	[RETURN]	${tmp}[0]

Neuron Ready
    Start Neuron
    Sleep           3

Compare Tag Value As Bool
    [Arguments]    ${tags}    ${name}    ${value}

    ${ret} =    Compare Tag Value Bool    ${tags}    ${name}    ${value}

    Should Be Equal As Integers    ${ret}    0

Compare Tag Value As Int
    [Arguments]    ${tags}    ${name}    ${value}

    ${ret} =    Compare Tag Value Int    ${tags}    ${name}    ${value}

    Should Be Equal As Integers    ${ret}    0

Compare Tag Value As Float
    [Arguments]    ${tags}    ${name}    ${value}

    ${ret} =    Compare Tag Value Float    ${tags}    ${name}    ${value}

    Should Be Equal As Integers    ${ret}    0

Compare Tag Value As String
    [Arguments]    ${tags}    ${name}    ${value}

    ${ret} =    Compare Tag Value String    ${tags}    ${name}    ${value}

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
    ${data_set} =                  Get Variable Value   ${DATASET}                       total-10k
    ${data_dir} =                  Set Variable         ${test_data_dir}/persistence/${data_set}
    Directory Should Exist         ${data_dir}

    Directory Should Exist         build
    Directory Should Not Exist     build/persistence
    ${rc} =                        Run And Return Rc    cp -r ${data_dir} build/persistence
    Should Be Equal As Integers    ${rc}                0
