#!/bin/bash


case $1 in
    (core)
        pytest -s -v tests/ft/log
        pytest -s -v tests/ft/login
        pytest -s -v tests/ft/metrics
        pytest -s -v tests/ft/neuron
        pytest -s -v tests/ft/plugin
        pytest -s -v tests/ft/tag
        pytest -s -v tests/ft/driver/test_driver.py;;
    (modbus)
        pytest -s -v tests/ft/driver;;
esac