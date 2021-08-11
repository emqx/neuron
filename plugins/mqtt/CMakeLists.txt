find_package(eclipse-paho-mqtt-c 1.3.9 REQUIRED)

include_directories(../../include/)
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(src plugin_mqtt.c paho_client.c)
add_library(plugin-mqtt SHARED ${src})
target_link_libraries(plugin-mqtt neuron-base paho-mqtt3as ssl crypto)
target_link_libraries(plugin-mqtt ${CMAKE_THREAD_LIBS_INIT})
