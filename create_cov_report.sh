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
        rm -f cov_report/cov-neuron.info cov_report/cov-restful.info
        for dir in build/tests/ut/CMakeFiles/*_test.dir; do
            test_name=$(basename ${dir} "_test.dir")
            lcov -c -d ${dir} -o "cov_report/cov-${test_name}_test.info"
        done
        cd cov_report
        lcov $(for file in cov-*.info; do echo "-a $file "; done) -a cov-neuron-base.info -o cov-ut.info;;
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
    (azure)
        lcov -c -d build/plugins/mqtt/CMakeFiles/plugin-azure-iot.dir -o cov_report/cov-azure.info
        cd cov_report
        gen_trace -a cov-azure.info -o cov-azure.info;;
    (core)
        cd cov_report
        gen_trace -o cov-core.info;;
    (metrics)
        cd cov_report
        gen_trace -o cov-metrics.info;;
    (*)
        lcov -c -d build/plugins/$1/CMakeFiles/plugin-$1.dir -o cov_report/cov-$1.info;;
esac
