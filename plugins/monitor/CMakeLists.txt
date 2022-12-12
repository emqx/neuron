# neuron monitor plugin
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

set(src
    monitor.c
    metric_handle.c
)

add_library(plugin-monitor SHARED ${src})

target_include_directories(plugin-monitor PRIVATE
  ${CMAKE_SOURCE_DIR}/include/neuron
)

target_link_libraries(plugin-monitor neuron-base nng)
target_link_libraries(plugin-monitor ${CMAKE_THREAD_LIBS_INIT})
