# MQTT Plugin
# SparkplugB Plugin
cmake_minimum_required(VERSION 3.12)
project(plugin-mqtt)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

file(COPY ${CMAKE_SOURCE_DIR}/plugins/mqtt/mqtt.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)
set(src 
	mqtt.c 
	util.c
	command/read_write.c
	command/heartbeat.c
	command/command.c)

add_library(${PROJECT_NAME} SHARED ${src})
target_include_directories(${PROJECT_NAME} PRIVATE 
	${CMAKE_SOURCE_DIR}/include/neuron)
target_link_libraries(${PROJECT_NAME} neuron-base)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
