*** Settings ***
Resource          common.resource
Suite Setup       Neuron Context Ready
Suite Teardown    Stop All Processes

*** Variables ***
${MQTT_CONFIG_ERR_PORT}    ${1889}
${MQTT_CONFIG_ERR}         {"client-id": "${MQTT_CLIENTID}",
...                        "ssl": false, "host": "${MQTT_CONFIG_HOST}", "port": ${MQTT_CONFIG_ERR_PORT},
...                        "username": "", "password": "", "ca-path":"", "ca-file": ""}


*** Test Cases ***
The connection between the mqtt driver and the mqtt server is disconnected, and when reconnected, the command can be executed correctly
	Skip If Not MQTT API
	Skip If Not Http API
	Ping

	_Http LOGIN
	${mqtt_node} =        _Http Get Node ID    ${NODE_MQTT}          mqtt-adapter
	_Http Node Setting    ${mqtt_node}         ${MQTT_CONFIG_ERR}

	Sleep                 1s
	_Http Node Setting    ${mqtt_node}    ${MQTT_CONFIG}
	Sleep                 1s

	Ping

*** Keywords ***
Neuron Context Ready
    Import Neuron API Resource

    Neuron Ready

    LOGIN

Stop All Processes
    Stop Neuron                remove_persistence_data=True
    sleep                      1s
    Terminate All Processes    kill=false