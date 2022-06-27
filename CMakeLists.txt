cmake_minimum_required(VERSION 3.12)
project(neuron)

enable_testing()

enable_language(C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

if(NOT CMAKE_SYSTEM_NAME)
  set(CMAKE_SYSTEM_NAME "Linux")
endif()
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_definitions(-DNEU_PLATFORM_LINUX)

elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  add_definitions(-DNEU_PLATFORM_DARWIN)

elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DNEU_PLATFORM_WINDOWS)

endif()

set(CMAKE_C_FLAGS "$ENV{CFLAGS} -Wall -Werror -Wextra -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O1")
set(CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS} -O0 -fsanitize=address"
)
set(CMAKE_CXX_FLAGS_DEBUG
    "-Wall -g -O0 -fsanitize=address"
)

add_custom_target(neuron-version
  COMMAND ${CMAKE_COMMAND} -P
          ${CMAKE_CURRENT_SOURCE_DIR}/cmake/neuron-version.cmake)

configure_file(extern/uthash/include/uthash.h ${CMAKE_SOURCE_DIR}/include/neuron/utils/uthash.h)
configure_file(extern/uthash/include/utarray.h ${CMAKE_SOURCE_DIR}/include/neuron/utils/utarray.h)
configure_file(extern/uthash/include/utlist.h ${CMAKE_SOURCE_DIR}/include/neuron/utils/utlist.h)

find_package(nng CONFIG REQUIRED)
find_package(Threads)

if (CMAKE_STAGING_PREFIX)
  include_directories(${CMAKE_STAGING_PREFIX}/include)
  link_directories(${CMAKE_STAGING_PREFIX}/lib)
else()
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
endif()

aux_source_directory(src/parser NEURON_SRC_PARSE)
set(NEURON_BASE_SOURCES
    src/base/tag.c
    src/base/neu_plugin_common.c
    src/base/tag_sort.c
    src/base/group.c
    src/connection/connection.c
    src/connection/mqtt_c_client.c
    src/connection/mqtt_client_intf.c
    src/event/event_linux.c
    src/event/event_unix.c
    src/utils/json.c
    src/utils/neu_jwt.c
    src/utils/base64.c
    src/utils/async_queue.c)

add_library(neuron-base SHARED)
target_sources(neuron-base PRIVATE ${NEURON_BASE_SOURCES} ${NEURON_SRC_PARSE}) 
target_include_directories(neuron-base
                           PRIVATE include/neuron src)
target_compile_definitions(neuron-base PRIVATE MQTT_USE_BIO)
target_link_libraries(neuron-base nng libzlog.so jansson crypto ssl mqttc jwt
                      ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(neuron-base neuron-version)

set(PERSIST_SOURCES
    src/persist/persist.c
    src/persist/json/persist_json_adapter.c
    src/persist/json/persist_json_datatags.c
    src/persist/json/persist_json_group_configs.c
    src/persist/json/persist_json_plugin.c
    src/persist/json/persist_json_subs.c)
set(NEURON_SOURCES
    src/main.c
    src/argparse.c
    src/daemon.c
    src/core/manager_internal.c
    src/core/manager.c
    src/core/subscribe.c
    src/core/plugin_manager.c
    src/core/node_manager.c
    src/core/storage.c
    src/adapter/storage.c
    src/adapter/adapter.c
    src/adapter/driver/cache.c
    src/adapter/driver/driver.c
    plugins/restful/handle.c
    plugins/restful/license.c
    plugins/restful/license_handle.c
    plugins/restful/log_handle.c
    plugins/restful/normal_handle.c
    plugins/restful/rw_handle.c
    plugins/restful/adapter_handle.c
    plugins/restful/datatag_handle.c
    plugins/restful/group_config_handle.c
    plugins/restful/plugin_handle.c
    plugins/restful/version_handle.c
    plugins/restful/rest.c
    plugins/restful/http.c
    plugins/restful/websocket.c
    ${PERSIST_SOURCES})

set(CMAKE_BUILD_RPATH ./)
add_executable(neuron)
target_sources(neuron PRIVATE ${NEURON_SOURCES}) 
target_include_directories(neuron PRIVATE include/neuron src plugins)
target_link_libraries(neuron dl neuron-base)

#copy file for run
file(COPY ${CMAKE_SOURCE_DIR}/zlog.conf DESTINATION ${CMAKE_BINARY_DIR}/config)
file(COPY ${CMAKE_SOURCE_DIR}/dev.conf DESTINATION ${CMAKE_BINARY_DIR}/config)
file(COPY ${CMAKE_SOURCE_DIR}/.gitkeep DESTINATION ${CMAKE_BINARY_DIR}/logs)
file(COPY ${CMAKE_SOURCE_DIR}/neuron.key DESTINATION ${CMAKE_BINARY_DIR}/config)
file(COPY ${CMAKE_SOURCE_DIR}/neuron.pem DESTINATION ${CMAKE_BINARY_DIR}/config)
file(COPY ${CMAKE_SOURCE_DIR}/default_plugins.json DESTINATION ${CMAKE_BINARY_DIR}/persistence)
file(RENAME ${CMAKE_BINARY_DIR}/persistence/default_plugins.json ${CMAKE_BINARY_DIR}/persistence/plugins.json)

add_subdirectory(plugins/modbus)
add_subdirectory(plugins/mqtt)
add_subdirectory(plugins/ekuiper)

add_subdirectory(simulator)

if(NOT DISABLE_UT)
  add_subdirectory(tests)
endif()

# Set sane defaults for multi-lib linux systems
include(GNUInstallDirs)
if(UNIX)
	mark_as_advanced(CLEAR
	    CMAKE_INSTALL_BINDIR
		CMAKE_INSTALL_INCLUDEDIR
		CMAKE_INSTALL_LIBDIR
		CMAKE_INSTALL_SYSCONFDIR)
else()
	set(CMAKE_INSTALL_BINDIR bin)
	set(CMAKE_INSTALL_INCLUDEDIR include)
	set(CMAKE_INSTALL_LIBDIR lib)
	set(CMAKE_INSTALL_SYSCONFDIR etc)
endif()

# Should work, but CMake incorrectly prepends /usr to etc...
install(FILES "${CMAKE_SOURCE_DIR}/neuron.conf" DESTINATION etc/ld.so.conf.d)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING
        PATTERN "*.h")
install(FILES "${CMAKE_SOURCE_DIR}/cmake/neuron-config.cmake"
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/neuron)
install(TARGETS neuron neuron-base
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
