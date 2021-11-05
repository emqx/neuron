# MQTT Plugin
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set(src 
	plugin_mqtt.c 
	mqttc_util.c
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
