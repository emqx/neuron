*** Settings ***
Resource          resource/api.resource
Resource          resource/common.resource
Resource          resource/error.resource
Suite Setup       Start Neuronx
Suite Teardown    Stop Neuronx


*** Variables ***
${g_plugin}         ${PLUGIN_MODBUS_TCP}
${g_template}       template-modbus
${g_node}           modbus
&{g_tag1}           name=tag1     address=1!400001      attribute=${3}   type=${4}    precision=${1}    decimal=${0}
&{g_tag2}           name=tag2     address=1!400009      attribute=${1}   type=${11}
&{g_tag_bad_type}   name=tag3     address=1!000001      attribute=${1}   type=${4}
&{g_tag_bad_addr}   name=tag4     address=F!400009      attribute=${1}   type=${11}
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
    ${name} =                     Evaluate              "a"*200
    ${group} =                    Create Dictionary     name=${name}                  interval=${2000}      tags=${g_tags}
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
    Check Response Status         ${res}                206
    Check Error Code              ${res}                ${NEU_ERR_TAG_TYPE_NOT_SUPPORT}

    [Teardown]                    Del Template          ${g_template}


Create a template with invalid tag address, it should fail.
    ${tags} =                     Create List           ${g_tag_bad_addr}
    ${group} =                    Create Dictionary     name=group1                   interval=${2000}      tags=${tags}
    ${groups} =                   Create List           ${group}

    ${res} =                      Add Template          ${g_template}                 ${g_plugin}           ${groups}
    Check Response Status         ${res}                206
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


Create a template with singleton plugin, it should fail.
    ${res} =                      Add Template          ${g_template}                 Monitor               ${g_empty}
    Check Response Status         ${res}                400
    Check Error Code              ${res}                ${NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE}

    [Teardown]                    Del Template          ${g_template}


Get nonexistent template, it should fail.
    ${res} =                      Get Template          ${g_template}
    Check Response Status         ${res}                404
    Check Error Code              ${res}                ${NEU_ERR_TEMPLATE_NOT_FOUND}


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


*** Keywords ***
Restart Neuron
    ${result} =                   Terminate Process     ${neuron_process}
    Should Be Empty               ${result.stderr}

    ${process} =                  Start Process         ./neuron  --log  --disable_auth  cwd=build  stdout=neuron.log
    Set Global Variable           ${neuron_process}     ${process}
    Sleep   1


Should Match Test Tag1
    [Arguments]                   ${tag}
    Should Be Equal As Strings    ${tag}[name]          ${g_tag1}[name]
    Should Be Equal As Strings    ${tag}[address]       ${g_tag1}[address]
    Should Be Equal As Integers   ${tag}[attribute]     ${g_tag1}[attribute]
    Should Be Equal As Integers   ${tag}[type]          ${g_tag1}[type]
    Should Be Equal As Integers   ${tag}[decimal]       ${g_tag1}[decimal]
    Should Be Equal As Integers   ${tag}[precision]     ${g_tag1}[precision]


Should Match Test Tag2
    [Arguments]                   ${tag}
    Should Be Equal As Strings    ${tag}[name]          ${g_tag2}[name]
    Should Be Equal As Strings    ${tag}[address]       ${g_tag2}[address]
    Should Be Equal As Integers   ${tag}[attribute]     ${g_tag2}[attribute]
    Should Be Equal As Integers   ${tag}[type]          ${g_tag2}[type]
    Should Be Equal As Integers   ${tag}[decimal]       0
    Should Be Equal As Integers   ${tag}[precision]     0


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
    List Length Should Be         ${res}[nodes]         1
    Set Local Variable            ${node}               ${res}[nodes][0]
    Should Be Equal As Strings    ${node}[name]         ${node_name}
    Should Be Equal As Strings    ${node}[plugin]       ${g_plugin}

    ${res}=                       Get Group             ${node_name}
    Check Response Status         ${res}                200
    Should Have Only One Group    ${res}[groups]        ${g_group1}[name]             ${g_group1}[interval]
    Group Tag Count Should Be     ${res}[groups][0]     2

    ${res}=                       Get Tags              ${node_name}                  ${g_group1}[name]
    Check Response Status         ${res}                200
    Should Match Test Tags        ${res}[tags]
