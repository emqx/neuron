# Neuron project cmake file

cmake_minimum_required(VERSION 3.12)
project(neuron)

enable_language(C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_C_FLAGS "$ENV{CFLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O2")
if(APPLE)
set(CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS} -g -O0 -fsanitize=address \
  -fsanitize-recover=address -fsanitize-address-use-after-scope \
  -fno-stack-protector -fno-omit-frame-pointer -fno-var-tracking -static-libsan"
)
else()
set(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_C_FLAGS} -g -O0 -fsanitize=address \
  -fsanitize-recover=address -fsanitize-address-use-after-scope \
  -fno-stack-protector -fno-omit-frame-pointer -fno-var-tracking -static-libasan"
)
endif()

if(NOT DEFINED CMAKE_SUPPRESS_DEVELOPER_WARNINGS)
  set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS
      1
      CACHE INTERNAL "No dev warnings")
endif()

add_subdirectory(extern/vector)
add_subdirectory(extern/libcsptr)
configure_file(extern/vector/vector.h ${CMAKE_SOURCE_DIR}/include/vector.h
               COPYONLY)

# Call this from your own project's makefile.

find_package(nng CONFIG REQUIRED)
find_package(Threads)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_definitions(-DNEU_PLATFORM_LINUX)

elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  add_definitions(-DNEU_PLATFORM_DARWIN)

elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DNEU_PLATFORM_WINDOWS)

endif()

# define neuron executalbe program
set(NEURON_SOURCES
    src/main.c
    src/core/message.c
    src/core/databuf.c
    src/core/neu_manager.c
    src/core/plugin_manager.c
    src/core/neu_datatag_manager.c
    src/core/neu_datatag_table.c
    src/adapter/adapter.c
    src/adapter/neu_webserver.c)

set(NEURON_INCLUDE_DIRECTORIES include src)
add_executable(neuron)
target_sources(neuron PRIVATE ${NEURON_SOURCES})
target_include_directories(neuron PRIVATE ${NEURON_INCLUDE_DIRECTORIES})
if(APPLE)
  target_link_libraries(neuron nng::nng neuron-base)
else()
  target_link_libraries(neuron dl nng jansson neuron-base)
endif()
target_link_libraries(neuron ${CMAKE_THREAD_LIBS_INIT})

# define neuron base library
set(NEURON_BASE_SOURCES
    src/types.c
    src/utils/atomic_data.c
    src/utils/mem_alloc.c
    src/utils/idhash.c
    src/utils/panic.c
    src/utils/log.c
    src/utils/json.c
    src/utils/common.c)

set(NEURON_BASE_INCLUDE_DIRECTORIES include)
add_library(neuron-base SHARED)
target_sources(neuron-base PRIVATE ${NEURON_BASE_SOURCES})
target_include_directories(neuron-base
                           PRIVATE ${NEURON_BASE_INCLUDE_DIRECTORIES})
target_link_libraries(neuron-base vector-static jansson ${CMAKE_THREAD_LIBS_INIT})

# define for plugins
set(SAMPLE_PLUGIN_SOURCES plugins/plugin_sample.c)
add_library(plugin-sample SHARED)
target_sources(plugin-sample PRIVATE ${SAMPLE_PLUGIN_SOURCES})
target_include_directories(plugin-sample PRIVATE include)
target_link_libraries(plugin-sample neuron-base)
target_link_libraries(plugin-sample ${CMAKE_THREAD_LIBS_INIT})
