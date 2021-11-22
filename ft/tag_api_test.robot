*** Settings ***
Library           Keyword.Neuron.Neuron
Library           Keyword.Neuron.Tool
Library           Keyword.Neuron.Node
Library           Keyword.Neuron.Tag
Resource          error.robot
Resource          neuron.robot
Library           REST                     http://127.0.0.1:7001
Suite Setup       Neuron Context Ready
Suite Teardown    Stop Neuron

*** Variables ***
${test_node}	test-node
${test_gconfig}    test_grp_config

*** Test Cases ***
Add different types of tags to the group, it should return success
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	should not be equal as integers    ${test_node_id}    0

	Add Group Config    ${test_node_id}	${test_gconfig}

	POST	/api/v2/tags	{"node_id": ${test_node_id}, "group_config_name": "${test_gconfig}", "tags": [{"name": "tag1", "address": "1!400001", "attribute": 1, "type": 4},{"name": "tag2", "address": "1!00001", "attribute": 3, "type": 14}]}

	Integer	response status    200

	Del Group Config    ${test_node_id}	${test_gconfig}

Add tags to the non-existent group, the group will be created automatically
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	should not be equal as integers    ${test_node_id}    0

	POST	/api/v2/tags	{"node_id": ${test_node_id}, "group_config_name": "${test_gconfig}", "tags": [{"name": "tag1", "address": "1!400001", "attribute": 1, "type": 4},{"name": "tag2", "address": "1!00001", "attribute": 3, "type": 14}]}

	Integer	response status    200
	Del Group Config           ${test_node_id}	${test_gconfig}

When the attribute of the tag added is incorrect, it should return partial success
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	should not be equal as integers    ${test_node_id}    0

	POST	/api/v2/tags	{"node_id": ${test_node_id}, "group_config_name": "${test_gconfig}", "tags": [{"name": "tag1", "address": "1!400001", "attribute": 8, "type": 4},{"name": "tag2", "address": "1!00001", "attribute": 3, "type": 14}]}

	Integer	response status    206
	Integer                    response body error    ${ERR_TAG_ATTRIBUTE_NOT_SUPPORT}

	Del Group Config    ${test_node_id}	${test_gconfig}

Add tags to non-existent node, it should return failure
	POST	/api/v2/tags	{"node_id": 999, "group_config_name": "${test_gconfig}", "tags": [{"name": "tag1", "address": "1!400001", "attribute": 1, "type": 4},{"name": "tag2", "address": "1!00001", "attribute": 3, "type": 14}]}

	Integer	response status    404
	Integer                    response body error    ${ERR_NODE_NOT_EXIST}

Delete tag from non-existent group config, it should return failure
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	should not be equal as integers    ${test_node_id}    0

	DELETE	/api/v2/tags	{"node_id": ${test_node_id}, "group_config_name": "${test_gconfig}", "ids": [1, 2, 3]}

	Integer	response status    404
	Integer                    response body error    ${ERR_NODE_NOT_EXIST}

Delete non-existent tags, it should return success
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	should not be equal as integers    ${test_node_id}                    0
	Add Group Config                   ${test_node_id}	${test_gconfig}

	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	should not be equal as integers    ${test_node_id}    0

	DELETE	/api/v2/tags	{"node_id": ${test_node_id}, "group_config_name": "${test_gconfig}", "ids": [1, 2, 3]}

	Integer	response status    200
	Integer                    response body error    ${ERR_SUCCESS}

Query tag from non-existent node, it should return failure
	GET	/api/v2/tags?node_id=999

	Integer    response status        404
	Integer    response body error    ${ERR_NODE_NOT_EXIST}

After adding the tag, query the tag, it should return the added tag
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	should not be equal as integers    ${test_node_id}    0

	POST	/api/v2/tags	{"node_id": ${test_node_id}, "group_config_name": "${test_gconfig}", "tags": [{"name": "tag1", "address": "1!400001", "attribute": 1, "type": 4},{"name": "tag2", "address": "1!00001", "attribute": 3, "type": 14}]}

	Integer	response status    200

	GET	/api/v2/tags?node_id=${test_node_id}
	Integer                                     response status    200
	${result}	To Array                          $.tags


	${tags_len}	Array Len	${result}
	should be equal as integers        ${tags_len}    2

	${ret}	Tag Check	${result}	tag1	4	test_grp_config    1	1!400001
	should be equal as integers                          ${ret}        0

	${ret}	Tag Check	${result}	tag2	14	test_grp_config    3	1!00001
	should be equal as integers                           ${ret}       0

Update tags, it should return success
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	should not be equal as integers    ${test_node_id}    0

	GET	/api/v2/tags?node_id=${test_node_id}
	Integer                                     response status    200
	${result}	To Array                          $.tags

	${tag1_id}	Tag Find By Name	${result}	tag1
	${tag2_id}	Tag Find By Name	${result}	tag2

	PUT	/api/v2/tags	{"node_id": ${test_node_id}, "tags": [{"id": ${tag1_id}, "name": "tag11", "type": 14, "attribute": 3, "address": "2!400002"},{"id": ${tag2_id}, "name": "tag22", "type": 4, "attribute": 1, "address": "3!100001"}]}

	Integer    response status        200
	Integer    response body error    ${ERR_SUCCESS}

	GET	/api/v2/tags?node_id=${test_node_id}
	Integer                                     response status    200
	${result}	To Array                          $.tags


	${tags_len}	Array Len	${result}
	should be equal as integers        ${tags_len}    2

	${ret}	Tag Check	${result}	tag11	14	test_grp_config    3	2!400002
	should be equal as integers                            ${ret}        0

	${ret}	Tag Check	${result}	tag22	4	test_grp_config    1	3!100001
	should be equal as integers                           ${ret}        0

Update non-existent tags, it should return partial success
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	should not be equal as integers    ${test_node_id}    0

	GET	/api/v2/tags?node_id=${test_node_id}
	Integer                                     response status    200
	${result}	To Array                          $.tags

	${tag1_id}	Tag Find By Name	${result}	tag11

	PUT	/api/v2/tags	{"node_id": ${test_node_id}, "tags": [{"id": ${tag1_id}, "name": "tag11", "type": 4, "attribute": 1, "address": "1!400002"},{"id": 7788, "name": "tag22", "type": 4, "attribute": 1, "address": "3!100001"}]}

	Integer    response status        404
	Integer    response body error    ${ERR_TAG_NOT_EXIST}

	GET	/api/v2/tags?node_id=${test_node_id}
	Integer                                     response status    200
	${result}	To Array                          $.tags


	${tags_len}	Array Len	${result}
	should be equal as integers        ${tags_len}    2

	${ret}	Tag Check	${result}	tag11	4	test_grp_config    1	1!400002
	should be equal as integers                           ${ret}        0

	${ret}	Tag Check	${result}	tag22	4	test_grp_config    1	3!100001
	should be equal as integers                           ${ret}        0

Delete tags, it should return success
	${test_node_id}    Get Node ID	${NODE_DRIVER}    ${test_node}    

	should not be equal as integers    ${test_node_id}    0

	GET	/api/v2/tags?node_id=${test_node_id}

	Integer               response status    200
	${result}	To Array    $.tags

	${tags_len}	Array Len          ${result}
	should be equal as integers    ${tags_len}    2

	${tag1_id}	Tag Find By Name	${result}	tag11
	${tag2_id}	Tag Find By Name	${result}	tag22


	DELETE    /api/v2/tags	{"node_id": ${test_node_id}, "group_config_name": "${test_gconfig}", "ids": [${tag1_id}, ${tag2_id}]}

	Integer	response status    200
	Integer                    response body error    ${ERR_SUCCESS}

	GET	/api/v2/tags?node_id=${test_node_id}
	Integer                                     response status    200
	Object                                      $	{}


#When the address format of the tag added does not match the driver, it should return failure

#Add an existing tag(same name), it should return failure

#When adding a tag of an unsupported type in node, it should return failure

*** Keywords ***
Neuron Context Ready
	Neuron Ready
	Add Node	${NODE_DRIVER}    ${test_node}	${PLUGIN_MODBUS}

