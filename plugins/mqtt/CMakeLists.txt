# MQTT Plugin
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set(src 
	plugin_mqtt.c 
	mqtt_util.c
	mqtt_c_client.c
	command/common.c 
	command/read_write.c
	command/group_config.c
	command/datatag.c
	command/node.c
	command/ping.c
	command/plugin.c
	command/command.c)

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

install(TARGETS plugin-mqtt
	LIBRARY DESTINATION lib/neuron)
