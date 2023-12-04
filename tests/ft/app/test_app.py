import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import *

hold_int16 = [{"name": "hold_int16", "address": "1!400001",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]

class TestDriver:

    @description(given="running neuron", when="add driver node", then="add success")
    @pytest.mark.parametrize('node,plugin', [('modbus-tcp-1', config.PLUGIN_MODBUS_TCP)])
    def test_create_driver_node(self, node, plugin):
        response = api.add_node(node=node,
                                plugin=plugin)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

    @description(given="running neuron", when="add app node", then="add success")
    @pytest.mark.parametrize('node,plugin', [('mqtt-1', config.PLUGIN_MQTT)])
    def test_create_app_node(self, node, plugin):
        response = api.add_node(node=node,
                                plugin=plugin)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

    @description(given="app node with subscriptions", when="delete node", then="should success")
    @pytest.mark.parametrize('app,plugin', [('mqtt', config.PLUGIN_MQTT)])
    def test_delete_app(self, app, plugin):
        driver = 'modbus'
        group = 'group'
        try:
            api.add_node_check(node=app, plugin=plugin)
            api.add_node_check(node=driver, plugin=config.PLUGIN_MODBUS_TCP)
            api.modbus_tcp_node_setting(node=driver, port=502)
            api.add_tags_check(node=driver, group=group, tags=hold_int16)
            api.subscribe_group_check(app=app, driver=driver, group=group)
            response = api.del_node(app)
            assert 200 == response.status_code
            assert error.NEU_ERR_SUCCESS == response.json()['error']
        finally:
            api.del_node(driver)

    @description(given="modbus tcp node", when="setting modbus tcp node and groups/tags", then="setting success")
    def test_modbus_tcp_setting(self):
        response = api.modbus_tcp_node_setting(node='modbus-tcp-1', port=502)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.add_group(node='modbus-tcp-1', group="group-1")
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.add_group(node='modbus-tcp-1', group="group-2")
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.add_group(node='modbus-tcp-1', group="group-3")
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.add_tags(node='modbus-tcp-1', group="group-1", tags=hold_int16)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.add_tags(node='modbus-tcp-1', group="group-2", tags=hold_int16)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.add_tags(node='modbus-tcp-1', group="group-3", tags=hold_int16)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

    @description(given="mqtt node", when="setting mqtt node and subscribe modbus group", then="setting&subscribe success")
    def test_mqtt_setting(self):
        response = api.mqtt_node_setting(node='mqtt-1')
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.subscribe_groups(app='mqtt-1', groups=[{"driver": "modbus-tcp-1", "group": "group-1"},
                                                              {"driver": "modbus-tcp-1", "group": "group-2"},
                                                              {"driver": "modbus-tcp-1", "group": "group-3"}])
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

    @description(given="mqtt node", when="subscribe/unsubscribe modbus group", then="return correct sub_group_count in node state")
    def test_mqtt_sub_group_count(self):
        response = api.get_nodes_state(node='mqtt-1')
        assert 200 == response.status_code
        assert 3 == response.json()['sub_group_count']

        response = api.unsubscribe_group(app='mqtt-1', driver='modbus-tcp-1', group='group-1')
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.unsubscribe_group(app='mqtt-1', driver='modbus-tcp-1', group='group-2')
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.unsubscribe_group(app='mqtt-1', driver='modbus-tcp-1', group='group-3')
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes_state(node='mqtt-1')
        assert 200 == response.status_code
        assert 0 == response.json()['sub_group_count']

        response = api.subscribe_group(app='mqtt-1', driver='modbus-tcp-1', group='group-1')
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes_state(node='mqtt-1')
        assert 200 == response.status_code
        assert 1 == response.json()['sub_group_count']