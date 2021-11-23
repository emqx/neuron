*** Settings ***
Library           Keyword.Neuron.Neuron
Library           Keyword.Neuron.Tool
Library           Keyword.Neuron.Node
Library           Keyword.Neuron.GrpConfig
Library           Keyword.Neuron.Subscribe
Resource          error.robot
Resource          neuron.robot
Library           MQTTLibrary	
Library           REST                        http://127.0.0.1:7001
Suite Setup       Neuron Context Ready
Suite Teardown    Stop Neuron

*** Variables ***
${mqtt_node_name}    mqtt-adapter

*** Test Cases ***
Publish
	${mqtt_node_id}    Get Node ID    ${NODE_MQTT}	${mqtt_node_name}

	Connect    ${MQTT_CONFIG_HOST}
	Publish    topic=${MQTT_CONFIG_REQ_TOPIC}    message=test message

	[Teardown]	Disconnect

#Add group config under non-existent node, it should return failure
#Add group config under existent node, it should return success
#Delete a non-existent group config, it should return failure
#Delete a group config from non-existent node, it should return failure
#Delete a group config, it should return success
#Update a non-existent group config, it should return failure
#Update a group config from non-existent node, it should return failure
#Add a group config to the node, then get the group config from this node, it should return the group config
#Update the interval of the test group config, it should return success
#North APP subscribe non-existent group config, it should return failure
#North APP subscribe group config, it should return success
#Query the subscribers of the group config, it should return all nodes subscribed to this group
#Delete the subscribe group config, it should return failure
#North APP unsubscribe non-existent group config, it should return failure
#North APP unsubscribe group config, it should return success

*** Keywords ***
Neuron Context Ready
	Neuron Ready