*** Settings ***
Resource          common.resource
Suite Setup       Neuron Context Ready
Suite Teardown    Neuron Context Stop

*** Variables ***
${test_node_id}
${test_gconfig}   test_gconfig
${test_node}      test_node
${driver_node}    driver-node
${app_node}       app-node

*** Test Cases ***
Add group config under non-existent node, it should return failure
  ${res} =                  Add Group Config                ${1234}               ${test_gconfig}             1000

  Check Response Status     ${res}                          404
  Check Error Code          ${res}                          ${ERR_NODE_NOT_EXIST}

Add group config under existent node, it should return success
  ${res} =                  Add Group Config                ${test_node_id}       ${test_gconfig}             1000

  Check Response Status     ${res}                          200
  Check Error Code          ${res}                          ${ERR_SUCCESS}

Delete a non-existent group config, it should return failure
  ${res} =                  Del Group Config                ${test_node_id}       xxxx

  Check Response Status     ${res}                          404
  Check Error Code          ${res}                          ${ERR_GRPCONFIG_NOT_EXIST}

Delete a group config from non-existent node, it should return failure
  ${res} =                  Del Group Config                ${1234}               ${test_gconfig}

  Check Response Status     ${res}                          404
  Check Error Code          ${res}                          ${ERR_GRPCONFIG_NOT_EXIST}

Delete a group config, it should return success
  ${res} =                  Del Group Config                ${test_node_id}        ${test_gconfig}

  Check Response Status     ${res}                          200
  Check Error Code          ${res}                          ${ERR_SUCCESS}

Update a non-existent group config, it should return failure
  ${res} =                  Update Group Config             ${test_node_id}       xxxx                      2000

  Check Response Status     ${res}                          404
  Check Error Code          ${res}                          ${ERR_GRPCONFIG_NOT_EXIST}

Update a group config from non-existent node, it should return failure
  ${res} =                  Update Group Config             ${1234}               ${test_gconfig}           2000

  Check Response Status     ${res}                          404
  Check Error Code          ${res}                          ${ERR_GRPCONFIG_NOT_EXIST}

Add a group config to the node, then get the group config from this node, it should return the group config
  ${res} =                  Add Group Config                ${test_node_id}       ${test_gconfig}           1000

  Check Response Status     ${res}                          200
  Check Error Code          ${res}                          ${ERR_SUCCESS}

  ${res} =                  Get Group Config                ${test_node_id}
  ${len} =                  Get Length                      ${res}[group_configs]
  ${config}                 Set Variable                    ${res}[group_configs][0]

  Should Be Equal As Integers                               ${len}                1
  Should Be Equal As Integers                               ${config}[interval]  1000

Update the interval of the test group config, it should return success
  ${res} =                  Update Group Config             ${test_node_id}       ${test_gconfig}           2000

  Check Response Status     ${res}                          200
  Check Error Code          ${res}                          ${ERR_SUCCESS}

  ${res} =                  Get Group Config                ${test_node_id}
  ${len} =                  Get Length                      ${res}[group_configs]
  ${config}                 Set Variable                    ${res}[group_configs][0]

  Should Be Equal As Integers                               ${len}                1
  Should Be Equal As Integers                               ${config}[interval]  2000

North APP subscribe non-existent group config, it should return failure
  ${app_node_id} =          Add Node And Return ID          ${NODE_MQTT}           ${app_node}               ${PLUGIN_MQTT}
  ${driver_node_id} =       Add Node And Return ID          ${NODE_DRIVER}        ${driver_node}            ${PLUGIN_MODBUS_TCP}

  ${res} =                  Subscribe Group                 ${driver_node_id}     ${app_node_id}            grp_config

  Check Response Status     ${res}                          404
  Check Error Code          ${res}                          ${ERR_GRPCONFIG_NOT_EXIST}

North APP subscribe group config, it should return success
  ${app_node_id} =          Get Node ID                     ${NODE_MQTT}           ${app_node}
  ${driver_node_id} =       Get Node ID                     ${NODE_DRIVER}        ${driver_node}
  ${res} =                  Add Group Config                ${driver_node_id}     grp_config                1000

  ${res} =                  Subscribe Group                 ${driver_node_id}     ${app_node_id}            grp_config

  Check Response Status     ${res}                          200
  Check Error Code          ${res}                          ${ERR_SUCCESS}

Query the subscribers of the group config, it should return all nodes subscribed to this group
  ${app_node_id} =          Get Node ID                     ${NODE_MQTT}           ${app_node}
  ${driver_node_id1} =      Get Node ID                     ${NODE_DRIVER}        ${driver_node}

  ${res} =                  Get Subscribe Group             ${app_node_id}
  Check Response Status     ${res}                          200

  ${len} =                  Get Length                      ${res}[groups]
  ${groups}                 Set Variable                    ${res}[groups]

  Should Be Equal As Integers                               ${len}                1

  ${check_ret} =            Subscribe_Check                 ${groups}             ${driver_node_id1}        grp_config
  Should Be Equal As Integers                               ${check_ret}          0

  ${driver_node_id2} =      Add Node And Return ID          ${NODE_DRIVER}        driver-node2              ${PLUGIN_MODBUS_TCP}

  ${res} =                  Add Group Config                ${driver_node_id2}    grp_config                1000
  Check Response Status     ${res}                          200
  Check Error Code          ${res}                          ${ERR_SUCCESS}

  ${res} =                  Subscribe Group                 ${driver_node_id2}    ${app_node_id}            grp_config
  Check Response Status     ${res}                          200
  Check Error Code          ${res}                          ${ERR_SUCCESS}

  ${res} =                  Get Subscribe Group             ${app_node_id}
  Check Response Status     ${res}                          200

  ${len} =                  Get Length                      ${res}[groups]
  ${groups}                 Set Variable                    ${res}[groups]

  Should Be Equal As Integers                               ${len}                2

  ${check_ret} =            Subscribe_Check                 ${groups}             ${driver_node_id1}        grp_config
  Should Be Equal As Integers                               ${check_ret}          0

  ${check_ret} =            Subscribe_Check                 ${groups}             ${driver_node_id2}        grp_config
  Should Be Equal As Integers                               ${check_ret}          0


Delete the subscribe group config, it should return failure
  ${driver_node_id1} =      Get Node ID                     ${NODE_DRIVER}        ${driver_node}

  ${res} =                  Del Group Config                ${driver_node_id1}    grp_config

  Check Response Status     ${res}                          412
  Check Error Code          ${res}                          ${ERR_GRP_CONFIG_IN_USE}

North APP unsubscribe non-existent group config, it should return failure
  ${app_node_id} =          Get Node ID                     ${NODE_MQTT}           ${app_node}
  ${driver_node_id} =       Get Node ID                     ${NODE_DRIVER}        ${driver_node}
  ${res} =                  Unsubscribe Group               ${driver_node_id}     ${app_node_id}            xxgrp_config

  Check Response Status     ${res}                          404
  Check Error Code          ${res}                          ${ERR_GRPCONFIG_NOT_EXIST}

North APP unsubscribe group config, it should return success
  ${app_node_id} =          Get Node ID                     ${NODE_MQTT}           ${app_node}
  ${driver_node_id1} =      Get Node ID                     ${NODE_DRIVER}        ${driver_node}
  ${driver_node_id2} =      Get Node ID                     ${NODE_DRIVER}        driver-node2

  ${res} =                  Unsubscribe Group               ${driver_node_id1}    ${app_node_id}            grp_config

  Check Response Status     ${res}                          200
  Check Error Code          ${res}                          ${ERR_SUCCESS}

  ${res} =                  Get Subscribe Group             ${app_node_id}
  Check Response Status     ${res}                          200

  ${len} =                  Get Length                      ${res}[groups]
  ${groups}                 Set Variable                    ${res}[groups]

  Should Be Equal As Integers                               ${len}                1

  ${check_ret} =            Subscribe_Check                 ${groups}             ${driver_node_id2}        grp_config
  Should Be Equal As Integers                               ${check_ret}          0


*** Keywords ***
Neuron Context Ready
  Import Neuron API Resource

  Neuron Ready

  LOGIN

  ${node_id} =              Add Node And Return ID          ${NODE_DRIVER}        ${test_node}              ${PLUGIN_MODBUS_TCP}
  Set Global Variable       ${test_node_id}                 ${node_id}

Neuron Context Stop
  Stop Neuron               remove_persistence_data=True
