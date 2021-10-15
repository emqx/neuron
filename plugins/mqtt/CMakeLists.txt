find_package(eclipse-paho-mqtt-c 1.3.9 REQUIRED)

file(GLOB PARSER_SRC "${CMAKE_SOURCE_DIR}/src/parser/*.c")
file(GLOB CONFIG_SRC "${CMAKE_SOURCE_DIR}/src/config.c")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(src 
	${PARSER_SRC} 
	${CONFIG_SRC} 
	plugin_mqtt.c 
	# paho_client.c 
	mqttc_client.c
	common.c 
	command_rw.c
	command_group_config.c
	command_datatag.c
	command_node.c)
add_library(plugin-mqtt SHARED ${src})

target_include_directories(plugin-mqtt PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(plugin-mqtt 
	neuron-base mqttc 
	# paho-mqtt3as 
	ssl crypto 
	yaml)
target_link_libraries(plugin-mqtt ${CMAKE_THREAD_LIBS_INIT})
