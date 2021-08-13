# Neuron project cmake file

cmake_minimum_required(VERSION 3.12)
project(neuron)

enable_language(C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "$ENV{CFLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O2")
set(CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS} -g -O0 -fsanitize=address \
  -fsanitize-recover=address -fsanitize-address-use-after-scope \
  -fno-stack-protector -fno-omit-frame-pointer -fno-var-tracking"
)
set(CMAKE_CXX_FLAGS_DEBUG
    "-g -O0 -fsanitize=address \
  -fsanitize-recover=address -fsanitize-address-use-after-scope \
  -fno-stack-protector -fno-omit-frame-pointer -fno-var-tracking"
)

if(NOT DEFINED CMAKE_SUPPRESS_DEVELOPER_WARNINGS)
  set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS
      1
      CACHE INTERNAL "No dev warnings")
endif()

add_subdirectory(extern/vector)
set_property(TARGET vector-static PROPERTY POSITION_INDEPENDENT_CODE ON)
add_subdirectory(extern/libcsptr)
configure_file(extern/vector/vector.h ${CMAKE_SOURCE_DIR}/include/vector.h
               COPYONLY)

# Call this from your own project's makefile.

if(APPLE)
include_directories(/usr/local/include)
link_directories(/usr/local/lib /usr/local/opt/openssl/lib)
endif()

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
    src/parser/neu_json_parser.c
    src/parser/neu_json_read.c
    src/parser/neu_json_write.c
    plugins/restful/handle.c
    plugins/restful/rest.c)

set(NEURON_INCLUDE_DIRECTORIES include src plugins)
add_executable(neuron)
target_sources(neuron PRIVATE ${NEURON_SOURCES})
target_include_directories(neuron PRIVATE ${NEURON_INCLUDE_DIRECTORIES})
if(APPLE)
  target_link_libraries(neuron nng::nng jansson neuron-base)
else()
  target_link_libraries(neuron dl nng jansson neuron-base)
endif()
target_link_libraries(neuron ssl crypto ${CMAKE_THREAD_LIBS_INIT})

# define neuron base library
set(NEURON_BASE_SOURCES
    src/types.c
    src/utils/atomic_data.c
    src/utils/mem_alloc.c
    src/utils/idhash.c
    src/utils/list.c
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
# sample driver plugin
set(SAMPLE_PLUGIN_SOURCES plugins/sample/plugin_sample_drv.c)
add_library(plugin-sample-drv SHARED)
target_sources(plugin-sample-drv PRIVATE ${SAMPLE_PLUGIN_SOURCES})
target_include_directories(plugin-sample-drv PRIVATE include)
target_link_libraries(plugin-sample-drv neuron-base)
target_link_libraries(plugin-sample-drv ${CMAKE_THREAD_LIBS_INIT})

# sample application plugin
set(SAMPLE_PLUGIN_SOURCES plugins/sample/plugin_sample_app.c)
add_library(plugin-sample-app SHARED)
target_sources(plugin-sample-app PRIVATE ${SAMPLE_PLUGIN_SOURCES})
target_include_directories(plugin-sample-app PRIVATE include)
target_link_libraries(plugin-sample-app neuron-base)
target_link_libraries(plugin-sample-app ${CMAKE_THREAD_LIBS_INIT})

# mqtt plugin
add_subdirectory(plugins/mqtt)
enable_testing()
add_subdirectory(tests)
#add_subdirectory(tests/plugins/mqtt)
