#!/bin/bash


case $1 in
    (modbus)
        lcov -c -d build/plugins/modbus/CMakeFiles/plugin-modbus-rtu.dir -o cov_report/cov-modbus-rtu.info
        lcov -c -d build/plugins/modbus/CMakeFiles/plugin-modbus-tcp.dir -o cov_report/cov-modbus-tcp.info
        cd cov_report
        lcov -a cov-modbus-rtu.info -a cov-modbus-tcp.info -o cov-modbus.info;;
    (core)
        lcov -c -d build/CMakeFiles/neuron-base.dir/src -o cov_report/cov-neuron-base.info
        lcov -c -d build/CMakeFiles/neuron.dir/src -o cov_report/cov-neuron.info
        lcov -c -d build/CMakeFiles/neuron.dir/plugins/restful -o cov_report/cov-restful.info
        cd cov_report
        lcov -a cov-neuron-base.info -a cov-neuron.info -a cov-restful.info -o cov-core.info;;
        (*)
        lcov -c -d build/plugins/$1/CMakeFiles/plugin-$1.dir -o cov_report/cov-$1.info;;
esac