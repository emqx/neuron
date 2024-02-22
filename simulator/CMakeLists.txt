cmake_minimum_required(VERSION 3.12)

project(simulator)

if(NOT CMAKE_STAGING_PREFIX)
  if(APPLE)
  include_directories(/usr/local/include /usr/local/opt/openssl/include)
  link_directories(/usr/local/lib /usr/local/opt/openssl/lib)
  else()
  link_directories(/usr/local/lib)
  endif()
else()
  set(CMAKE_PREFIX_PATH ${CMAKE_STAGING_PREFIX})
  include_directories(${CMAKE_STAGING_PREFIX}/openssl/include)
  link_directories(${CMAKE_STAGING_PREFIX}/openssl/lib)
endif()
find_package(Threads)


add_executable(modbus_simulator modbus_simulator.c modbus_s.c)
target_include_directories(modbus_simulator PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron ${CMAKE_SOURCE_DIR})
target_link_libraries(modbus_simulator neuron-base ${CMAKE_THREAD_LIBS_INIT} dl)

add_executable(modbus_tty_simulator modbus_tty_simulator.c modbus_s.c)
target_include_directories(modbus_tty_simulator PRIVATE ${CMAKE_SOURCE_DIR}/include/neuron ${CMAKE_SOURCE_DIR})
target_link_libraries(modbus_tty_simulator neuron-base ${CMAKE_THREAD_LIBS_INIT} dl)