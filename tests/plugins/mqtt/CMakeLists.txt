include_directories(../../../include/)
include_directories(../../../src/utils/)
include_directories(../../../plugins/mqtt)
file(GLOB MQTT_SRC "../../../plugins/mqtt/*.c")
set(src ${MQTT_SRC} paho_client_test.cc)
add_executable(paho_client_test ${src})
target_link_libraries(paho_client_test neuron-base gtest gtest_main paho-mqtt3as ssl crypto)

# add_test(
#     NAME 
#     paho_client_test 
#     COMMAND  
#     ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/tests/plugins/mqtt/paho_client_test)

include(GoogleTest)
gtest_discover_tests(paho_client_test)