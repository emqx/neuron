import pytest
import neuron.api as api
from neuron.config import *
from neuron.common import (
    description,
    random_port,
)
import neuron.process as process
import neuron.config as config
from neuron.error import *
import shutil
import os
from neuron.mqtt import Mock
import time

mqtt_node = "mqtt"
upload_topic = "/neuron/test/sub_filter"

modbus_port = random_port()
modbus_node = "modbus_tcp"

hold_bit = [{"name": "hold_bit", "address": "1!400001.15",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_int16 = [{"name": "hold_int16", "address": "1!400001",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_INT16}]
hold_uint16 = [{"name": "hold_uint16", "address": "1!400002",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_UINT16}]
hold_int32 = [{"name": "hold_int32", "address": "1!400003",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_INT32}]
hold_uint32 = [{"name": "hold_uint32", "address": "1!400015",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_UINT32}]
hold_float = [{"name": "hold_float", "address": "1!400017",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_FLOAT}]
hold_string = [{"name": "hold_string", "address": "1!400020.10",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_STRING}]


@pytest.fixture(autouse=True, scope="class")
def mocker():
    mock = Mock()
    yield mock
    mock.done()

class TestLaunch:

    @description(
        given="sub_filter_error(command line)",
        when="start neuron",
        then="should make sense",
    )
    def test_sub_filter_error_command(self, mocker):
        process.remove_persistence()

        p = process.NeuronProcess()
        p.start_sub_filter_err()

        api.add_node(node=mqtt_node, plugin=config.PLUGIN_MQTT)
        api.mqtt_node_setting(node=mqtt_node, client_id=mocker.rand_id(), host="127.0.0.1", port=mocker.port)

        api.add_node(node=modbus_node, plugin=config.PLUGIN_MODBUS_TCP)
        api.modbus_tcp_node_setting(node=modbus_node, port=modbus_port)
        api.add_group(node=modbus_node, group='group')
        api.add_tags_check(node=modbus_node, group='group', tags=hold_bit)
        api.add_tags_check(node=modbus_node, group='group', tags=hold_int16)
        api.add_tags_check(node=modbus_node, group='group', tags=hold_uint16)
        api.add_tags_check(node=modbus_node, group='group', tags=hold_int32)
        api.add_tags_check(node=modbus_node, group='group', tags=hold_uint32)
        api.add_tags_check(node=modbus_node, group='group', tags=hold_float)
        api.add_tags_check(node=modbus_node, group='group', tags=hold_string)

        api.subscribe_group(app=mqtt_node, driver=modbus_node, group='group', params={"topic":upload_topic})

        msg = mocker.get(upload_topic, timeout=3)
        assert msg is None

        p_s = process.start_simulator(
            ["./modbus_simulator", "tcp", f"{modbus_port}", "ip_v4"]
        )

        time.sleep(5)

        msg = mocker.get(upload_topic, timeout=3)
        assert msg is None        

        p.stop()
        process.stop_simulator(p_s)

    @description(
        given="sub_filter_error(environment variable)",
        when="start neuron",
        then="should make sense",
    )
    def test_sub_filter_error_environment(self, mocker):

        os.environ['NEURON_SUB_FILTER_ERROR'] = '1'

        p = process.NeuronProcess()
        p.start()

        msg = mocker.get(upload_topic, timeout=3)
        assert msg is None

        p_s = process.start_simulator(
            ["./modbus_simulator", "tcp", f"{modbus_port}", "ip_v4"]
        )

        time.sleep(5)

        msg = mocker.get(upload_topic, timeout=3)
        assert msg is None        

        del os.environ['NEURON_SUB_FILTER_ERROR']

        p.stop()
        process.stop_simulator(p_s)

    @description(
        given="sub_filter_error(configuration file)",
        when="start neuron",
        then="should make sense",
    )
    def test_sub_filter_error_file(self, mocker):
        dst_file = './build/config/neuron.json'
        dst_backup = dst_file + '_backup'
        os.rename(dst_file, dst_backup)
        try:
            shutil.copy2('./tests/ft/login/sub_filter_error.json', dst_file)
        except Exception as e:
            if os.path.exists(dst_backup):
                os.rename(dst_backup, dst_file)
            return

        p = process.NeuronProcess()
        p.start()

        msg = mocker.get(upload_topic, timeout=3)
        assert msg is None

        p_s = process.start_simulator(
            ["./modbus_simulator", "tcp", f"{modbus_port}", "ip_v4"]
        )

        time.sleep(5)

        msg = mocker.get(upload_topic, timeout=3)
        assert msg is None        

        p.stop()
        process.stop_simulator(p_s)

        os.remove(dst_file)
        os.rename(dst_backup, dst_file)


    @description(given="encironment variables 0, configuration file 0 , command line 1",
                 when="start neuron",
                 then="command line has the highest priority")
    def test_priority_command_line(self):
        process.remove_persistence()

        os.environ['NEURON_DISABLE_AUTH'] = '0'

        p = process.NeuronProcess()
        p.start_disable_auth()

        response = api.get_nodes_disable_auth(NEU_NODE_DRIVER)
        assert 200 == response.status_code

        p.stop()

        del os.environ['NEURON_DISABLE_AUTH']

    @description(given="encironment variables 1, configuration file 0 , command line 0",
                 when="start neuron",
                 then="encironment variables has the second priority")
    def test_priority_environment_variable(self):
        process.remove_persistence()

        os.environ['NEURON_DISABLE_AUTH'] = '1'

        p = process.NeuronProcess()
        p.start()

        response = api.get_nodes_disable_auth(NEU_NODE_DRIVER)
        assert 200 == response.status_code

        p.stop()

        del os.environ['NEURON_DISABLE_AUTH']

    @description(given="encironment variables 0, configuration file 1 , command line 0",
                 when="start neuron",
                 then="configuration file has the third priority")
    def test_priority_configuration_file(self):
        process.remove_persistence()

        dst_file = './build/config/neuron.json'
        dst_backup = dst_file + '_backup'
        os.rename(dst_file, dst_backup)
        try:
            shutil.copy2('./tests/ft/login/neuron.json', dst_file)
        except Exception as e:
            if os.path.exists(dst_backup):
                os.rename(dst_backup, dst_file)
            return

        p = process.NeuronProcess()
        p.start()

        response = api.get_nodes_disable_auth(NEU_NODE_DRIVER)
        assert 200 == response.status_code

        p.stop()

        os.remove(dst_file)
        os.rename(dst_backup, dst_file)

    @description(given="encironment variables 1, command line 0",
                 when="start neuron",
                 then="log level set right")
    def test_log_level_environment(self):
        process.remove_persistence()

        os.environ['NEURON_LOG_LEVEL'] = 'DEBUG'

        p = process.NeuronProcess()
        p.start()

        response = api.get_nodes_state(node="")
        assert 200 == response.status_code
        assert "debug" == response.json()["neuron_core"]

        p.stop()

        del os.environ['NEURON_LOG_LEVEL']

    @description(given="encironment variables 1, command line 1",
                 when="start neuron",
                 then="log level set right")
    def test_log_level_environment_command_line(self):
        process.remove_persistence()

        os.environ['NEURON_LOG_LEVEL'] = 'NOTICE'

        p = process.NeuronProcess()
        p.start_debug()

        response = api.get_nodes_state(node="")
        assert 200 == response.status_code
        assert "debug" == response.json()["neuron_core"]

        p.stop()

        del os.environ['NEURON_LOG_LEVEL']