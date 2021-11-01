link_directories(/usr/local/lib)

aux_source_directory(${CMAKE_SOURCE_DIR}/src/parser SRC_PARSER)
add_executable(json_test json_test.cc)
target_include_directories(json_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(json_test neuron-base gtest_main gtest pthread jansson)

add_executable(api_json_test api_json_test.cc ${SRC_PARSER})
target_include_directories(api_json_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(api_json_test neuron-base gtest_main gtest pthread jansson)

add_executable(json_datatag_table_test json_datatag_table_test.cc ${SRC_PARSER})
target_include_directories(json_datatag_table_test PRIVATE
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
)
target_link_libraries(json_datatag_table_test neuron-base gtest_main gtest pthread jansson)

add_executable(json_nodes_test json_nodes_test.cc ${SRC_PARSER})
target_include_directories(json_nodes_test PRIVATE
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
)
target_link_libraries(json_nodes_test neuron-base gtest_main gtest pthread jansson)

add_executable(json_plugin_test json_plugin_test.cc ${SRC_PARSER})
target_include_directories(json_plugin_test PRIVATE
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
)
target_link_libraries(json_plugin_test neuron-base gtest_main gtest pthread jansson)

add_executable(json_group_config_test json_group_config_test.cc ${SRC_PARSER})
target_include_directories(json_group_config_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(json_group_config_test neuron-base gtest_main gtest)

add_executable(variable_test variable_test.cc)
target_include_directories(variable_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(variable_test neuron-base gtest_main gtest)

aux_source_directory(${CMAKE_SOURCE_DIR}/plugins/modbus SRC_MODBUS)
add_executable(modbus_point_test modbus_point_test.cc ${SRC_MODBUS})
target_include_directories(modbus_point_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/plugins
)
target_link_libraries(modbus_point_test neuron-base gtest_main gtest pthread nng)
target_link_directories(modbus_point_test PRIVATE /usr/local/lib)

add_executable(datatag_table_test datatag_table_test.cc ${CMAKE_SOURCE_DIR}/src/base/neu_datatag_table.c)
target_include_directories(datatag_table_test PRIVATE
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
)
target_link_libraries(datatag_table_test neuron-base gtest_main gtest pthread jansson)

add_executable(config_test config_test.cc ${CMAKE_SOURCE_DIR}/src/config.c)
target_include_directories(config_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
)
target_link_libraries(config_test neuron-base gtest_main gtest pthread yaml)
#copy yaml for test
file(COPY ${CMAKE_SOURCE_DIR}/neuron.yaml DESTINATION ${CMAKE_BINARY_DIR}/tests)

add_executable(data_val_test data_val_test.cc ${SRC_PARSER})
target_include_directories(data_val_test PRIVATE
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
)
target_link_libraries(data_val_test neuron-base gtest_main gtest pthread jansson)

aux_source_directory(${CMAKE_SOURCE_DIR}/plugins/restful SRC_RESTFUL)
add_executable(plugin_manager_test plugin_manager_test.cc ${CMAKE_SOURCE_DIR}/src/core/plugin_manager.c ${SRC_RESTFUL} ${SRC_PARSER} ${CMAKE_SOURCE_DIR}/src/config.c)
target_include_directories(plugin_manager_test PRIVATE
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/plugins
)
target_link_libraries(plugin_manager_test neuron-base gtest_main gtest pthread jansson dl nng yaml)

add_executable(uuid_test uuid_test.cc ${CMAKE_SOURCE_DIR}/src/utils/uuid4.c)
target_include_directories(uuid_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
)
target_link_libraries(uuid_test neuron-base gtest_main gtest crypto)

#copy schema for test
file(COPY ${CMAKE_SOURCE_DIR}/plugin_param_schema_test.json DESTINATION ${CMAKE_BINARY_DIR}/tests)
add_executable(schema_valid_test schema_valid_test.cc ${CMAKE_SOURCE_DIR}/src/schema/schema.c)
target_include_directories(schema_valid_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
)
target_link_libraries(schema_valid_test neuron-base gtest_main gtest jansson)

file(COPY ${CMAKE_SOURCE_DIR}/plugin_param_schema.json DESTINATION ${CMAKE_BINARY_DIR}/tests)
add_executable(mqtt_test mqtt_test.cc 
	${CMAKE_SOURCE_DIR}/src/config.c
	${CMAKE_SOURCE_DIR}/src/schema/schema.c
	${CMAKE_SOURCE_DIR}/plugins/mqtt/mqtt_client.c)
target_include_directories(mqtt_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
	${CMAKE_SOURCE_DIR}/src/schema
	${CMAKE_SOURCE_DIR}/plugins/mqtt)
target_link_libraries(mqtt_test neuron-base gtest_main gtest yaml jansson ssl crypto)

add_executable(opcua_test opcua_test.cc 
	${CMAKE_SOURCE_DIR}/src/config.c
	${CMAKE_SOURCE_DIR}/src/schema/schema.c
	${CMAKE_SOURCE_DIR}/plugins/opcua/option.c)
target_include_directories(opcua_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
	${CMAKE_SOURCE_DIR}/src/schema
	${CMAKE_SOURCE_DIR}/plugins/opcua)
target_link_libraries(opcua_test neuron-base gtest_main gtest yaml jansson ssl crypto)

include(GoogleTest)
gtest_discover_tests(data_val_test)
gtest_discover_tests(modbus_point_test)
gtest_discover_tests(config_test)
gtest_discover_tests(plugin_manager_test)
gtest_discover_tests(datatag_table_test)
gtest_discover_tests(json_test)
gtest_discover_tests(api_json_test)
gtest_discover_tests(json_datatag_table_test)
gtest_discover_tests(json_nodes_test)
gtest_discover_tests(json_plugin_test)
gtest_discover_tests(json_group_config_test)
gtest_discover_tests(uuid_test)
gtest_discover_tests(schema_valid_test)
gtest_discover_tests(mqtt_test)
gtest_discover_tests(opcua_test)
