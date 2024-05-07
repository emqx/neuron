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

        response = api.get_nodes(type=1)
        assert 200 == response.status_code

    @description(given="running neuron", when="add app node", then="add success")
    @pytest.mark.parametrize('node,plugin', [('mqtt-1', config.PLUGIN_MQTT)])
    def test_create_app_node(self, node, plugin):
        response = api.add_node(node=node,
                                plugin=plugin)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes(type=2)
        assert 200 == response.status_code

    @description(given="running neuron", when="add app node with wrong setting", then="should fail")
    @pytest.mark.parametrize('node,plugin', [('mqtt-test', config.PLUGIN_MQTT)])
    def test_create_app_node_with_wrong_setting(self, node, plugin):
        params = {"client-id": "neuron_aBcDeF"}
        response = api.add_node(node=node, plugin=plugin, params=params)
        assert 400 == response.status_code
        assert error.NEU_ERR_NODE_SETTING_INVALID == response.json()['error']

        response = api.get_nodes_state(node)
        assert 404 == response.status_code
        assert error.NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="running neuron", when="add app node with correct setting", then="should success")
    @pytest.mark.parametrize('node,plugin', [('mqtt-test', config.PLUGIN_MQTT)])
    def test_create_app_node_with_correct_setting(self, node, plugin):
        params = {
            "client-id": "neuron_aBcDeF",
            "qos": 0,
            "format": 0,
            "write-req-topic": f"/neuron/{node}/write/req",
            "write-resp-topic": f"/neuron/{node}/write/resp",
            "offline-cache": False,
            "host": "broker.emqx.io",
            "port": 1883,
            "ssl": False
        }
        response = api.add_node(node=node, plugin=plugin, params=params)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes_state(node)
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()['running']
        api.del_node(node)

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

        response = api.subscribe_groups(app='mqtt-1', groups=[{"driver": "modbus-tcp-1", "group": "group-1", "params":{"topic":"/neuron"}},
                                                              {"driver": "modbus-tcp-1", "group": "group-2"},
                                                              {"driver": "modbus-tcp-1", "group": "group-3"}])
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

    @description(given="mqtt node", when="subscribe with err body", then="subscribe failed")
    def test_mqtt_sub_err(self):
        response = api.subscribe_groups(app='mqtt-1', groups=[{"driver": "modbus-tcp-1", "group": "group-1", "params":{"topic":"/neuron"}},
                                                              {"driver": "modbus-tcp-1", "group": "group-2"},
                                                              {"driver": "modbus-tcp-1", "err": "group-3"}])
        assert 400 == response.status_code

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

        response = api.get_subscribe_group(app='mqtt-1')
        assert 200 == response.status_code
        assert [] == response.json()["groups"]

        response = api.subscribe_group(app='mqtt-1', driver='modbus-tcp-1', group='group-1')
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes_state(node='mqtt-1')
        assert 200 == response.status_code
        assert 1 == response.json()['sub_group_count']

    @description(given="mqtt node", when="subscribe non-existent group", then="return failure")
    def test_mqtt_sub_non_existent_group(self):
        response = api.subscribe_group(app='mqtt-1', driver='modbus-tcp-1', group='non-existent')
        assert 404 == response.status_code
        assert error.NEU_ERR_GROUP_NOT_EXIST == response.json()['error']

    @description(given="mqtt node", when="update subscribe non-existent group", then="return failure")
    def test_mqtt_update_subscribe_non_existent_group(self):
        response = api.subscribe_group_update(app='mqtt-1', driver='modbus-tcp-1', group='non-existent', params={"topic":"new_topic"})
        assert 404 == response.status_code
        assert error.NEU_ERR_GROUP_NOT_SUBSCRIBE == response.json()['error']

    @description(given="mqtt node", when="update subscribe group", then="success")
    def test_mqtt_update_subscribe_group(self):
        response = api.subscribe_group_update(app='mqtt-1', driver='modbus-tcp-1', group='group-1', params={"topic":"new_topic"})
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

    @description(given="mqtt node", when="subscribe multiple non-existent groups", then="subscribe failed")
    def test_mqtt_subscribe_multiple_non_existent_groups(self):
        response = api.subscribe_groups(app='mqtt-1', groups=[{"driver": "modbus-tcp-1", "group": "non-1"},
                                                              {"driver": "modbus-tcp-1", "group": "non-2"},
                                                              {"driver": "modbus-tcp-1", "group": "non-3"}])
        assert 404 == response.status_code
        assert error.NEU_ERR_GROUP_NOT_EXIST == response.json()['error']

    @description(given="mqtt node", when="subscribe group with empty topic", then="subscribe failed")
    def test_mqtt_subscribe_group_with_empty_topic(self):
        response = api.subscribe_group(app='mqtt-1', driver='modbus-tcp-1', group='group-1', params={"topic":""})
        assert 200 == response.status_code
        assert error.NEU_ERR_MQTT_SUBSCRIBE_FAILURE == response.json()['error']

        response = api.subscribe_groups(app='mqtt-1', groups=[{"driver": "modbus-tcp-1", "group": "group-1", "params": {"topic":""}}])
        assert 200 == response.status_code
        assert error.NEU_ERR_MQTT_SUBSCRIBE_FAILURE == response.json()['error']

    @description(given="mqtt node with subscriptions", when="get subscriptions", then="return all subscriptions")
    def test_mqtt_get_subscriptions(self):
        response = api.get_subscribe_group(app='mqtt-1')
        assert 200 == response.status_code
        assert "modbus-tcp-1" == response.json()["groups"][0]["driver"]
        assert "group-1" == response.json()["groups"][0]["group"]
        assert "new_topic" == response.json()["groups"][0]["params"]["topic"]

    @description(given="delete driver group that is subscribed", when="get subscriptions", then="return empty")
    def test_get_subscription_group_be_deleted(self):
        response = api.del_group(node='modbus-tcp-1', group="group-1")
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_subscribe_group(app='mqtt-1')
        assert 200 == response.status_code
        assert [] == response.json()["groups"]

    @description(given="delete driver node whose group is subscribed", when="get subscriptions", then="return empty")
    def test_get_subscription_node_be_deleted(self):
        response = api.add_group(node="modbus-tcp-1", group="group_test")
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.subscribe_group(app='mqtt-1', driver='modbus-tcp-1', group='group_test')
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_subscribe_group(app='mqtt-1')
        assert 200 == response.status_code
        assert "modbus-tcp-1" == response.json()["groups"][0]["driver"]
        assert "group_test" == response.json()["groups"][0]["group"]

        response = api.del_node(node="modbus-tcp-1")
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_subscribe_group(app='mqtt-1')
        assert 200 == response.status_code
        assert [] == response.json()["groups"]