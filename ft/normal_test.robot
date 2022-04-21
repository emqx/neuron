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

# logout without token, it should return ERR_NEED_TOKEN
#    POST       /api/v2/logout
#    Integer    response status    401
#    Integer    $.error            ${ERR_NEED_TOKEN}

# logout with invalid token, it should return ERR_DECODE_TOKEN
#    Set Headers    {"Authorization":"Bearer xxxxxxxx"}
#    POST           /api/v2/logout

#    Integer    response status    401
#    Integer    $.error            ${ERR_DECODE_TOKEN}

# logout with invalid token, it should return ERR_EXPIRED_TOKEN
#    Set Headers    {"Authorization":"Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJhdWQiOiJOZXVyb24iLCJib2R5RW5jb2RlIjowLCJleHAiOjE2NTA2MTc5NTYsImlhdCI6MTY1MDYxNDM1NiwiaXNzIjoiTmV1cm9uIn0.Xzosngbxnww0kG9MOE-_m5POhms0hkrigXO1aXqIdbmW8yToVVzzRsPdeLBh8NdzzZzAER7VDSzArjTlJskfVbn9kCt_8UzJEEl-zesCLqSj9rUccUXLvRMzqumIci-cZTvPiD8RPpT5IStZAwpbnPg0VMeU7zYV61EZj3fdvf3LNqwr6uzY-6JNt-dOgqc-knY_ibPW5Z_r-I_Y5jvNi7tR-OAokzbvbTJyEas4pxfGhsB-A6vFFnvIpO-PwnZLWbLMuqyrJTH1cGWZyjQfsezYr5QNNHiqHLhoMtsQS2umQO4QA69xjb_sWioM2_-coBeFw1p0vRpOu2ES5aILEQ"}
#    POST           /api/v2/logout

#    Integer    response status    403
#    Integer    $.error            ${ERR_EXPIRED_TOKEN}

# logout with invalid token, it should return ERR_INVALID_TOKEN
#    ${token} =     LOGIN
#    ${jwt} =       Catenate                       Bearer    ${token}
#    Set Headers    {"Authorization": "${jwt}"}
#    LOGOUT

#    Set Headers    {"Authorization": "${jwt}"}
#    POST           /api/v2/logout
#    Integer        response status                403
#    Integer        $.error                        ${ERR_INVALID_TOKEN}

# logout with valid token, it should return success
#    ${token} =     LOGIN
#    ${jwt} =       Catenate                      Bearer    ${token}
#    Set Headers    {"Authorization":"${jwt}"}
#    LOGOUT

*** Keywords ***
Neuron Context Ready
    Import Neuron API Resource
    Skip If Not Http API

    Neuron Ready

Neuron Context Stop
    Stop Neuron    remove_persistence_data=True
