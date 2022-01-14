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

logout without token, it should return ERR_NEED_TOKEN
    POST       /api/v2/logout
    Integer    response status    401
    Integer    $.error            ${ERR_NEED_TOKEN}

logout with invalid token, it should return ERR_DECODE_TOKEN
    Set Headers    {"Authorization":"Bearer xxxxxxxx"}
    POST           /api/v2/logout

    Integer    response status    401
    Integer    $.error            ${ERR_DECODE_TOKEN}

logout with invalid token, it should return ERR_EXPIRED_TOKEN
    Set Headers    {"Authorization":"Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE2MzgzNTczODh9.AQPun0sh20cJ-S8JOH3zAJu_CkjLGOdV-jghtkqyN3RCBSMAyPhSAP2L2H22sZF_id6l-EYVFvJXVFi9vVZ1qT1xTHpakze84Kbf_KkWfg_SbhywTZ8HgLhkf0_9en2eyoCPUA7NxM0l6_JZJbrTLMlb2b8bPq-CjXbNv-vt4yjyovZTmWuU379aCU4rc4FPYGvBucLVRTxNBGFzJwHQ22pAtkbByIrb5pPOqOT8-73-D6NwawPXAHouNUSjrXuqTIlf_tG6FZld0CGekm1EI93m1IoJ_Y-z4M419BAHpFFD-uUVXqnZPc3AgRhrXvs98IsE5tvwzuZqNaYh34iHiQ"}
    POST           /api/v2/logout

    Integer    response status    403
    Integer    $.error            ${ERR_EXPIRED_TOKEN}

logout with invalid token, it should return ERR_INVALID_TOKEN
    ${token} =     LOGIN
    ${jwt} =       Catenate                       Bearer    ${token}
    Set Headers    {"Authorization": "${jwt}"}
    LOGOUT

    Set Headers    {"Authorization": "${jwt}"}
    POST           /api/v2/logout
    Integer        response status                403
    Integer        $.error                        ${ERR_INVALID_TOKEN}

logout with valid token, it should return success
    ${token} =     LOGIN
    ${jwt} =       Catenate                      Bearer    ${token}
    Set Headers    {"Authorization":"${jwt}"}
    LOGOUT

*** Keywords ***
Neuron Context Ready
  Import Neuron API Resource
  Skip If Not Http API

  Neuron Ready

Neuron Context Stop
  Stop Neuron      remove_persistence_data=True
