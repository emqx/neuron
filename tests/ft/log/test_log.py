import os
import sys

import neuron.api as api
from neuron.config import *
from neuron.common import *


class TestLog:

    @description(given="node", when="change log_level to debug", then="change success")
    def test_change_log_level_debug(self):
        response = api.add_node(node='modbus-tcp-1', plugin=PLUGIN_MODBUS_TCP)
        assert 200 == response.status_code
        assert 0 == response.json()['error']

        response = api.change_log_level(json={"node": 'modbus-tcp-1', "level": 'debug'})
        assert 200 == response.status_code
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "debug" == response.json()['log_level']

    @description(given="node", when="change log_level to info", then="change success")
    def test_change_log_level_info(self):
        response = api.change_log_level(json={"node": 'modbus-tcp-1', "level": 'info'})
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "info" == response.json()['log_level']

    @description(given="node", when="change log_level to warn", then="change success")
    def test_change_log_level_warn(self):
        response = api.change_log_level(json={"node": 'modbus-tcp-1', "level": 'warn'})
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "warn" == response.json()['log_level']

    @description(given="node", when="change log_level to error", then="change success")
    def test_change_log_level_error(self):
        response = api.change_log_level(json={"node": 'modbus-tcp-1', "level": 'error'})
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "error" == response.json()['log_level']

    @description(given="node", when="change log_level to fatal", then="change success")
    def test_change_log_level_fatal(self):
        response = api.change_log_level(json={"node": 'modbus-tcp-1', "level": 'fatal'})
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "fatal" == response.json()['log_level']

    @description(given="node", when="change log_level to notice", then="change success")
    def test_change_log_level_notice(self):
        response = api.change_log_level(json={"node": 'modbus-tcp-1', "level": 'notice'})
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "notice" == response.json()['log_level']

    @description(given="node", when="change log_level to invalid level", then="change failed")
    def test_change_log_level_invalid(self):
        response = api.change_log_level(json={"node": 'modbus-tcp-1', "level": 'invalid'})
        assert 400 == response.status_code
        assert 1003 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "notice" == response.json()['log_level']

    @description(given="core=true, without node", when="change core log_level", then="change success")
    def test_change_core_log_level_only(self):
        response = api.change_log_level(json={"level": 'info', "core": True})
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "info" == response.json()['neuron_core']
        assert "notice" == response.json()['log_level']

    @description(given="core=false, without node", when="change core log_level", then="return error")
    def test_change_log_level_err_param(self):
        response = api.change_log_level(json={"level": 'info', "core": False})
        assert 400 == response.status_code
        assert 1003 == response.json()['error']

    @description(given="without core, node exists", when="change core and node log_level", then="change success")
    def test_change_core_node_log_level_without_core(self):
        response = api.change_log_level(json={"node": 'modbus-tcp-1', "level": 'warn'})
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "warn" == response.json()['neuron_core']
        assert "warn" == response.json()['log_level']

    @description(given="core=true, node exists", when="change core and node log_level", then="change success")
    def test_change_core_node_log_level(self):
        response = api.change_log_level(json={"node": 'modbus-tcp-1', "level": 'notice', "core": True})
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "notice" == response.json()['neuron_core']
        assert "notice" == response.json()['log_level']

    @description(given="core=false, node exists", when="change node log_level", then="change success")
    def test_change_node_log_level_only(self):
        response = api.change_log_level(json={"node": 'modbus-tcp-1', "level": 'info', "core": False})
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "notice" == response.json()['neuron_core']
        assert "info" == response.json()['log_level']

        response = api.get_nodes_state(node='')
        assert 200 == response.status_code
        assert "notice" == response.json()['neuron_core']
    
    @description(given="neuron started", when="get log list", then="success")
    def test_get_log_list(self):
        response = api.get_log_list()
        assert 200 == response.status_code
        print(response.json())
        assert "neuron.log" in [item['file'] for item in response.json()["files"]]

    @description(given="neuron started", when="get log file", then="success")
    def test_get_log_file(self):
        response = api.get_log_file(file_name='neuron.log')
        assert 200 == response.status_code
