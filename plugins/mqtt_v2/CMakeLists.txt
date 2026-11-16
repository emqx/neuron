cmake_minimum_required(VERSION 3.12)

project(plugin-mqtt-v2)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

file(COPY ${CMAKE_SOURCE_DIR}/plugins/mqtt_v2/mqtt-v2.json
     DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)

add_library(${PROJECT_NAME} SHARED
  mqtt_v2_config.c
  mqtt_v2_device.c
  mqtt_v2_handle.c
  mqtt_v2_plugin.c
  mqtt_v2_plugin_intf.c
  mqtt_v2_template.c
  mqtt_v2_write.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/include/neuron
  ${CMAKE_SOURCE_DIR}/plugins/mqtt_v2
)

target_link_libraries(${PROJECT_NAME} neuron-base)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
