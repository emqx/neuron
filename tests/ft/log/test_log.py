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

        response = api.change_log_level(node='modbus-tcp-1', level='debug')
        assert 200 == response.status_code
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "debug" == response.json()['log_level']

    @description(given="node", when="change log_level to info", then="change success")
    def test_change_log_level_info(self):
        response = api.change_log_level(node='modbus-tcp-1', level='info')
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "info" == response.json()['log_level']

    @description(given="node", when="change log_level to warn", then="change success")
    def test_change_log_level_warn(self):
        response = api.change_log_level(node='modbus-tcp-1', level='warn')
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "warn" == response.json()['log_level']

    @description(given="node", when="change log_level to error", then="change success")
    def test_change_log_level_error(self):
        response = api.change_log_level(node='modbus-tcp-1', level='error')
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "error" == response.json()['log_level']

    @description(given="node", when="change log_level to fatal", then="change success")
    def test_change_log_level_fatal(self):
        response = api.change_log_level(node='modbus-tcp-1', level='fatal')
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "fatal" == response.json()['log_level']

    @description(given="node", when="change log_level to notice", then="change success")
    def test_change_log_level_notice(self):
        response = api.change_log_level(node='modbus-tcp-1', level='notice')
        assert 0 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "notice" == response.json()['log_level']

    @description(given="node", when="change log_level to invalid level", then="change failed")
    def test_change_log_level_invalid(self):
        response = api.change_log_level(node='modbus-tcp-1', level='invalid')
        assert 400 == response.status_code
        assert 1003 == response.json()['error']

        response = api.get_nodes_state('modbus-tcp-1')
        assert 200 == response.status_code
        assert "notice" == response.json()['log_level']
