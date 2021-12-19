*** Settings ***
Library           Keyword.Neuron.GrpConfig
Library           Keyword.Neuron.Neuron
Library           Keyword.Neuron.Node
Library           Keyword.Neuron.Subscribe
Library           Keyword.Neuron.Read
Library           Keyword.Neuron.Tag
Library           Keyword.Neuron.Tool

Resource          error.resource
Resource          simulator.resource

*** Variables ***
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

${MQTT_CONFIG_HOST}    broker.fengzero.com
${MQTT_CONFIG_REQ_TOPIC}	test/mqtt_req
${MQTT_CONFIG_RES_TOPIC}	test/mqtt_res
${MQTT_CONFIG}    {"req-topic": "${MQTT_CONFIG_REQ_TOPIC}", "res-topic": "${MQTT_CONFIG_RES_TOPIC}", "ssl": false, "host": "${MQTT_CONFIG_HOST}", "port": 1883, "username": "", "password": "", "ca-path":"", "ca-file": ""}

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
  [Arguments]         ${tags}                 ${id}                         ${value}

  ${ret} =            Compare Tag Value Bool  ${tags}                       ${id}         ${value}

  Should Be Equal As Integers                 ${ret}                        0

Compare Tag Value As Int
  [Arguments]         ${tags}                 ${id}                         ${value}

  ${ret} =            Compare Tag Value Int   ${tags}                       ${id}         ${value}

  Should Be Equal As Integers                 ${ret}                        0

Compare Tag Value As Float
  [Arguments]         ${tags}                 ${id}                         ${value}

  ${ret} =            Compare Tag Value Float   ${tags}                     ${id}         ${value}

  Should Be Equal As Integers                 ${ret}                        0

Compare Tag Value As String
  [Arguments]         ${tags}                 ${id}                         ${value}

  ${ret} =            Compare Tag Value String   ${tags}                    ${id}         ${value}

  Should Be Equal As Integers                 ${ret}                        0
