set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

set(MODBUS_SRC modbus.c modbus_point.c modbus_req.c modbus_stack.c)

set(CMAKE_BUILD_RPATH ./)
file(COPY ${CMAKE_SOURCE_DIR}/plugins/modbus/modbus-tcp.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)
file(COPY ${CMAKE_SOURCE_DIR}/plugins/modbus/modbus-rtu.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)
# modbus rtu plugin
set(MODBUS_RTU_PLUGIN plugin-modbus-rtu)
set(MODBUS_RTU_PLUGIN_SOURCES  modbus_rtu.c
                               ${MODBUS_SRC})
add_library(${MODBUS_RTU_PLUGIN} SHARED)
target_include_directories(${MODBUS_RTU_PLUGIN} PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_sources(${MODBUS_RTU_PLUGIN} PRIVATE ${MODBUS_RTU_PLUGIN_SOURCES})
target_link_libraries(${MODBUS_RTU_PLUGIN} neuron-base)

# modbus tcp plugin
set(MODBUS_TCP_PLUS_PLUGIN plugin-modbus-tcp)
set(MODBUS_TCP_PLUS_PLUGIN_SOURCES  modbus_tcp.c
                                    ${MODBUS_SRC})
add_library(${MODBUS_TCP_PLUS_PLUGIN} SHARED)
target_include_directories(${MODBUS_TCP_PLUS_PLUGIN} PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_sources(${MODBUS_TCP_PLUS_PLUGIN} PRIVATE ${MODBUS_TCP_PLUS_PLUGIN_SOURCES})
target_link_libraries(${MODBUS_TCP_PLUS_PLUGIN} neuron-base)

