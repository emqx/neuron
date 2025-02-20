set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

set(CMAKE_BUILD_RPATH ./)
file(COPY ${CMAKE_SOURCE_DIR}/plugins/file/file.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)

set(PLUGIN_NAME plugin-file)
set(PLUGIN_SOURCES file_plugin.c)
add_library(${PLUGIN_NAME} SHARED)
target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_sources(${PLUGIN_NAME} PRIVATE ${PLUGIN_SOURCES})
target_link_libraries(${PLUGIN_NAME} neuron-base)