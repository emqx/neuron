# sample driver plugin
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

set(SAMPLE_PLUGIN_SOURCES plugin_sample_drv.c)
add_library(plugin-sample-drv SHARED)
target_sources(plugin-sample-drv PRIVATE ${SAMPLE_PLUGIN_SOURCES})
target_include_directories(plugin-sample-drv PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(plugin-sample-drv neuron-base dl)
target_link_libraries(plugin-sample-drv ${CMAKE_THREAD_LIBS_INIT})

# sample application plugin
set(SAMPLE_PLUGIN_SOURCES plugin_sample_app.c)
add_library(plugin-sample-app SHARED)
target_sources(plugin-sample-app PRIVATE ${SAMPLE_PLUGIN_SOURCES})
target_include_directories(plugin-sample-app PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(plugin-sample-app neuron-base dl)
target_link_libraries(plugin-sample-app ${CMAKE_THREAD_LIBS_INIT})
