# MQTT Plugin
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

file(COPY ${CMAKE_SOURCE_DIR}/plugins/mqtt/mqtt.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)
set(src 
	mqtt.c 
	util.c
	command/ping.c
	command/read_write.c
	command/command.c)

add_library(plugin-mqtt SHARED ${src})

target_include_directories(plugin-mqtt PRIVATE 
	${CMAKE_SOURCE_DIR}/include/neuron)

target_link_libraries(plugin-mqtt neuron-base)
target_link_libraries(plugin-mqtt ${CMAKE_THREAD_LIBS_INIT})
