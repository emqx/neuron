# LF Edge eKuiper plugin
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

file(COPY ${CMAKE_SOURCE_DIR}/plugins/ekuiper/ekuiper.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)
set(src
  json_rw.c
  read_write.c
  plugin_ekuiper.c)

add_library(plugin-ekuiper SHARED ${src})

target_include_directories(plugin-ekuiper PRIVATE 
  ${CMAKE_SOURCE_DIR}/include/neuron)

target_link_libraries(plugin-ekuiper neuron-base nng)
target_link_libraries(plugin-ekuiper ${CMAKE_THREAD_LIBS_INIT})
