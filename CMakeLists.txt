# Neuron project cmake file

cmake_minimum_required(VERSION 3.12)
project(neuron)

enable_language(C)
set(CMAKE_C_STANDARD 11)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "$ENV{CFLAGS} -Wall -Werror -Wextra -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O2")
set(CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_FLAGS} -O0 -fsanitize=address \
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

configure_file(extern/uthash/include/uthash.h ${CMAKE_SOURCE_DIR}/include/neuron/utils/uthash.h)
configure_file(extern/uthash/include/utarray.h ${CMAKE_SOURCE_DIR}/include/neuron/utils/utarray.h)
configure_file(extern/uthash/include/utlist.h ${CMAKE_SOURCE_DIR}/include/neuron/utils/utlist.h)

add_subdirectory(extern/vector)
set_property(TARGET vector-static PROPERTY POSITION_INDEPENDENT_CODE ON)
add_subdirectory(extern/libcsptr)
configure_file(extern/vector/vector.h ${CMAKE_SOURCE_DIR}/include/neuron/vector.h
               COPYONLY)
configure_file(extern/libcsptr/include/csptr/smart_ptr.h ${CMAKE_SOURCE_DIR}/include/csptr/smart_ptr.h
               COPYONLY)
configure_file(extern/libcsptr/include/csptr/common.h ${CMAKE_SOURCE_DIR}/include/csptr/common.h
               COPYONLY)
configure_file(extern/libcsptr/include/csptr/smalloc.h ${CMAKE_SOURCE_DIR}/include/csptr/smalloc.h
               COPYONLY)
configure_file(extern/libcsptr/include/csptr/config.h ${CMAKE_SOURCE_DIR}/include/csptr/config.h
               COPYONLY)

# Call this from your own project's makefile.

if(NOT CMAKE_STAGING_PREFIX)
  if(APPLE)
  include_directories(/usr/local/include /usr/local/opt/openssl/include)
  link_directories(/usr/local/lib /usr/local/opt/openssl/lib)
  else()
  link_directories(/usr/local/lib)
  endif()
else()
  set(CMAKE_PREFIX_PATH ${CMAKE_STAGING_PREFIX})
  include_directories(${CMAKE_STAGING_PREFIX}/include ${CMAKE_STAGING_PREFIX}/openssl/include)
  link_directories(${CMAKE_STAGING_PREFIX} ${CMAKE_STAGING_PREFIX}/lib ${CMAKE_STAGING_PREFIX}/lib/${COMPILER_PREFIX} ${CMAKE_STAGING_PREFIX}/openssl/lib)
  message(STATUS ${CMAKE_STAGING_PREFIX})
endif()


find_package(nng CONFIG REQUIRED)
find_package(Threads)

add_definitions(-DPROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
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

# define neuron executalbe program
aux_source_directory(src/parser NEURON_SRC_PARSE)
aux_source_directory(src/schema NEURON_SRC_SCHEMA)
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
    src/config.c
    src/daemon.c
    src/core/message.c
    src/core/databuf.c
    src/core/neu_trans_buf.c
    src/core/neu_manager.c
    src/core/plugin_manager.c
    src/core/neu_datatag_manager.c
    src/adapter/adapter.c
    src/adapter/driver/cache.c
    src/adapter/driver/driver.c
    src/adapter/config/config.c
    src/adapter/control/control.c
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
    plugins/restful/rest.c
    plugins/restful/http.c
    plugins/dummy/dummy.c
    ${PERSIST_SOURCES})

set(CMAKE_BUILD_RPATH ./)
set(NEURON_INCLUDE_DIRECTORIES include include/neuron src plugins)
add_executable(neuron)
target_sources(neuron PRIVATE ${NEURON_SOURCES}) 
target_include_directories(neuron PRIVATE ${NEURON_INCLUDE_DIRECTORIES})
target_include_directories(neuron PUBLIC ${CMAKE_STAGING_PREFIX})
if(APPLE)
  target_link_libraries(neuron nng::nng jansson yaml neuron-base)
else()
  target_link_libraries(neuron dl nng jansson yaml neuron-base)
endif()
target_link_libraries(neuron ssl crypto ${CMAKE_THREAD_LIBS_INIT})

# define neuron base library
set(NEURON_BASE_SOURCES
    src/types.c
    src/base/tag_table.c
    src/base/neu_tag.c
    src/base/neu_datatag_table.c
    src/base/neu_tag_group_config.c
    src/base/neu_plugin_common.c
    src/base/neu_data_expr.c
    src/base/tag_class.c
    src/connection/tcp_client.c
    src/connection/tcp_server.c
    src/connection/connection.c
    src/connection/mqtt_client_intf.c
    src/event/event_linux.c
    src/event/event_unix.c
    src/utils/atomic_data.c
    src/utils/idhash.c
    src/utils/hash_table.c
    src/utils/panic.c
    src/utils/log.c
    src/utils/json.c
    src/utils/neu_jwt.c
    src/utils/file.c
    src/utils/base64.c
    src/config.c)

if(USE_NNG_MQTT)
  message("-- USE_NNG_MQTT is defined")
  add_definitions(-DUSE_NNG_MQTT)
  set(NEURON_BASE_SOURCES 
    ${NEURON_BASE_SOURCES}
      src/connection/mqtt_nng_client.c)
else()
  message("-- USE_NNG_MQTT not defined")
  set(NEURON_BASE_SOURCES 
    ${NEURON_BASE_SOURCES}
      src/connection/mqtt_c_client.c)
endif()

set(NEURON_BASE_INCLUDE_DIRECTORIES include include/neuron src)
add_library(neuron-base SHARED)
target_sources(neuron-base PRIVATE ${NEURON_BASE_SOURCES} ${NEURON_SRC_PARSE} ${NEURON_SRC_SCHEMA})
target_include_directories(neuron-base
                           PRIVATE ${NEURON_BASE_INCLUDE_DIRECTORIES})
target_include_directories(neuron-base PRIVATE ${CMAKE_STAGING_PREFIX})
target_compile_definitions(neuron-base PRIVATE MQTT_USE_BIO)

if(USE_NNG_MQTT)
  target_link_libraries(neuron-base mbedtls mbedx509 mbedcrypto nng)
endif()

target_link_libraries(neuron-base libzlog.so vector-static csptr jansson crypto ssl mqttc yaml jwt
                      ${CMAKE_THREAD_LIBS_INIT})

#copy file for run
file(COPY ${CMAKE_SOURCE_DIR}/zlog.conf DESTINATION ${CMAKE_BINARY_DIR}/config)
file(COPY ${CMAKE_SOURCE_DIR}/.gitkeep DESTINATION ${CMAKE_BINARY_DIR}/logs)
file(COPY ${CMAKE_SOURCE_DIR}/neuron.yaml DESTINATION ${CMAKE_BINARY_DIR}/config)
file(COPY ${CMAKE_SOURCE_DIR}/plugins/ekuiper/ekuiper.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)
file(COPY ${CMAKE_SOURCE_DIR}/plugins/modbus/modbus-tcp.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)
file(COPY ${CMAKE_SOURCE_DIR}/plugins/mqtt/mqtt.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)
file(COPY ${CMAKE_SOURCE_DIR}/neuron.key DESTINATION ${CMAKE_BINARY_DIR}/config)
file(COPY ${CMAKE_SOURCE_DIR}/neuron.pem DESTINATION ${CMAKE_BINARY_DIR}/config)

# define for plugins
include_directories(include/neuron)
add_subdirectory(plugins/sample)
add_subdirectory(plugins/modbus)
add_subdirectory(plugins/mqtt)
add_subdirectory(plugins/ekuiper)

# define for simulator
add_subdirectory(simulator)

if(NOT DISABLE_UT)
  # define for tests
  enable_testing()
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
#install(FILES "${CMAKE_SOURCE_DIR}/neuron.conf" DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/ld.so.conf.d)
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
