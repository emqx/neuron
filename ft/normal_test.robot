*** Settings ***
Resource 	resource/api.resource
Resource	resource/common.resource
Resource	resource/error.resource
Suite Setup	Start Neuronx
Suite Teardown	Stop Neuronx

*** Test Cases ***
login with invalid username and password, it should return failure
    POST       /api/v2/login      {"name":"admin1", "pass": "0000"}
    Integer    response status    401
    Integer    $.error            ${NEU_ERR_INVALID_USER_OR_PASSWORD}

login with username and password, it should return success
    POST       /api/v2/login      {"name":"admin", "pass": "0000"}
    Integer    response status    200
    output     $.token
