*** Settings ***
Library     Process
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


${PLUGIN_MODBUS_TCP}	Modbus TCP
${PLUGIN_MONITOR}	Monitor
${PLUGIN_MQTT}    MQTT

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
${MQTT_CONFIG}                              {"client-id": "${MQTT_CLIENTID}", "upload-topic": "", "heartbeat-topic": "", "format": 1, 
...                                          "ssl": false, "host": "${MQTT_CONFIG_HOST}", "port": ${MQTT_CONFIG_PORT},
...                                          "username": "", "password": "", "ca":"", "cert":"", "key":"", "keypass":""}

${MODBUS_TCP_CONFIG}	                    {"host": "127.0.0.1", "port": 60502, "connection_mode": 0, "transport_mode": 0, "timeout": 3000, "interval": 20}

*** Variables ***
${neuron_process}

${MODBUS_PLUS_TCP_CONFIG}                   {"host": "127.0.0.1", "port": 60502, "connection_mode": 0, "transport_mode": 0, "timeout": 3000}

*** Keywords ***
Start Neuronx
    Remove Persistence
    ${process} =     Start Process     ./neuron  --log      cwd=build       stdout=neuron.log
    Set Global Variable     ${neuron_process}       ${process}
    Sleep   2
    LOGIN

Stop Neuronx
    ${result} =     Terminate Process       ${neuron_process}
    Should Be Empty     ${result.stderr} 




To Array
	[Arguments]	${str_array}
	${tmp}                      Array    ${str_array}
	[RETURN]	${tmp}[0]

Neuron Ready
    Start Neuron
    Sleep           3

List Length Should Be
    [Arguments]    ${data}    ${len}
    ${res} =                      Get Length          ${data}
    Should Be Equal As Integers   ${len}              ${res}

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

Compare Tag Value As Strings
    [Arguments]    ${tags}    ${name}    ${value}

    ${ret} =    Compare Tag Value Strings    ${tags}    ${name}    ${value}

    Should Be Equal As Integers    ${ret}    0

Check Tag Error As Int
    [Arguments]    ${tags}    ${name}    ${value}

    ${ret} =    Check Tag Error  ${tags}  ${name}  ${value}

    Should Be Equal As Integers    ${ret}    0

#Import Neuron API Resource
    #${api} =       Get Variable Value                                                    ${NEURON_API}    http
    #IF             $api.lower() not in ["mqtt", "http"]
    #Fatal Error    Neuron API should be `http` or `mqtt`, `${NEURON_API}` unsupported
    #END

    #Set Global Variable    ${NEURON_API}                                                            ${api}
    #Set Global Variable    ${NEURON_API_RESOURCE}                                                   api_${api}.resource
    #Log To Console         Neuron API is `${NEURON_API}`, load resource `${NEURON_API_RESOURCE}`
    #Import Resource        ${NEURON_API_RESOURCE}

Copy Persistence Test Data
    ${data_set} =                  Get Variable Value   ${DATASET}                       total-10k
    ${data_dir} =                  Set Variable         ${test_data_dir}/persistence/${data_set}
    Directory Should Exist         ${data_dir}

    Directory Should Exist         build
    Directory Should Not Exist     build/persistence
    ${rc} =                        Run And Return Rc    cp -r ${data_dir} build/persistence
    Should Be Equal As Integers    ${rc}                0
