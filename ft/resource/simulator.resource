*** Settings ***
Library    Process

*** Variables ***
${SIMULATOR_DIR}                  build/simulator
${MODBUS_TCP_SERVER_SIMULATOR}    ./modbus_simulator
${OPCUA_SERVER_SIMULATOR}         ./opcua_simulator

*** Keywords ***
Start Modbus Simulator
	${handle} =	Start Process    ${MODBUS_TCP_SERVER_SIMULATOR}	tcp	60502  ip_v4   cwd=build/simulator
	[Return]	${handle}

Start Simulator Arg
	[Arguments]	${simulator}	${cwd}

	${handle} =	Start Process    ${simulator}	tcp	60502   cwd=${cwd}
	[Return]	${handle}

Start Simulator
	[Arguments]	${simulator}	${cwd}

	${handle} =	Start Process    ${simulator}    cwd=${cwd}
	[Return]	${handle}

Stop Simulator
	[Arguments]	${handle}

	Terminate Process	${handle}	kill=true