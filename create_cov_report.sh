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
