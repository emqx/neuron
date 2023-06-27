*** Settings ***
Library             String
Library             Collections
Resource            resource/api.resource
Resource            resource/common.resource
Resource            resource/error.resource

Suite Setup         Start Neuronx
Suite Teardown      Stop Neuronx


*** Test Cases ***
Get all metrics, it should return success
    ${res}=    Get all metrics

    Check Response Status    ${res}    200

Get single node metrics, it should return success
    ${res}=    Get single node metrics    monitor    app

    Check Response Status    ${res}    200

Check Prometheus format, it should return success
    ${nlines}=    Get metrics lines
    ${parameter_lines}=    Get parameter lines    ${nlines}

    Check endswith isdigit    ${parameter_lines}

Check node state, it should return success
    ${nlines}=    Get metrics lines
    ${parameter_lines}=    Get parameter lines    ${nlines}

    Check node state    ${parameter_lines}