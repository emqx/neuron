find_package(eclipse-paho-mqtt-c 1.3.9 REQUIRED)

# include_directories(../../include/)

file(GLOB PARSER_SRC "${CMAKE_SOURCE_DIR}/src/parser/*.c")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(src ${PARSER_SRC} plugin_mqtt.c paho_client.c message_handle.c)
add_library(plugin-mqtt SHARED ${src})

target_include_directories(plugin-mqtt PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(plugin-mqtt neuron-base paho-mqtt3as ssl crypto)
target_link_libraries(plugin-mqtt ${CMAKE_THREAD_LIBS_INIT})
