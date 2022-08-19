*** Settings ***
Resource 	resource/api.resource
Resource	resource/common.resource
Resource	resource/error.resource
Suite Setup	Start Neuronx
Suite Teardown	Stop Neuronx


*** Test Cases ***
Start an unconfigured node, it should return failure
	${res}=		Add Node  modbus-node  ${PLUGIN-MODBUS-TCP}
	Sleep 	1s

    	${state} =    Get Node State    modbus-node
    	Should Be Equal As Integers    ${state}[running]    ${NODE_STATE_INIT}
    	Should Be Equal As Integers    ${state}[link]       ${NODE_LINK_STATE_DISCONNECTED}

    	${res} =    Node Ctl    modbus-node    ${NODE_CTL_START}
    	Check Response Status    ${res}    409
    	Check Error Code         ${res}    ${NEU_ERR_NODE_NOT_READY}

Stop node that is ready(init/idle), it should return failure
    	${res} =                 Node Ctl    modbus-node    ${NODE_CTL_STOP}
    	Check Response Status    ${res}      409
    	Check Error Code         ${res}      ${NEU_ERR_NODE_NOT_RUNNING}

    	${state} =    Get Node State    modbus-node
    	Should Be Equal As Integers    ${state}[running]    ${NODE_STATE_INIT}
    	Should Be Equal As Integers    ${state}[link]       ${NODE_LINK_STATE_DISCONNECTED}

    	[Teardown]    Del Node  modbus-node

Start the configured node, it should return success
	${res}=		Add Node  modbus-node  ${PLUGIN-MODBUS-TCP}
    	Sleep    1s

    	Node Setting        modbus-node		${MODBUS_TCP_CONFIG}
    	${state} =    Get Node State    modbus-node
    	Should Be Equal As Integers    ${state}[running]    ${NODE_STATE_RUNNING}
    	Should Be Equal As Integers    ${state}[link]       ${NODE_LINK_STATE_DISCONNECTED}

    	${res} =                 Node Ctl    modbus-node    ${NODE_CTL_STOP}
    	Check Response Status    ${res}      200
    	Check Error Code         ${res}      ${NEU_ERR_SUCCESS}

    	${state} =    Get Node State    modbus-node
    	Should Be Equal As Integers    ${state}[running]    ${NODE_STATE_STOP}
    	Should Be Equal As Integers    ${state}[link]       ${NODE_LINK_STATE_DISCONNECTED}

Stop node that is stopped, it should return failure
    	${res} =                 Node Ctl    modbus-node    ${NODE_CTL_STOP}
    	Check Response Status    ${res}      409
    	Check Error Code         ${res}      ${NEU_ERR_NODE_IS_STOPED}

Start the stopped node, it should return success
    	${res} =                 Node Ctl    modbus-node    ${NODE_CTL_START}
    	Check Response Status    ${res}      200
    	Check Error Code         ${res}      ${NEU_ERR_SUCCESS}

    	${state} =    Get Node State    modbus-node
    	Should Be Equal As Integers    ${state}[running]    ${NODE_STATE_RUNNING}
    	Should Be Equal As Integers    ${state}[link]       ${NODE_LINK_STATE_DISCONNECTED}

Start the running node, it should return failure
    	${res} =                 Node Ctl    modbus-node    ${NODE_CTL_START}
    	Check Response Status    ${res}      409
    	Check Error Code         ${res}      ${NEU_ERR_NODE_IS_RUNNING}

When setting up a READY/RUNNING/STOPED node, the node status will not change
	${res} =    Node Setting    modbus-node    ${MODBUS_TCP_CONFIG}

    	${state} =    Get Node State    modbus-node
    	Should Be Equal As Integers    ${state}[running]    ${NODE_STATE_RUNNING}
    	Should Be Equal As Integers    ${state}[link]       ${NODE_LINK_STATE_DISCONNECTED}

    	${res} =                 Node Ctl    modbus-node    ${NODE_CTL_STOP}
    	Check Response Status    ${res}      200
    	Check Error Code         ${res}      ${NEU_ERR_SUCCESS}

	${res} =    Node Setting    modbus-node    ${MODBUS_TCP_CONFIG}

    	${state} =    Get Node State    modbus-node
    	Should Be Equal As Integers    ${state}[running]    ${NODE_STATE_STOP}
    	Should Be Equal As Integers    ${state}[link]       ${NODE_LINK_STATE_DISCONNECTED}

    	[Teardown]    Del Node  modbus-node