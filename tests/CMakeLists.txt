add_executable(json_test json_test.cc)
target_include_directories(json_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(json_test neuron-base gtest_main gtest pthread jansson)

aux_source_directory(${CMAKE_SOURCE_DIR}/src/parser SRC_PARSER)
add_executable(api_json_test api_json_test.cc ${SRC_PARSER})
target_include_directories(api_json_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(api_json_test neuron-base gtest_main gtest pthread jansson)

include(GoogleTest)
gtest_discover_tests(json_test)
gtest_discover_tests(api_json_test)
