cmake_minimum_required(VERSION 3.12)

project(plugin-datalayers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/plugins")

file(COPY ${CMAKE_SOURCE_DIR}/plugins/datalayers/datalayers.json DESTINATION ${CMAKE_BINARY_DIR}/plugins/schema/)

add_library(${PROJECT_NAME} SHARED
  datalayers_config.c
  datalayers_handle.c
  datalayers_plugin.c
  datalayers_plugin_intf.c
  ${CMAKE_SOURCE_DIR}/src/persist/datalayers/flight_sql_client.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE 
  ${CMAKE_SOURCE_DIR}/include/neuron
  ${CMAKE_SOURCE_DIR}/plugins/datalayers
  ${CMAKE_SOURCE_DIR}/src/persist/datalayers
)

find_package(Arrow REQUIRED)
find_package(ArrowFlight REQUIRED)
find_package(ArrowFlightSql REQUIRED)

target_link_libraries(${PROJECT_NAME} 
  Arrow::arrow_shared
  ArrowFlight::arrow_flight_shared
  ArrowFlightSql::arrow_flight_sql_shared
  stdc++
  gflags
  ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(${PROJECT_NAME} neuron-base)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})