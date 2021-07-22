# Neuron project cmake file

cmake_minimum_required (VERSION 3.12)
project (neuron)

ENABLE_LANGUAGE(C)

add_subdirectory(extern/vector)
configure_file(extern/vector/vector.h ${CMAKE_SOURCE_DIR}/include/vector.h COPYONLY)

# Call this from your own project's makefile.
find_package(nng CONFIG REQUIRED)
find_package(Threads)

# define neuron executalbe program
set(NEURON_SOURCES src/main.c
		   src/core/message.c
		   src/core/databuf.c
		   src/core/neu_manager.c
		   src/core/plugin_manager.c
		   src/adapter/adapter.c)
set(NEURON_INCLUDE_DIRECTORIES include src deps)
add_executable(neuron)
target_sources(neuron
	PRIVATE ${NEURON_SOURCES})
target_include_directories(neuron
       	PRIVATE ${NEURON_INCLUDE_DIRECTORIES})
if (APPLE)
    target_link_libraries(neuron nng::nng neuron-base)
else()
    target_link_libraries(neuron dl nng neuron-base)
endif()
target_link_libraries(neuron ${CMAKE_THREAD_LIBS_INIT} )

# define neuron base library
set(NEURON_BASE_SOURCES   deps/cJSON.c
	           	  src/types.c
	           	  src/utils/atomic_data.c
	           	  src/utils/panic.c
		   	  src/utils/log.c)
set(NEURON_BASE_INCLUDE_DIRECTORIES include deps)
add_library(neuron-base SHARED)
target_sources(neuron-base
	PRIVATE ${NEURON_BASE_SOURCES})
target_include_directories(neuron-base
	PRIVATE ${NEURON_BASE_INCLUDE_DIRECTORIES})
target_link_libraries(neuron-base vector-static ${CMAKE_THREAD_LIBS_INIT} )

# define for plugins
set(SAMPLE_PLUGIN_SOURCES plugins/plugin_sample.c)
add_library(sample-plugin SHARED)
target_sources(sample-plugin
	PRIVATE ${SAMPLE_PLUGIN_SOURCES})
target_include_directories(sample-plugin
       	PRIVATE include)
target_link_libraries(sample-plugin neuron-base)
target_link_libraries(sample-plugin ${CMAKE_THREAD_LIBS_INIT} )

