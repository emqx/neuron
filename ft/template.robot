*** Settings ***
Resource          resource/api.resource
Resource          resource/common.resource
Resource          resource/error.resource
Suite Setup       Start Neuronx
Suite Teardown    Stop Neuronx


*** Variables ***
${ATTR_RW}          ${${TAG_ATTRIBUTE_RW}}
${ATTR_RD}          ${${TAG_ATTRIBUTE_READ}}
${TYPE_I16}         ${${TAG_TYPE_INT16}}
${TYPE_U16}         ${${TAG_TYPE_UINT16}}
${TYPE_BIT}         ${${TAG_TYPE_BIT}}

${g_plugin}         ${PLUGIN_MODBUS_TCP}
${g_template}       template-modbus
${g_node}           modbus
${g_long_str}       ${{'a'*200}}
&{g_tag1}           name=tag1     address=1!400001      attribute=${${ATTR_RW}}       type=${${TYPE_U16}}   precision=${1}    decimal=${0}
&{g_tag2}           name=tag2     address=1!400009      attribute=${${ATTR_RD}}       type=${${TYPE_BIT}}
${g_tag_bad_type}   ${{{**$g_tag1, 'address':'0!000001'}}}
${g_tag_bad_addr}   ${{{**$g_tag2, 'address':'F!400009'}}}
@{g_tags}           ${g_tag1}     ${g_tag2}
&{g_group1}         name=group1   interval=${2000}      tags=${g_tags}
@{g_groups}         ${g_group1}
@{g_empty}


*** Test Cases ***
Create a template with a plugin that does not exist, it should fail.
    ${res} =                      Add Template          ${g_template}                 no-such-plugin        ${g_empty}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_PLUGIN_NOT_FOUND}

    [Teardown]                    Del Template          ${g_template}


Create a template with too long name, it should fail.
    ${res} =                      Add Template          ${g_long_str}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_long_str}


Create a template with too long plugin name, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_long_str}         ${g_groups}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_PLUGIN_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Create a template having no group, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    [Teardown]                    Del Template          ${g_template}


Create a template having one group, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    [Teardown]                    Del Template          ${g_template}


Create a template with invalid group interval, it should fail.
    ${group} =                    Create Dictionary     name=group1                   interval=10           tags=${g_tags}
    ${groups} =                   Create List           ${group}

    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${groups}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_PARAMETER_INVALID}

    [Teardown]                    Del Template          ${g_template}


Create a template with too long group name, it should fail.
    ${group} =                    Create Dictionary     name=${g_long_str}            interval=${2000}      tags=${g_tags}
    ${groups} =                   Create List           ${group}

    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${groups}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Create a template with invalid tag type, it should fail.
    ${tags} =                     Create List           ${g_tag_bad_type}
    ${group} =                    Create Dictionary     name=group1                   interval=${2000}      tags=${tags}
    ${groups} =                   Create List           ${group}

    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${groups}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TAG_TYPE_NOT_SUPPORT}

    [Teardown]                    Del Template          ${g_template}


Create a template with invalid tag address, it should fail.
    ${tags} =                     Create List           ${g_tag_bad_addr}
    ${group} =                    Create Dictionary     name=group1                   interval=${2000}      tags=${tags}
    ${groups} =                   Create List           ${group}

    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${groups}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TAG_ADDRESS_FORMAT_INVALID}

    [Teardown]                    Del Template          ${g_template}


Create a template with occupied name, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                409
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_EXIST}

    [Teardown]                    Del Template          ${g_template}


Create a template with north plugin, it should fail.
    ${res} =                      Add Template          ${g_template}                 MQTT                  ${g_empty}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE}

    [Teardown]                    Del Template          ${g_template}


Get nonexistent template, it should fail.
    ${res} =                      Get Template          ${g_template}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}


Get a template using too long name query parameter, it should fail.
    ${res} =                      Get Template          ${g_long_str}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_PARAM_IS_WRONG}


Get an existent template having no groups, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template          ${g_template}
    Check Response Status         ${res}                200
    Should Be Equal As Strings    ${res}[name]          ${g_template}
    Should Be Equal As Strings    ${res}[plugin]        ${g_plugin}
    Should Be Empty               ${res}[groups]

    [Teardown]                    Del Template          ${g_template}


Get an existent template having one group, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template          ${g_template}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        ${g_group1}[name]             ${g_group1}[interval]
    Should Match Test Tags        ${res}[groups][0][tags]

    [Teardown]                    Del Template          ${g_template}


Get all templates when there is none, it should return empty result.
    ${res} =                      Get Templates
    Check Response Status         ${res}                200
    Should Be Empty               ${res}[templates]


Get all templates, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Templates
    Check Response Status         ${res}                200
    List Length Should Be         ${res}[templates]     1
    Set Local Variable            ${info}               ${res}[templates][0]
    Should Be Equal As Strings    ${info}[name]         ${g_template}
    Should Be Equal As Strings    ${info}[plugin]       ${g_plugin}

    [Teardown]                    Del Template          ${g_template}


Delete a nonexistent template, it should fail.
    ${res} =                      Del Template          ${g_template}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}


Delete a template using too long name query parameter, it should fail.
    ${res} =                      Del Template          ${g_long_str}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_PARAM_IS_WRONG}


Delete an existent template, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template          ${g_template}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    [Teardown]                    Del Template          ${g_template}


When a template is created, its data should be persisted.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    Restart Neuron

    ${res} =                      Get Template          ${g_template}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        ${g_group1}[name]             ${g_group1}[interval]
    Should Match Test Tags        ${res}[groups][0][tags]

    Del Template                  ${g_template}

    Restart Neuron

    ${res} =                      Get Template          ${g_template}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}

    [Teardown]                    Del Template          ${g_template}


Instantiate a node from a nonexistent template, it should fail.
    ${res} =                      Inst Template         ${g_template}                 ${g_node}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}

    [Teardown]                    Run Keywords          Del Node                      ${g_node}
    ...                           AND                   Del Template                  ${g_template}


Instantiate a node with conflicting name from an existent template, it should fail.
    ${res} =                      Add Node              ${g_node}                     ${g_plugin}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Inst Template         ${g_template}                 ${g_node}
    Check Response Status         ${res}                409
    Check Error Code              ${res}                ${NEU_ERR_NODE_EXIST}

    [Teardown]                    Run Keywords          Del Node                      ${g_node}
    ...                           AND                   Del Template                  ${g_template}


Instantiate a new node from an existent template, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Inst Template         ${g_template}                 ${g_node}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    Should Match Test Template    ${g_node}

    [Teardown]                    Run Keywords          Del Node                      ${g_node}
    ...                           AND                   Del Template                  ${g_template}


Instantiated node should be persisted.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Inst Template         ${g_template}                 ${g_node}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    Restart Neuron

    Should Match Test Template    ${g_node}

    [Teardown]                    Run Keywords          Del Node                      ${g_node}
    ...                           AND                   Del Template                  ${g_template}


Instantiate a batch with a nonexistent template, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${inst1} =                    Create Dictionary     name=${g_template}            node=node1
    ${inst2} =                    Create Dictionary     name=not-exist-tmpl           node=node2
    ${res} =                      Inst Templates        ${inst1}                      ${inst2}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}

    [Teardown]                    Run Keywords          Del Node                      node1
    ...                           AND                   Del Node                      node2
    ...                           AND                   Del Template                  ${g_template}


Instantiate a batch with conflicting name from an existent template, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${inst1} =                    Create Dictionary     name=${g_template}            node=node
    ${inst2} =                    Create Dictionary     name=${g_template}            node=node
    ${res} =                      Inst Templates        ${inst1}                      ${inst2}
    Check Response Status         ${res}                409
    Check Error Code              ${res}                ${NEU_ERR_NODE_EXIST}

    [Teardown]                    Run Keywords          Del Node                      node
    ...                           AND                   Del Node                      node
    ...                           AND                   Del Template                  ${g_template}


Instantiate a batch from existent templates, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${inst1} =                    Create Dictionary     name=${g_template}            node=node1
    ${inst2} =                    Create Dictionary     name=${g_template}            node=node2
    ${res} =                      Inst Templates        ${inst1}                      ${inst2}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    Should Match Test Template    node1
    Should Match Test Template    node2

    [Teardown]                    Run Keywords          Del Node                      node1
    ...                           AND                   Del Node                      node2
    ...                           AND                   Del Template                  ${g_template}


Instantiated batch should be persisted.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${inst1} =                    Create Dictionary     name=${g_template}            node=node1
    ${inst2} =                    Create Dictionary     name=${g_template}            node=node2
    ${res} =                      Inst Templates        ${inst1}                      ${inst2}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    Restart Neuron

    Should Match Test Template    node1
    Should Match Test Template    node2

    [Teardown]                    Run Keywords          Del Node                      node1
    ...                           AND                   Del Node                      node2
    ...                           AND                   Del Template                  ${g_template}


Add group to nonexistent template, it should fail.
    ${res} =                      Add Template Group    ${g_template}                 ${g_group1}[name]     ${2000}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}

    [Teardown]                    Del Template          ${g_template}


Add group with conflicting name to existing template, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template Group    ${g_template}                 ${g_group1}[name]     ${2000}
    Check Response Status         ${res}                409
    Check Error Code              ${res}                ${NEU_ERR_GROUP_EXIST}

    [Teardown]                    Del Template          ${g_template}


Add group with invalid interval to existing template, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template Group    ${g_template}                 ${g_group1}[name]     ${10}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_PARAMETER_INVALID}

    [Teardown]                    Del Template          ${g_template}


Add group using too long template name in request, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template Group    ${g_long_str}                 ${g_group1}[name]     ${2000}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Add group with too long name to existing template, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template Group    ${g_template}                 ${g_long_str}         ${2000}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Add group to existing template, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template Group    ${g_template}                 ${g_group1}[name]     ${g_group1}[interval]
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        ${g_group1}[name]             ${g_group1}[interval]
    Group Tag Count Should Be     ${res}[groups][0]     0

    [Teardown]                    Del Template          ${g_template}


Get groups from nonexistent template, it should fail.
    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}


Get groups using too long name query parameter, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Groups   ${g_long_str}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_PARAM_IS_WRONG}

    [Teardown]                    Del Template          ${g_template}


Get groups when there are no groups, it should return empty result.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                200
    Should Be Empty               ${res}[groups]

    [Teardown]                    Del Template          ${g_template}


Get groups from existent template having one group, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        ${g_group1}[name]             ${g_group1}[interval]
    Group Tag Count Should Be     ${res}[groups][0]     2

    [Teardown]                    Del Template          ${g_template}


Update group interval when template does not exist, it should fail.
    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]     interval=${1000}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}

    [Teardown]                    Del Template          ${g_template}


Update group using too long template name in request, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Group    ${g_long_str}                 ${g_group1}[name]     interval=${2000}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Update group using too long group name in request, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Group    ${g_template}                 ${g_long_str}         interval=${2000}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Update group interval when group does not exist, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]     interval=${1000}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NOT_EXIST}

    [Teardown]                    Del Template          ${g_template}


Update group interval to invalid value, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]     interval=${10}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_PARAMETER_INVALID}

    [Teardown]                    Del Template          ${g_template}


Update group interval, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]     interval=${1000}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        ${g_group1}[name]             ${1000}
    Group Tag Count Should Be     ${res}[groups][0]     2

    [Teardown]                    Del Template          ${g_template}


Update group with empty new name, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]     new_name=${EMPTY}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_BODY_IS_WRONG}

    [Teardown]                    Del Template          ${g_template}


Update group with too long new name, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]     new_name=${g_long_str}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Update group name to conflicting group name, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template Group    ${g_template}                 grp                   ${1000}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}


    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]     new_name=grp
    Check Response Status         ${res}                409
    Check Error Code              ${res}                ${NEU_ERR_GROUP_EXIST}

    [Teardown]                    Del Template          ${g_template}


Update group name, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]     new_name=grp
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        grp                           ${g_group1}[interval]
    Group Tag Count Should Be     ${res}[groups][0]     2

    [Teardown]                    Del Template          ${g_template}


Update group with at least name or interval, otherwise it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_BODY_IS_WRONG}

    [Teardown]                    Del Template          ${g_template}


Update group name and interval, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]     new_name=grp      interval=${1000}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        grp                           ${1000}
    Group Tag Count Should Be     ${res}[groups][0]     2

    [Teardown]                    Del Template          ${g_template}


Delete group from nonexistent template, it should fail.
    ${res} =                      Del Template Group    ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}


Delete nonexistent group from existent template, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template Group    ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NOT_EXIST}

    [Teardown]                    Del Template          ${g_template}


Delete group using too long template name in request, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template Group    ${g_long_str}                 ${g_group1}[name]
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Delete group using too long group name in request, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template Group    ${g_template}                 ${g_long_str}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Delete group from existent template, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template Group    ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                200
    Should Be Empty               ${res}[groups]

    [Teardown]                    Del Template          ${g_template}


Template groups should be persisted.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template Group    ${g_template}                 ${g_group1}[name]     ${g_group1}[interval]
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    Restart Neuron

    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        ${g_group1}[name]             ${g_group1}[interval]
    Group Tag Count Should Be     ${res}[groups][0]     0

    ${res} =                      Put Template Group    ${g_template}                 ${g_group1}[name]     new_name=grp      interval=${1000}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    Restart Neuron

    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        grp                           ${1000}
    Group Tag Count Should Be     ${res}[groups][0]     0

    ${res} =                      Del Template Group    ${g_template}                 grp
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    Restart Neuron

    ${res} =                      Get Template Groups   ${g_template}
    Check Response Status         ${res}                200
    Should Be Empty               ${res}[groups]

    [Teardown]                    Del Template          ${g_template}


Add tag to nonexistent template, it should fail.
    ${res} =                      Add Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}
    #Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}

    [Teardown]                    Del Template          ${g_template}


Add tag to nonexistent template group, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}
    #Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NOT_EXIST}

    [Teardown]                    Del Template          ${g_template}


Add tag with conflicting name to existing template group, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}
    #Check Response Status         ${res}                409
    Check Error Code              ${res}                ${NEU_ERR_TAG_NAME_CONFLICT}

    [Teardown]                    Del Template          ${g_template}


Add tag using too long template name in request, it should fail.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    ${res} =                      Add Template Tags     ${g_long_str}                 ${g_group1}[name]     ${g_tag1}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Add tag using too long group name in request, it should fail.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    ${res} =                      Add Template Tags     ${g_template}                 ${g_long_str}         ${g_tag1}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Add tag with too long name to existing template group, it should fail.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    &{tag} =                      New Tag From          ${g_tag1}                     name=${g_long_str}
    ${res} =                      Add Template Tags     ${g_template}                 ${g_group1}[name]     ${tag}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TAG_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Add tag with invalid type to existing template group, it should fail.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    ${res} =                      Add Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag_bad_type}
    #Check Response Status         ${res}                206
    Check Error Code              ${res}                ${NEU_ERR_TAG_TYPE_NOT_SUPPORT}

    [Teardown]                    Del Template          ${g_template}


Add tag with invalid address to existing template group, it should fail.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    ${res} =                      Add Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag_bad_addr}
    #Check Response Status         ${res}                206
    Check Error Code              ${res}                ${NEU_ERR_TAG_ADDRESS_FORMAT_INVALID}

    [Teardown]                    Del Template          ${g_template}


Add tag to existing template group, it should success.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    ${res} =                      Add Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    [Teardown]                    Del Template          ${g_template}


Add many tags to existing template group, it should success.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    ${res} =                      Add Template Tags     ${g_template}                 ${g_group1}[name]     @{g_tags}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    Should Match Test Tags        ${res}[tags]

    [Teardown]                    Del Template          ${g_template}


Get tags from nonexistent template, it should fail.
    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}

    [Teardown]                    Del Template          ${g_template}


Get tags from nonexistent template group, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NOT_EXIST}

    [Teardown]                    Del Template          ${g_template}


Get tags using too long template name query parameter, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Tags     ${g_long_str}                 ${g_group1}[name]
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_PARAM_IS_WRONG}

    [Teardown]                    Del Template          ${g_template}


Get tags using too long group name query parameter, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Tags     ${g_template}                 ${g_long_str}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_PARAM_IS_WRONG}

    [Teardown]                    Del Template          ${g_template}


Get tags when there are none from existent template group, it should return empty result.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    Should Be Empty               ${res}[tags]

    [Teardown]                    Del Template          ${g_template}


Get tags from existent template group, it should return the tags.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    Should Match Test Tags        ${res}[tags]

    [Teardown]                    Del Template          ${g_template}


Get tags using too long tag name query parameter, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Grep Template Tags    ${g_template}                 ${g_group1}[name]     ${g_long_str}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_PARAM_IS_WRONG}

    [Teardown]                    Del Template          ${g_template}


Get tags by name pattern when there is no match, it should return empty result.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    ${res} =                      Grep Template Tags    ${g_template}                 ${g_group1}[name]     ${g_tag1}[name]
    Check Response Status         ${res}                200
    Should Be Empty               ${res}[tags]

    [Teardown]                    Del Template          ${g_template}


Get tags by name pattern when there is one match, it should return the matched tag.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Grep Template Tags    ${g_template}                 ${g_group1}[name]     ${g_tag1}[name]
    Check Response Status         ${res}                200
    List Length Should Be         ${res}[tags]          1
    Should Match Test Tag1        ${res}[tags][0]

    [Teardown]                    Del Template          ${g_template}


Get tags by name pattern, it should return all matched tags.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Grep Template Tags    ${g_template}                 ${g_group1}[name]     tag
    Check Response Status         ${res}                200
    Should Match Test Tags        ${res}[tags]

    [Teardown]                    Del Template          ${g_template}


Update tag when template does not exist, it should fail.
    ${res} =                      Add Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}
    #Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}

    [Teardown]                    Del Template          ${g_template}


Update tag when template group does not exist, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}
    #Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NOT_EXIST}

    [Teardown]                    Del Template          ${g_template}


Update tag when template tag does not exist, it should fail.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    ${res} =                      Put Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}
    #Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TAG_NOT_EXIST}

    [Teardown]                    Del Template          ${g_template}


Update tag using too long template name in request, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Tags     ${g_long_str}                 ${g_group1}[name]     ${g_tag1}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Update tag using too long group name in request, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Tags     ${g_template}                 ${g_long_str}         ${g_tag1}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Update tag name to invalid value, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    &{tag} =                      New Tag From          ${g_tag1}                     name=${g_long_str}
    ${res} =                      Put Template Tags     ${g_template}                 ${g_group1}[name]     ${tag}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TAG_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Update tag type to invalid value, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag_bad_type}
    #Check Response Status         ${res}                206
    Check Error Code              ${res}                ${NEU_ERR_TAG_TYPE_NOT_SUPPORT}

    [Teardown]                    Del Template          ${g_template}


Update tag address to invalid value, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Put Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag_bad_addr}
    #Check Response Status         ${res}                206
    Check Error Code              ${res}                ${NEU_ERR_TAG_ADDRESS_FORMAT_INVALID}

    [Teardown]                    Del Template          ${g_template}


Update one tag, it should success.
    &{tag} =                      New Tag From          ${g_tag1}                     address=1!400002
    Add Template And Tags         ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]   ${tag}                ${g_tag2}

    ${res} =                      Put Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    Should Match Test Tags        ${res}[tags]

    [Teardown]                    Del Template          ${g_template}


Update many tags, it should success.
    &{tag1} =                     New Tag From          ${g_tag1}                     type=${${TAG_TYPE_INT16}}
    &{tag2} =                     New Tag From          ${g_tag2}                     address=1!400008
    Add Template And Tags         ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]   ${tag1}               ${tag2}

    ${res} =                      Put Template Tags     ${g_template}                 ${g_group1}[name]     @{g_tags}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    Should Match Test Tags        ${res}[tags]

    [Teardown]                    Del Template          ${g_template}


Delete tag when template does not exist, it should fail.
    ${res} =                      Del Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}[name]
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}

    [Teardown]                    Del Template          ${g_template}


Delete tag when template group does not exist, it should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}[name]
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NOT_EXIST}

    [Teardown]                    Del Template          ${g_template}


Delete tag when template tag does not exist, it trivially success.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    ${res} =                      Del Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}[name]
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    [Teardown]                    Del Template          ${g_template}


Delete tag using too long template name in request, it should should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template Tags     ${g_long_str}                 ${g_group1}[name]     ${g_tag1}[name]
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Delete tag using too long group name in request, it should should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template Tags     ${g_template}                 ${g_long_str}         ${g_tag1}[name]
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_GROUP_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Delete tag using too long tag name in request, it should should fail.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}[name]         ${g_long_str}
    #Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_TAG_NAME_TOO_LONG}

    [Teardown]                    Del Template          ${g_template}


Delete one tag, it should keep other tags untouched.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}[name]
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    List Length Should Be         ${res}[tags]          1
    Should Match Test Tag2        ${res}[tags][0]

    [Teardown]                    Del Template          ${g_template}


Delete all tags, it should success.
    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${g_groups}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Del Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}[name]         ${g_tag2}[name]
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    List Length Should Be         ${res}[tags]          0

    [Teardown]                    Del Template          ${g_template}


Template tags should be persisted.
    Add Template And One Group    ${g_template}         ${g_plugin}                   ${g_group1}[name]     ${g_group1}[interval]

    &{tag1} =                     New Tag From          ${g_tag1}                     address=1!400002
    &{tag2} =                     New Tag From          ${g_tag2}                     address=1!400008
    ${res} =                      Add Template Tags     ${g_template}                 ${g_group1}[name]     ${tag1}                 ${tag2}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    Restart Neuron

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    List Length Should Be         ${res}[tags]          2
    Should Be Equal As Tags       ${res}[tags][0]       ${tag1}
    Should Be Equal As Tags       ${res}[tags][1]       ${tag2}

    Restart Neuron

    ${res} =                      Put Template Tags     ${g_template}                 ${g_group1}[name]     @{g_tags}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    Should Match Test Tags        ${res}[tags]

    ${res} =                      Del Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag1}[name]

    Restart Neuron

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    List Length Should Be         ${res}[tags]          1
    Should Match Test Tag2        ${res}[tags][0]

    ${res} =                      Del Template Tags     ${g_template}                 ${g_group1}[name]     ${g_tag2}[name]

    Restart Neuron

    ${res} =                      Get Template Tags     ${g_template}                 ${g_group1}[name]
    Check Response Status         ${res}                200
    Should Be Empty               ${res}[tags]

    [Teardown]                    Del Template          ${g_template}


*** Keywords ***
Restart Neuron
    ${result} =                   Terminate Process     ${neuron_process}
    Should Be Empty               ${result.stderr}

    ${process} =                  Start Process         ./neuron  --log  --disable_auth  cwd=build  stdout=neuron.log
    Set Global Variable           ${neuron_process}     ${process}
    Sleep   1


Should Be Equal As Tags
    [Arguments]                   ${tag1}               ${tag2}
    Should Be Equal As Strings    ${tag1}[name]         ${tag2}[name]
    Should Be Equal As Strings    ${tag1}[address]      ${tag2}[address]
    Should Be Equal As Integers   ${tag1}[attribute]    ${tag2}[attribute]
    Should Be Equal As Integers   ${tag1}[type]         ${tag2}[type]

    IF        'decimal' in $tag1 and 'decimal' in $tag2
    Should Be Equal As Integers   ${tag1}[decimal]      ${tag2}[decimal]
    ELSE IF   'decimal' in $tag1
    Should Be Equal As Integers   ${tag1}[decimal]      0
    ELSE IF   'decimal' in $tag2
    Should Be Equal As Integers   ${tag2}[decimal]      0
    END

    IF        'precision' in $tag1 and 'precision' in $tag2
    Should Be Equal As Integers   ${tag1}[precision]    ${tag2}[precision]
    ELSE IF   'precision' in $tag1
    Should Be Equal As Integers   ${tag1}[precision]    0
    ELSE IF   'precision' in $tag2
    Should Be Equal As Integers   ${tag2}[precision]    0
    END

    IF        'description' in $tag1 and 'description' in $tag2
    Should Be Equal As Strings    ${tag1}[description]  ${tag2}[description]
    ELSE IF   'description' in $tag1
    Should Be Equal As Strings    ${tag1}[description]  ${EMPTY}
    ELSE IF   'description' in $tag2
    Should Be Equal As Strings    ${tag2}[description]  ${EMPTY}
    END


Should Match Test Tag1
    [Arguments]                   ${tag}
    Should Be Equal As Tags       ${tag}                ${g_tag1}


Should Match Test Tag2
    [Arguments]                   ${tag}
    Should Be Equal As Tags       ${tag}                ${g_tag2}


Should Match Test Tags
    [Arguments]                   ${tags}
    List Length Should Be         ${tags}               2
    Should Match Test Tag1        ${tags}[0]
    Should Match Test Tag2        ${tags}[1]


Should Have Only One Group
    [Arguments]                   ${groups}             ${name}                       ${interval}
    List Length Should Be         ${groups}             1
    Should Be Equal As Strings    ${name}               ${groups}[0][name]
    Should Be Equal As Integers   ${interval}           ${groups}[0][interval]


Group Tag Count Should Be
    [Arguments]                   ${group}              ${count}
    Should Be Equal As Integers   ${count}              ${group}[tag_count]


Should Match Test Template
    [Arguments]                   ${node_name}
    ${res}=                       Get Nodes             ${NODE_DRIVER}
    Check Response Status         ${res}                200
    ${nodes}=                     Evaluate              list(filter(lambda x: x['name'] == '${node_name}', $res['nodes']))
    List Length Should Be         ${nodes}              1
    Set Local Variable            ${node}               ${nodes}[0]
    Should Be Equal As Strings    ${node}[name]         ${node_name}
    Should Be Equal As Strings    ${node}[plugin]       ${g_plugin}

    ${res}=                       Get Group             ${node_name}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        ${g_group1}[name]             ${g_group1}[interval]
    Group Tag Count Should Be     ${res}[groups][0]     2

    ${res}=                       Get Tags              ${node_name}                  ${g_group1}[name]
    Check Response Status         ${res}                200
    Should Match Test Tags        ${res}[tags]


New Tag From
    [Arguments]                   ${tag}                &{updates}
    ${copy}                       Evaluate              {**$tag, **$updates}
    [Return]                      ${copy}


Add Template And One Group
    [Arguments]                   ${tmpl}               ${plugin}                     ${group}              ${interval}
    ${res} =                      Add Template          ${tmpl}                       ${plugin}             ${g_empty}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}

    ${res} =                      Add Template Group    ${tmpl}                       ${group}              ${interval}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}


Add Template And Tags
    [Arguments]                   ${tmpl}               ${plugin}                     ${group}              ${interval}             @{tags}
    Add Template And One Group    ${tmpl}               ${plugin}                     ${group}              ${interval}

    ${res} =                      Add Template Tags     ${tmpl}                       ${group}              @{tags}
    Check Response Status         ${res}                200
    Check Error Code              ${res}                ${NEU_ERR_SUCCESS}
