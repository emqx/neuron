*** Settings ***
Library    Process

*** Variables ***
${SIMULATOR_DIR}                  build/simulator
${MODBUS_TCP_SERVER_SIMULATOR}    ./modbus_tcp_server_simulator
${OPCUA_SERVER_SIMULATOR}         ./opcua_simulator

*** Keywords ***
Start Simulator
	[Arguments]	${simulator}	${cwd}

	${handle} =	Start Process    ${simulator}    cwd=${cwd}
	[Return]	${handle}

Stop Simulator
	[Arguments]	${handle}

	Terminate Process	${handle}	kill=true