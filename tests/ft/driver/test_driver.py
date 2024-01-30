import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import *


class TestDriver:

    @description(given="running neuron", when="add driver node", then="add success")
    @pytest.mark.parametrize('node,plugin', [('modbus-tcp-1', config.PLUGIN_MODBUS_TCP),
                                             ('modbus-rtu-1', config.PLUGIN_MODBUS_RTU)])
    def test_create_node(self, node, plugin):
        response = api.add_node(node=node,
                                plugin=plugin)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes(type=1)
        assert 200 == response.status_code

    @description(given="running neuron", when="add driver node with wrong setting", then="should fail")
    @pytest.mark.parametrize('node,plugin', [('modbus-01', config.PLUGIN_MODBUS_TCP)])
    def test_create_node_with_wrong_setting(self, node, plugin):
        params = {"connection_mode": 0}
        response = api.add_node(node=node, plugin=plugin, params=params)
        assert 400 == response.status_code
        assert error.NEU_ERR_NODE_SETTING_INVALID == response.json()['error']

        response = api.get_nodes_state(node)
        assert 404 == response.status_code
        assert error.NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="running neuron", when="add driver node with correct setting", then="should success")
    @pytest.mark.parametrize('node,plugin', [('modbus-01', config.PLUGIN_MODBUS_TCP)])
    def test_create_node_with_correct_setting(self, node, plugin):
        params = {
            "connection_mode": 0,
            "transport_mode": 0,
            "interval": 20,
            "host": "127.0.0.1",
            "port": 502,
            "timeout": 3000,
            "max_retries": 2
        }
        response = api.add_node(node=node, plugin=plugin, params=params)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes_state(node)
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()['running']
        api.del_node(node)

    @description(given="created node", when="get node state", then="state is init")
    @pytest.mark.parametrize('node', ['modbus-tcp-1', 'modbus-rtu-1'])
    def test_node_init(self, node):
        response = api.get_nodes_state(node=node)
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_INIT == response.json()['running']

    @description(given="created node", when="stop node", then="stop failed")
    @pytest.mark.parametrize('node', ['modbus-tcp-1', 'modbus-rtu-1'])
    def test_init_node_stop(self, node):
        response = api.node_ctl(node=node, ctl=config.NEU_CTL_STOP)
        assert 409 == response.status_code

    @description(given="modbus tcp node", when="setting modbus tcp node", then="setting success")
    def test_modbus_tcp_setting(self):
        response = api.modbus_tcp_node_setting(node='modbus-tcp-1', port=502)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

    @description(given="modbus rtu node", when="setting modbus rtu node", then="setting success")
    def test_modbus_rtu_setting(self):
        response = api.modbus_rtu_node_setting(node='modbus-rtu-1', port=502)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

    def test_wait_state(self):
        time.sleep(4)

    @description(given="configured node", when="get node state", then="state is running")
    @pytest.mark.parametrize('node', ['modbus-tcp-1', 'modbus-rtu-1'])
    def test_node_ready(self, node):
        response = api.get_nodes_state(node=node)
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()['running']

    @description(given="running modbus tcp node", when="setting modbus tcp node", then="state not change")
    def test_running_modbus_tcp_setting(self):
        response = api.modbus_tcp_node_setting(node='modbus-tcp-1', port=502)
        assert 200 == response.status_code

        response = api.get_nodes_state(node='modbus-tcp-1')
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()['running']

    @description(given="running modbus rtu node", when="setting modbus rtu node", then="state not change")
    def test_running_modbus_rtu_setting(self):
        response = api.modbus_rtu_node_setting(node='modbus-rtu-1', port=502)
        assert 200 == response.status_code

        response = api.get_nodes_state(node='modbus-tcp-1')
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()['running']

    @description(given="running node", when="stop node", then="state is stopped")
    @pytest.mark.parametrize('node', ['modbus-tcp-1', 'modbus-rtu-1'])
    def test_node_stop(self, node):
        response = api.node_ctl(node=node, ctl=config.NEU_CTL_STOP)
        assert 200 == response.status_code
        response = api.get_nodes_state(node=node)
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_STOPPED == response.json()['running']

    @description(given="stopped modbus tcp node", when="setting modbus tcp node", then="state not change")
    def test_stopped_modbus_tcp_setting(self):
        response = api.modbus_tcp_node_setting(node='modbus-tcp-1', port=502)
        assert 200 == response.status_code

        response = api.get_nodes_state(node='modbus-tcp-1')
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_STOPPED == response.json()['running']

    @description(given="stopped modbus rtu node", when="setting modbus rtu node", then="state not change")
    def test_stopped_modbus_rtu_setting(self):
        response = api.modbus_rtu_node_setting(node='modbus-rtu-1', port=502)
        assert 200 == response.status_code

        response = api.get_nodes_state(node='modbus-tcp-1')
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_STOPPED == response.json()['running']

    @description(given="stopped node", when="stop node", then="stop failed")
    @pytest.mark.parametrize('node', ['modbus-tcp-1', 'modbus-rtu-1'])
    def test_stop_node_stop(self, node):
        response = api.node_ctl(node=node, ctl=config.NEU_CTL_STOP)
        assert 409 == response.status_code

    @description(given="stopped node", when="start node", then="state is running")
    @pytest.mark.parametrize('node', ['modbus-tcp-1', 'modbus-rtu-1'])
    def test_node_start(self, node):
        response = api.node_ctl(node=node, ctl=config.NEU_CTL_START)
        assert 200 == response.status_code
        response = api.get_nodes_state(node=node)
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()['running']

    @description(given="running node", when="start node", then="start failed")
    @pytest.mark.parametrize('node', ['modbus-tcp-1', 'modbus-rtu-1'])
    def test_start_node_start(self, node):
        response = api.node_ctl(node=node, ctl=config.NEU_CTL_START)
        assert 409 == response.status_code

    @description(given="create node", when="del node", then="del success")
    @pytest.mark.parametrize('node', ['modbus-tcp-1', 'modbus-rtu-1'])
    def test_del_node(self, node):
        response = api.del_node(node=node)
        assert 200 == response.status_code
