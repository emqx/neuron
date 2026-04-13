cmake_minimum_required(VERSION 3.12)

project(plugin-kafka)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

file(COPY ${CMAKE_SOURCE_DIR}/plugins/kafka/kafka.json
     DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)

# --- locate librdkafka -------------------------------------------------------
# Prefer pkg-config (works for system install and staging sysroot).
# Falls back to find_library for minimal environments.
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(RDKAFKA IMPORTED_TARGET rdkafka)
endif()

if(NOT RDKAFKA_FOUND)
    find_path(RDKAFKA_INCLUDE_DIR librdkafka/rdkafka.h
              HINTS ${CMAKE_STAGING_PREFIX}/include
                    /usr/local/include
                    /usr/include)
    find_library(RDKAFKA_LIBRARY rdkafka
                 HINTS ${CMAKE_STAGING_PREFIX}/lib
                       /usr/local/lib
                       /usr/lib)
    if(RDKAFKA_INCLUDE_DIR AND RDKAFKA_LIBRARY)
        set(RDKAFKA_FOUND TRUE)
        set(RDKAFKA_INCLUDE_DIRS ${RDKAFKA_INCLUDE_DIR})
        set(RDKAFKA_LIBRARIES    ${RDKAFKA_LIBRARY})
    else()
        message(FATAL_ERROR
            "librdkafka not found. Install librdkafka-dev or set "
            "CMAKE_STAGING_PREFIX / PKG_CONFIG_PATH so that rdkafka.h "
            "and librdkafka.so are discoverable.")
    endif()
endif()

# --- plugin target ------------------------------------------------------------
add_library(${PROJECT_NAME} SHARED
    kafka_config.c
    kafka_handle.c
    kafka_plugin.c
    kafka_plugin_intf.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include/neuron
    ${CMAKE_SOURCE_DIR}/plugins/kafka
)

if(TARGET PkgConfig::RDKAFKA)
    target_link_libraries(${PROJECT_NAME} neuron-base PkgConfig::RDKAFKA)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE ${RDKAFKA_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} neuron-base ${RDKAFKA_LIBRARIES})
endif()

target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

# librdkafka header triggers -Wignored-qualifiers on some versions
target_compile_options(${PROJECT_NAME} PRIVATE -Wno-ignored-qualifiers)
