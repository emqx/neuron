*** Settings ***
Resource          common.resource
Suite Setup       Neuron Context Ready
Suite Teardown    Neuron Context Stop

*** Variables ***

*** Test Cases ***
login with invalid username and password, it should return failure
    POST       /api/v2/login      {"name":"admin1", "pass": "0000"}
    Integer    response status    401
    Integer    $.error            ${ERR_INVALID_USER_OR_PASSWORD}

login with username and password, it should return success
    POST       /api/v2/login      {"name":"admin", "pass": "0000"}
    Integer    response status    200
    output     $.token

*** Keywords ***
Neuron Context Ready
    Import Neuron API Resource
    Skip If Not Http API

    Neuron Ready

Neuron Context Stop
    Stop Neuron    remove_persistence_data=True
