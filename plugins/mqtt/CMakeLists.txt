file(GLOB PARSER_SRC "${CMAKE_SOURCE_DIR}/src/parser/*.c")
file(GLOB CONFIG_SRC "${CMAKE_SOURCE_DIR}/src/config.c")
file(GLOB SCHEMA_SRC "${CMAKE_SOURCE_DIR}/src/schema/schema.c")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(src 
	${PARSER_SRC} 
	${CONFIG_SRC} 
	${SCHEMA_SRC} 
	plugin_mqtt.c 
	mqtt_client.c
	mqttc_client.c
	command/common.c 
	command/command_rw.c
	command/command_group_config.c
	command/command_datatag.c
	command/command_node.c)
add_library(plugin-mqtt SHARED ${src})

target_include_directories(plugin-mqtt PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include)

target_compile_definitions(plugin-mqtt PRIVATE MQTT_USE_BIO)

target_link_libraries(plugin-mqtt 
	neuron-base 
	mqttc 
	ssl 
	crypto 
	yaml)

target_link_libraries(plugin-mqtt ${CMAKE_THREAD_LIBS_INIT})
