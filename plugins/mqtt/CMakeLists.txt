find_package(eclipse-paho-mqtt-c 1.3.9 REQUIRED)

set(src mqtt_client.c)
add_library(neuron_n_pahomq SHARED ${src})
target_link_libraries(neuron_n_pahomq paho-mqtt3as)