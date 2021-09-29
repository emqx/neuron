*** Settings ***
Library     Keyword.NeuronLib.NeuronLib
Library     REST    http://127.0.0.1:7001
Suite Setup     Neuron Ready
Suite Teardown  Stop Neuron

*** Variables ***
${pro}

*** Test Cases ***
POST ping, status can be ok
    POST        /api/v2/ping
    Integer     response status     200
    String      response body status    OK


*** Keywords ***
Neuron Ready
    Start Neuron
    Sleep   3

