*** Settings ***
Library           Keyword.Neuron.Neuron
Library           Keyword.Neuron.Node
Resource          error.robot
Resource          neuron.robot
Suite Setup       Neuron Ready
Suite Teardown    Stop Neuron

*** Variables ***

*** Test Cases ***
#Use the wrong body to crate a node, it should return failure
#
#Use a plugin name that does not exist to create a node, it should return failure
#
#Use a invalid node type to create a node, it should return failure
#
#Create a node with the correct body, it should return success
#
#Create an existing node, it should return failure
#
#Get node with wrong parameters, it will return failure
#
#Get DRIVER node, it should return all DRIVER node
#
#Get WEB node, it should return all WEB node
#
#Get MQTT node, it should return all mqtt node
#
#Get STREAM PROCESSOR node, it should return all STREAM PROCESSOR node
#
#Get APP node, it should return all APP node
#
#Get UNKNOWN type node, it should return empty node
#
#Delete the existing node, it will return success
#
#Delete a non-existent node, it will return failure
#
#Add setting to non-existent node, it should return failure
#
#Get setting from non-existent node, it should return failure
#
#Get setting from a node that has never been set, it should return failure
#
#Add the correct settings to the node, it should return success
#
#Get the setting of node, it should return to the previous setting
#
#Add wrong settings to node, it should return failure
#
#Update a node that does not exist, it should return failure
#
#Update the name of the node, it should return success

#Start an unconfigured node, it should return failure

#Start the configured node, it should return success

#Start the stopped node, it should return success

#Start the running node, it should restart

#Stop node that is not running, it should return failure

#Stop the running node, it should return success

#When setting up an IDLE node, the node status should become READY

#When setting up a READY/RUNNING/STOPED node, the node status will not change

*** Keywords ***
Neuron Context Ready
	Neuron Ready