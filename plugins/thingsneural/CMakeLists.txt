cmake_minimum_required(VERSION 3.12)

project(plugin-thingsneural)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

file(COPY ${CMAKE_SOURCE_DIR}/plugins/thingsneural/thingsneural.json
     DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)

add_library(${PROJECT_NAME} SHARED
    thingsneural_config.c
    thingsneural_handle.c
    thingsneural_plugin.c
    thingsneural_plugin_intf.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include/neuron
    ${CMAKE_SOURCE_DIR}/plugins/thingsneural
)

target_link_libraries(${PROJECT_NAME} neuron-base)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
