find_package(eclipse-paho-mqtt-c 1.3.9 REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(src mqtt_client.c)
add_library(mqtt-plugin SHARED ${src})
target_link_libraries(mqtt-plugin neuron-base paho-mqtt3as ${CMAKE_THREAD_LIBS_INIT})