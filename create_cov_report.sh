#!/bin/bash

# core trace
lcov -c -d build/CMakeFiles/neuron-base.dir/src -o cov_report/cov-neuron-base.info
lcov -c -d build/CMakeFiles/neuron.dir/src -o cov_report/cov-neuron.info
lcov -c -d build/CMakeFiles/neuron.dir/plugins/restful -o cov_report/cov-restful.info

# always with core trace
function gen_trace () {
    lcov -a cov-neuron-base.info -a cov-neuron.info -a cov-restful.info $@;
}

case $1 in
    (ut)
        lcov -c -d build/tests/ut/CMakeFiles/tag_sort_test.dir -o cov_report/cov-tag_sort_test.info
        lcov -c -d build/tests/ut/CMakeFiles/modbus_test.dir -o cov_report/cov-modbus_test.info
        lcov -c -d build/tests/ut/CMakeFiles/jwt_test.dir -o cov_report/cov-jwt_test.info
        lcov -c -d build/tests/ut/CMakeFiles/json_test.dir -o cov_report/cov-json_test.info
        lcov -c -d build/tests/ut/CMakeFiles/http_test.dir -o cov_report/cov-http_test.info
        lcov -c -d build/tests/ut/CMakeFiles/base64_test.dir -o cov_report/cov-base64_test.info
        cd cov_report
        lcov -a cov-tag_sort_test.info -a cov-modbus_test.info -a cov-jwt_test.info -a cov-json_test.info -a cov-http_test.info -a cov-base64_test.info -o cov-ut.info;;
    (ft)
        lcov -c -d build/plugins/mqtt/CMakeFiles/plugin-mqtt.dir -o cov_report/cov-mqtt.info
        lcov -c -d build/plugins/modbus/CMakeFiles/plugin-modbus-rtu.dir -o cov_report/cov-modbus-rtu.info
        lcov -c -d build/plugins/modbus/CMakeFiles/plugin-modbus-tcp.dir -o cov_report/cov-modbus-tcp.info
        cd cov_report
        gen_trace -a cov-mqtt.info -a cov-modbus-rtu.info -a cov-modbus-tcp.info -o cov-ft.info;;
    (ekuiper)
        lcov -c -d build/plugins/ekuiper/CMakeFiles/plugin-ekuiper.dir -o cov_report/cov-ekuiper.info
        cd cov_report
        gen_trace -a cov-ekuiper.info -o cov-ekuiper.info;;
    (modbus)
        lcov -c -d build/plugins/modbus/CMakeFiles/plugin-modbus-rtu.dir -o cov_report/cov-modbus-rtu.info
        lcov -c -d build/plugins/modbus/CMakeFiles/plugin-modbus-tcp.dir -o cov_report/cov-modbus-tcp.info
        lcov -c -d build/simulator/CMakeFiles/modbus_simulator.dir -o cov_report/cov-simulator-modbus.info
        cd cov_report
        gen_trace -a cov-simulator-modbus.info -a cov-modbus-rtu.info -a cov-modbus-tcp.info -o cov-modbus.info;;
    (mqtt)
        lcov -c -d build/plugins/mqtt/CMakeFiles/plugin-mqtt.dir -o cov_report/cov-mqtt.info
        cd cov_report
        gen_trace -a cov-mqtt.info -o cov-mqtt.info;;
    (core)
        cd cov_report
        gen_trace -o cov-core.info;;
    (metrics)
        cd cov_report
        gen_trace -o cov-metrics.info;;
    (*)
        lcov -c -d build/plugins/$1/CMakeFiles/plugin-$1.dir -o cov_report/cov-$1.info;;
esac
