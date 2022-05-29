set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

set(MODBUS_SRC modbus.c modbus_point.c modbus_req.c modbus_stack.c)

set(CMAKE_BUILD_RPATH ./)
file(COPY ${CMAKE_SOURCE_DIR}/plugins/modbus/modbus-tcp.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)

# modbus rtu plugin
set(MODBUS_TCP_PLUGIN plugin-modbus-tcp)
set(MODBUS_TCP_PLUGIN_SOURCES  modbus_tcp.c
                                    ${MODBUS_SRC})
add_library(${MODBUS_TCP_PLUGIN} SHARED)
target_sources(${MODBUS_TCP_PLUGIN} PRIVATE ${MODBUS_TCP_PLUGIN_SOURCES})
target_link_libraries(${MODBUS_TCP_PLUGIN} neuron-base)