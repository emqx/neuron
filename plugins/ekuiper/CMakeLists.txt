# LF Edge eKuiper plugin
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

set(src
  json_rw.c
  read_write.c
  plugin_ekuiper.c)

add_library(plugin-ekuiper SHARED ${src})

target_include_directories(plugin-ekuiper PRIVATE 
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(plugin-ekuiper neuron-base nng)
target_link_libraries(plugin-ekuiper ${CMAKE_THREAD_LIBS_INIT})
