import neuron.api as api
from neuron.config import *
from neuron.common import description
import neuron.process as process
from neuron.error import *
import shutil
import os


class TestLaunch:

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