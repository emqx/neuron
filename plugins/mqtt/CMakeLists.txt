cmake_minimum_required(VERSION 3.12)

project(plugin-mqtt)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

file(COPY ${CMAKE_SOURCE_DIR}/plugins/mqtt/mqtt.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)

add_library(${PROJECT_NAME} SHARED
  mqtt_config.c
  mqtt_handle.c
  mqtt_plugin.c
  mqtt_plugin_intf.c
  schema.c
  ptformat.pb-c.c
)

target_include_directories(${PROJECT_NAME} PRIVATE 
  ${CMAKE_SOURCE_DIR}/include/neuron
  ${CMAKE_SOURCE_DIR}/plugins/mqtt
)

target_link_libraries(${PROJECT_NAME} neuron-base)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

file(COPY ${CMAKE_SOURCE_DIR}/plugins/mqtt/aws-iot.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)

set(AWS_PLUGIN "plugin-aws-iot")

add_library(${AWS_PLUGIN} SHARED
  mqtt_config.c
  mqtt_handle.c
  mqtt_plugin_intf.c
  aws_iot_plugin.c
  schema.c
  ptformat.pb-c.c
)

target_include_directories(${AWS_PLUGIN} PRIVATE 
  ${CMAKE_SOURCE_DIR}/include/neuron
  ${CMAKE_SOURCE_DIR}/plugins/mqtt
)

target_link_libraries(${AWS_PLUGIN} neuron-base)
target_link_libraries(${AWS_PLUGIN} ${CMAKE_THREAD_LIBS_INIT})

file(COPY ${CMAKE_SOURCE_DIR}/plugins/mqtt/azure-iot.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)

set(AZURE_PLUGIN "plugin-azure-iot")

add_library(${AZURE_PLUGIN} SHARED
  mqtt_config.c
  mqtt_handle.c
  mqtt_plugin_intf.c
  azure_iot_plugin.c
  schema.c
  ptformat.pb-c.c
)

target_include_directories(${AZURE_PLUGIN} PRIVATE 
  ${CMAKE_SOURCE_DIR}/include/neuron
  ${CMAKE_SOURCE_DIR}/plugins/mqtt
)

target_link_libraries(${AZURE_PLUGIN} neuron-base)
target_link_libraries(${AZURE_PLUGIN} ${CMAKE_THREAD_LIBS_INIT})
