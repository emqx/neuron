import neuron.api as api
from neuron.config import *
from neuron.common import *
from neuron.error import *


class TestNode:

    @description(given="", when="ping", then="ping success")
    def test_ping(self):
        response = api.ping()
        assert 200 == response.status_code

    @description(given="non-existent plugin", when="add a driver with non-existent plugin", then="add failed")
    def test_add_driver_non_existent_plugin(self):
        response = api.add_node(node="unknow", plugin="non-existent")
        assert 404 == response.status_code
        assert NEU_ERR_LIBRARY_NOT_FOUND == response.json()['error']

    @description(given="non-existent type of node", when="get invalid type of node", then="get failed")
    def test_get_invalid_node_type(self):
        response = api.get_nodes(type=3)
        assert 400 == response.status_code
        assert NEU_ERR_PARAM_IS_WRONG == response.json()['error']

    @description(given="existent plugin", when="add a driver", then="add success")
    def test_add_driver(self):
        response = api.add_node(node="modbus-tcp", plugin=PLUGIN_MODBUS_TCP)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="driver", when="get the driver node", then="get success")
    def test_get_driver(self):
        response = api.get_nodes(type=NEU_NODE_DRIVER)
        assert 200 == response.status_code
        assert "modbus-tcp" == response.json()['nodes'][0]["name"]

    @description(given="a driver and no apps", when="get app node", then="get empty app")
    def test_get_app_empty(self):
        response = api.get_nodes(type=NEU_NODE_APP)
        assert 200 == response.status_code
        assert [] == response.json()['nodes']

    @description(given="exist a driver node", when="add a driver node with the same name", then="add failed")
    def test_add_driver_with_same_name(self):
        response = api.add_node(node="modbus-tcp", plugin=PLUGIN_MODBUS_TCP)
        assert 409 == response.status_code
        assert NEU_ERR_NODE_EXIST == response.json()['error']

    @description(given="existent plugin", when="add an app node", then="add success")
    def test_add_app(self):
        response = api.add_node(node="mqtt", plugin=PLUGIN_MQTT)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="app", when="get the app node", then="get success")
    def test_get_app(self):
        response = api.get_nodes(type=NEU_NODE_APP)
        assert 200 == response.status_code
        assert "mqtt" == response.json()['nodes'][0]["name"]

    @description(given="existent driver node", when="update the node name to empty string", then="update failed")
    def test_update_node_name_to_empty(self):
        response = api.update_node(node="modbus-tcp", new_name="")
        assert 400 == response.status_code
        assert NEU_ERR_NODE_NAME_EMPTY == response.json()['error']

    @description(given="non-existent driver node", when="update non-existent node", then="update failed")
    def test_update_non_existent_node(self):
        response = api.update_node(node="non-existent", new_name="not found")
        assert 404 == response.status_code
        assert NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="existent driver and app node", when="update node name conflict", then="update failed")
    def test_update_conflict_node_name(self):
        response = api.update_node(node="modbus-tcp", new_name="mqtt")
        assert 409 == response.status_code
        assert NEU_ERR_NODE_EXIST == response.json()['error']

    @description(given="existent driver and app node", when="update new node name to the same", then="update failed")
    def test_update_node_name_to_the_same(self):
        response = api.update_node(node="modbus-tcp", new_name="modbus-tcp")
        assert 409 == response.status_code
        assert NEU_ERR_NODE_EXIST == response.json()['error']

        response = api.update_node(node="mqtt", new_name="mqtt")
        assert 409 == response.status_code
        assert NEU_ERR_NODE_EXIST == response.json()['error']

    @description(given="existent driver", when="update driver node name", then="update success")
    def test_update_driver_node_name(self):
        response = api.update_node(node="modbus-tcp", new_name="modbus-tcp-new")
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.update_node(node="modbus-tcp-new", new_name="modbus-tcp")
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="existent app", when="update app node name", then="update success")
    def test_update_app_node_name(self):
        response = api.update_node(node="mqtt", new_name="mqtt-new")
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.update_node(node="mqtt-new", new_name="mqtt")
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="existent app", when="delete an APP node with existing name", then="delete success")
    def test_delete_app_node(self):
        response = api.del_node(node="mqtt")
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes(type=NEU_NODE_APP)
        assert 200 == response.status_code
        assert [] == response.json()['nodes']

    @description(given="non-existent app", when="delete an APP node with non-existing name", then="delete failed")
    def test_delete_app_node_non_existent(self):
        response = api.del_node(node="mqtt")
        assert 404 == response.status_code
        assert NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="existent driver", when="delete a Driver node with existing name", then="delete success")
    def test_delete_driver_node(self):
        response = api.del_node(node="modbus-tcp")
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes(type=NEU_NODE_DRIVER)
        assert 200 == response.status_code
        assert [] == response.json()['nodes']

    @description(given="non-existent driver", when="delete a Driver node with non-existing name", then="delete failed")
    def test_delete_driver_node_non_existent(self):
        response = api.del_node(node="modbus-tcp")
        assert 404 == response.status_code
        assert NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="non-existent driver", when="add setting to non-existent node", then="add failed")
    def test_set_non_existent_node(self):
        response = api.modbus_tcp_node_setting(node='modbus-tcp', interval=1, port=502)
        assert 404 == response.status_code
        assert NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="non-existent driver", when="get setting from non-existent node", then="get failed")
    def test_get_non_existent_node_setting(self):
        response = api.get_node_setting(node='modbus-tcp')
        assert 404 == response.status_code
        assert NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="add driver but not set", when="get setting from the node not set", then="get failed")
    def test_get_node_setting_not_set(self):
        response = api.add_node(node="modbus-tcp", plugin=PLUGIN_MODBUS_TCP)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_node_setting(node='modbus-tcp')
        assert 200 == response.status_code
        assert NEU_ERR_NODE_SETTING_NOT_FOUND == response.json()['error']

    @description(given="driver", when="set node", then="set success")
    def test_set_node(self):
        response = api.modbus_tcp_node_setting(node='modbus-tcp', interval=1, port=502)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="driver", when="get node setting", then="get success")
    def test_get_node_setting(self):
        response = api.get_node_setting(node='modbus-tcp')
        assert 200 == response.status_code
        assert 'modbus-tcp' == response.json()['node']
        assert 502 == response.json()['params']['port']
        assert '127.0.0.1' == response.json()['params']['host']
        assert 3000 == response.json()['params']['timeout']
        assert 1 == response.json()['params']['interval']