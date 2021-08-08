# Neuron project cmake file

cmake_minimum_required(VERSION 3.12)
project(neuron)

enable_language(C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE "Debug")
# set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -Wall -g -ggdb -fsanitize=address
# -fsanitize=leak")
set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -Wall -g -ggdb")

add_subdirectory(extern/vector)
set_property(TARGET vector-static PROPERTY POSITION_INDEPENDENT_CODE ON)
add_subdirectory(extern/libcsptr)
configure_file(extern/vector/vector.h ${CMAKE_SOURCE_DIR}/include/vector.h
               COPYONLY)

# Call this from your own project's makefile.
find_package(nng CONFIG REQUIRED)
find_package(Threads)

# define neuron executalbe program
set(NEURON_SOURCES
    src/main.c
    src/core/message.c
    src/core/databuf.c
    src/core/neu_manager.c
    src/core/plugin_manager.c
    src/core/datatag_table.c
    src/adapter/adapter.c)

set(NEURON_INCLUDE_DIRECTORIES include src deps)
add_executable(neuron)
target_sources(neuron PRIVATE ${NEURON_SOURCES})
target_include_directories(neuron PRIVATE ${NEURON_INCLUDE_DIRECTORIES})
if(APPLE)
  target_link_libraries(neuron nng::nng neuron-base)
else()
  target_link_libraries(neuron dl nng neuron-base)
endif()
target_link_libraries(neuron ${CMAKE_THREAD_LIBS_INIT})

# define neuron base library
set(NEURON_BASE_SOURCES
    deps/cJSON.c
    src/types.c
    src/utils/atomic_data.c
    src/utils/mem_alloc.c
    src/utils/idhash.c
    src/utils/panic.c
    src/utils/log.c)

set(NEURON_BASE_INCLUDE_DIRECTORIES include deps)
add_library(neuron-base SHARED)
target_sources(neuron-base PRIVATE ${NEURON_BASE_SOURCES})
target_include_directories(neuron-base
                           PRIVATE ${NEURON_BASE_INCLUDE_DIRECTORIES})
target_link_libraries(neuron-base vector-static ${CMAKE_THREAD_LIBS_INIT})

# define for plugins
set(SAMPLE_PLUGIN_SOURCES plugins/plugin_sample.c)
add_library(plugin-sample SHARED)
target_sources(plugin-sample PRIVATE ${SAMPLE_PLUGIN_SOURCES})
target_include_directories(plugin-sample PRIVATE include)
target_link_libraries(plugin-sample neuron-base)
target_link_libraries(plugin-sample ${CMAKE_THREAD_LIBS_INIT})
add_subdirectory(plugins/mqtt)

enable_testing()
add_subdirectory(tests/plugins/mqtt)
