set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/tests/plugins")

set(CMAKE_BUILD_RPATH ./)
file(COPY ${CMAKE_SOURCE_DIR}/tests/plugins/demo_app/demo_app.json DESTINATION ${CMAKE_BINARY_DIR}/tests/plugins/schema/)


set(PLUGIN_NAME plugin-demo_app)
set(PLUGIN_SOURCES demo_app_plugin.c )
add_library(${PLUGIN_NAME} SHARED)
target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_sources(${PLUGIN_NAME} PRIVATE ${PLUGIN_SOURCES})
target_link_libraries(${PLUGIN_NAME} neuron-base)