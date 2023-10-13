set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/tests/plugins")

set(CMAKE_BUILD_RPATH ./)
file(COPY ${CMAKE_SOURCE_DIR}/tests/plugins/sc1/sc1.json DESTINATION ${CMAKE_BINARY_DIR}/tests/plugins/schema/)


# v1
set(PLUGIN_NAME plugin-sc1)
set(PLUGIN_SOURCES sc1.c )
add_library(${PLUGIN_NAME} SHARED)
target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_sources(${PLUGIN_NAME} PRIVATE ${PLUGIN_SOURCES})
target_link_libraries(${PLUGIN_NAME} neuron-base)

# v2
set(PLUGIN_NAME_2 plugin-sc1_2)
set(PLUGIN_SOURCES_2 sc1_2.c )
add_library(${PLUGIN_NAME_2} SHARED)
target_include_directories(${PLUGIN_NAME_2} PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_sources(${PLUGIN_NAME_2} PRIVATE ${PLUGIN_SOURCES_2})
target_link_libraries(${PLUGIN_NAME_2} neuron-base)
