# OPC-UA Plugin
find_package(open62541 1.2.2 REQUIRED)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set(src
	plugin_opcua.c
	option.c
	open62541_client.c
	open62541_utils.c
	plugin_handle.c)

add_library(plugin-opcua SHARED ${src})

target_include_directories(plugin-opcua PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include)

target_link_libraries(plugin-opcua neuron-base open62541 ssl crypto yaml)
if(APPLE)
target_link_libraries(plugin-opcua nng::nng ${CMAKE_THREAD_LIBS_INIT})
else()
target_link_libraries(plugin-opcua nng ${CMAKE_THREAD_LIBS_INIT})
endif()
