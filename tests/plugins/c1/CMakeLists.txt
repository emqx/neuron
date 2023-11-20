set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/tests/plugins")

set(CMAKE_BUILD_RPATH ./)
file(COPY ${CMAKE_SOURCE_DIR}/tests/plugins/c1/c1.json DESTINATION ${CMAKE_BINARY_DIR}/tests/plugins/schema/)


# v1
set(PLUGIN_NAME plugin-c1)
set(PLUGIN_SOURCES c1.c )
add_library(${PLUGIN_NAME} SHARED)
target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_sources(${PLUGIN_NAME} PRIVATE ${PLUGIN_SOURCES})
target_link_libraries(${PLUGIN_NAME} neuron-base)

# v2
set(PLUGIN_NAME_2 plugin-c1_2)
set(PLUGIN_SOURCES_2 c1_2.c )
add_library(${PLUGIN_NAME_2} SHARED)
target_include_directories(${PLUGIN_NAME_2} PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_sources(${PLUGIN_NAME_2} PRIVATE ${PLUGIN_SOURCES_2})
target_link_libraries(${PLUGIN_NAME_2} neuron-base)

# v1_v_err
set(PLUGIN_NAME plugin-c1verr)
set(PLUGIN_SOURCES c1_v_err.c )
add_library(${PLUGIN_NAME} SHARED)
target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_sources(${PLUGIN_NAME} PRIVATE ${PLUGIN_SOURCES})
target_link_libraries(${PLUGIN_NAME} neuron-base)

# v2_v_err
set(PLUGIN_NAME_2 plugin-c1_2verr)
set(PLUGIN_SOURCES_2 c1_2_v_err.c )
add_library(${PLUGIN_NAME_2} SHARED)
target_include_directories(${PLUGIN_NAME_2} PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_sources(${PLUGIN_NAME_2} PRIVATE ${PLUGIN_SOURCES_2})
target_link_libraries(${PLUGIN_NAME_2} neuron-base)
