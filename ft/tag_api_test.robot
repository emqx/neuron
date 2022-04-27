*** Settings ***
Resource          common.resource
Suite Setup       Neuron Context Ready
Suite Teardown    Neuron Context Stop

*** Variables ***
${test_node}        test-node
${test_gconfig}     test_grp_config
${tag1}             {"name": "tag1", "address": "1!400001", "attribute": 1, "type": 4}
${tag2}             {"name": "tag2", "address": "1!00001", "attribute": 3, "type": 14}

*** Test Cases ***
Add different types of tags to the group, it should return success
  Add Group Config                ${test_node_id}                     ${test_gconfig}   1000

  ${res} =                        Add Tags      ${test_node_id}       ${test_gconfig}   ${tag1},${tag2}
  Check Response Status           ${res}        200
  Check Error Code                ${res}        ${ERR_SUCCESS}

  [Teardown]                      Del Group Config Check              ${test_node_id}   ${test_gconfig}

Add tags to the non-existent group, the group will be created automatically
  ${res} =                        Add Tags      ${test_node_id}       ${test_gconfig}   ${tag1},${tag2}

  Check Response Status           ${res}        200
  Check Error Code                ${res}        ${ERR_SUCCESS}

  [Teardown]                      Del Group Config Check              ${test_node_id}   ${test_gconfig}

When the attribute of the tag added is incorrect, it should return partial success
  ${res} =                        Add Tags      ${test_node_id}       ${test_gconfig}   {"name": "tag1", "address": "1!400001", "attribute": 8, "type": 4},{"name": "tag2", "address": "1!00001", "attribute": 3, "type": 14}

  Check Response Status           ${res}        206
  Check Error Code                ${res}        ${ERR_TAG_ATTRIBUTE_NOT_SUPPORT}

  [Teardown]                      Del Group Config Check              ${test_node_id}   ${test_gconfig}

Add tags to non-existent node, it should return failure
  ${res} =                        Add Tags      ${999}                ${test_gconfig}   ${tag1},${tag2}

  Check Response Status           ${res}        404
  Check Error Code                ${res}        ${ERR_NODE_NOT_EXIST}

Delete tag from non-existent group config, it should return failure
  ${res} =                        Del Tags      ${test_node_id}       ${test_gconfig}   1,2,3

  Check Response Status           ${res}        404
  Check Error Code                ${res}        ${ERR_NODE_NOT_EXIST}

Delete non-existent tags, it should return success
  Add Group Config                ${test_node_id}                     ${test_gconfig}   1000

  ${res} =                        Del Tags      ${test_node_id}       ${test_gconfig}   1,2,3

  Check Response Status           ${res}        200
  Check Error Code                ${res}        ${ERR_SUCCESS}

Query tag from non-existent node, it should return failure
  ${res} =                        Get Tags      ${999}                ${test_gconfig}

  Check Response Status           ${res}        404
  Check Error Code                ${res}        ${ERR_NODE_NOT_EXIST}

After adding the tag, query the tag, it should return the added tag
  ${res} =                        Add Tags      ${test_node_id}       ${test_gconfig}   ${tag1},${tag2}

  Check Response Status           ${res}        200
  Check Error Code                ${res}        ${ERR_SUCCESS}

  ${res} =                        Get Tags      ${test_node_id}       ${test_gconfig}

  Check Response Status           ${res}        200
  ${len} =                        Get Length    ${res}[tags]
  Should Be Equal As Integers     ${len}        2

  ${ret} =                        Tag Check     ${res}[tags]          tag1              4         test_grp_config    1   1!400001
  Should Be Equal As Integers     ${ret}        0

  ${ret} =                        Tag Check     ${res}[tags]          tag2              14        test_grp_config    3   1!00001
  Should Be Equal As Integers     ${ret}        0

Update tags, it should return success
  ${tag1_id} =                    Get Tag ID    ${test_node_id}       ${test_gconfig}   tag1
  ${tag2_id} =                    Get Tag ID    ${test_node_id}       ${test_gconfig}   tag2

  ${res} =                        Update Tags   ${test_node_id}       ${test_gconfig}   {"id": ${tag1_id}, "name": "tag11", "type": 14, "attribute": 3, "address": "2!400002"},{"id": ${tag2_id}, "name": "tag22", "type": 4, "attribute": 1, "address": "3!100001"}

  Check Response Status           ${res}        200
  Check Error Code                ${res}        ${ERR_SUCCESS}

  ${res} =                        Get Tags      ${test_node_id}       ${test_gconfig}
  Check Response Status           ${res}        200
  ${len} =                        Get Length    ${res}[tags]
  Should Be Equal As Integers     ${len}        2

  ${ret} =                        Tag Check     ${res}[tags]          tag11             14        test_grp_config    3    2!400002
  Should Be Equal As Integers     ${ret}        0

  ${ret} =                        Tag Check     ${res}[tags]          tag22             4         test_grp_config    1    3!100001
  Should Be Equal As Integers     ${ret}        0

Update non-existent tags, it should return partial success
  ${tag1_id} =                    Get Tag ID    ${test_node_id}       ${test_gconfig}   tag11

  ${res} =                        Update Tags   ${test_node_id}       ${test_gconfig}   {"id": ${tag1_id}, "name": "tag11", "type": 4, "attribute": 1, "address": "1!400002"},{"id": 7788, "name": "tag22", "type": 4, "attribute": 1, "address": "3!100001"}
  Check Response Status           ${res}        404
  Check Error Code                ${res}        ${ERR_TAG_NOT_EXIST}

  ${res} =                        Get Tags      ${test_node_id}       ${test_gconfig}
  Check Response Status           ${res}        200

  ${len} =                        Get Length    ${res}[tags]
  Should Be Equal As Integers     ${len}        2

  ${ret} =                        Tag Check     ${res}[tags]          tag11   4  test_grp_config    1	1!400002
  Should Be Equal As Integers     ${ret}        0

  ${ret} =                        Tag Check     ${res}[tags]          tag22   4  test_grp_config    1	3!100001
  Should Be Equal As Integers     ${ret}        0

Delete tags, it should return success
  ${tag1_id} =                    Get Tag ID    ${test_node_id}       ${test_gconfig}  tag11
  ${tag2_id} =                    Get Tag ID    ${test_node_id}       ${test_gconfig}  tag22

  ${res} =                        Del Tags      ${test_node_id}       ${test_gconfig}  ${tag1_id},${tag2_id}

  Check Response Status           ${res}        200
  Check Error Code                ${res}        ${ERR_SUCCESS}

  ${res} =                        Get Tags      ${test_node_id}       ${test_gconfig}
  Check Response Status           ${res}        200
  Run Keyword If  ${res}[tags] != @{EMPTY}            Fail  not an empty object


#When the address format of the tag added does not match the driver, it should return failure

#Add an existing tag(same name), it should return failure

#When adding a tag of an unsupported type in node, it should return failure

*** Keywords ***
Neuron Context Ready
  Import Neuron API Resource

  Neuron Ready
  LOGIN

  ${id} =   Add Node And Return ID  ${NODE_DRIVER}    ${test_node}  ${PLUGIN_MODBUS_TCP}
  Should Not Be Equal As Integers   ${id}             0

  Set Global Variable               ${test_node_id}  ${id}

Neuron Context Stop
  Stop Neuron                       remove_persistence_data=True

Del Group Config Check
  [Arguments]                     ${node_id}                          ${group}
  ${res} =                        Del Group Config                    ${node_id}   ${group}
  Check Response Status           ${res}        200
  Check Error Code                ${res}        ${ERR_SUCCESS}
