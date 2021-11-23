*** Settings ***
Library           Keyword.Neuron.Neuron
Library           Keyword.Neuron.Tool
Library           Keyword.Neuron.Node
Library           Keyword.Neuron.Tag
Resource          error.robot
Resource          neuron.robot
Suite Setup       Neuron Context Ready
Suite Teardown    Stop Neuron

*** Variables ***

*** Test Cases ***
#Add different types of tags to the group, it should return success

#Add tags to the non-existent group, the group will be created automatically

#When the attribute of the tag added is incorrect, it should return partial success

#Add tags to non-existent node, it should return failure

#Delete tag from non-existent group config, it should return failure

#Delete non-existent tags, it should return success

#Query tag from non-existent node, it should return failure

#After adding the tag, query the tag, it should return the added tag

#Update tags, it should return success

#Update non-existent tags, it should return partial success

#Delete tags, it should return success

#When the address format of the tag added does not match the driver, it should return failure

#Add an existing tag(same name), it should return failure

#When adding a tag of an unsupported type in node, it should return failure

*** Keywords ***
Neuron Context Ready
	Neuron Ready

