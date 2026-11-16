set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/tests/plugins")
set(CMAKE_BUILD_RPATH ./)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/auto_type.json
     DESTINATION ${CMAKE_BINARY_DIR}/tests/plugins/schema/)
add_library(plugin-auto-type SHARED auto_type.c)
target_include_directories(plugin-auto-type PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron)
target_link_libraries(plugin-auto-type neuron-base)
