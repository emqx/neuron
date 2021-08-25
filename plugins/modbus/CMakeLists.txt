# modbus tcp driver plugin
set(MODBUS_TCP_PLUGIN plugin-modbus-tcp)
set(MODBUS_TCP_PLUGIN_SOURCES modbus.c modbus_tcp.c modbus_point.c)
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
add_library(${MODBUS_TCP_PLUGIN} SHARED)
target_sources(${MODBUS_TCP_PLUGIN} PRIVATE ${MODBUS_TCP_PLUGIN_SOURCES})
target_include_directories(${MODBUS_TCP_PLUGIN} PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(${MODBUS_TCP_PLUGIN} neuron-base)
target_link_libraries(${MODBUS_TCP_PLUGIN} ${CMAKE_THREAD_LIBS_INIT})