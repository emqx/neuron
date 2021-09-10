find_package(open62541 1.2.2 REQUIRED)

file(GLOB PARSER_SRC "${CMAKE_SOURCE_DIR}/src/parser/*.c")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(src ${PARSER_SRC} plugin_opcua.c open62541_client.h open62541_client.c plugin_handle.h plugin_handle.c)
add_library(plugin-opcua SHARED ${src})

target_include_directories(plugin-opcua PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(plugin-opcua neuron-base open62541 ssl crypto)
target_link_libraries(plugin-opcua ${CMAKE_THREAD_LIBS_INIT})
