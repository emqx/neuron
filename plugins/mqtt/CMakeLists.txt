# MQTT Plugin
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

set(src 
	mqtt.c 
	util.c
	command/ping.c
	command/read_write.c
	command/command.c)

add_library(plugin-mqtt SHARED ${src})

target_include_directories(plugin-mqtt PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include)

target_link_libraries(plugin-mqtt neuron-base)
target_link_libraries(plugin-mqtt ${CMAKE_THREAD_LIBS_INIT})
