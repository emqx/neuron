import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import *

hold_int16 = [{"name": "hold_int16", "address": "1!400001",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]

global_config = {
    "nodes": [
        {
            "plugin": "MQTT",
            "name": "mqtt"
        },
        {
            "plugin": "Modbus TCP",
            "name": "modbus"
        }
    ],
    "groups": [
        {
            "driver": "modbus",
            "group": "group",
            "tag_count": 1,
            "interval": 100
        }
    ],
    "tags": [
        {
            "tags": [
                {
                    "type": 3,
                    "name": "hold_int16",
                    "attribute": 3,
                    "precision": 0,
                    "decimal": 0.0,
                    "address": "1!400001",
                    "description": ""
                },
                {
                    "type": 3,
                    "name": "hold_int16_s",
                    "attribute": 11,
                    "precision": 0,
                    "decimal": 0.0,
                    "address": "1!400002",
                    "description": "",
                    "value": 1
                }
            ],
            "driver": "modbus",
            "group": "group"
        }
    ],
    "subscriptions": [
        {
            "groups": [
                {
                    "driver": "modbus",
                    "group": "group"
                }
            ],
            "app": "mqtt"
        }
    ],
    "settings": [
        {
            "node": "mqtt",
            "params": {
                "client-id": "neuron_aBcDeF",
                "qos": 0,
                "format": 0,
                "write-req-topic": "/neuron/mqtt/write/req",
                "write-resp-topic": "/neuron/mqtt/write/resp",
                "offline-cache": False,
                "cache-sync-interval": 100,
                "host": "broker.emqx.io",
                "port": 1883,
                "username": "",
                "password": "",
                "ssl": False
            }
        },
        {
            "node": "modbus",
            "params": {
                "connection_mode": 0,
                "transport_mode": 0,
                "interval": 0,
                "host": "127.0.0.1",
                "port": 502,
                "timeout": 3000,
                "max_retries": 2
            }
        }
    ]
}

global_config_del_modbus = {
    "nodes": [
        {
            "plugin": "MQTT",
            "name": "mqtt"
        }
    ],
    "groups": [],
    "tags": [],
    "subscriptions": [],
    "settings": [
        {
            "node": "mqtt",
            "params": {
                "client-id": "neuron_aBcDeF",
                "qos": 0,
                "format": 0,
                "write-req-topic": "/neuron/mqtt/write/req",
                "write-resp-topic": "/neuron/mqtt/write/resp",
                "offline-cache": False,
                "cache-sync-interval": 100,
                "host": "broker.emqx.io",
                "port": 1883,
                "username": "",
                "password": "",
                "ssl": False
            }
        }
    ]
}


class TestHttp:

    @description(given="running neuron", when="add driver/app node/group/tag, app subscribes driver group, and get global config", then="success")
    def test_get_global_config(self):
        response = api.put_global_config(json=global_config)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_global_config()
        assert 200 == response.status_code

        config_data = response.json()

        assert 'nodes' in config_data
        assert len(config_data['nodes']) == 2
        assert config_data['nodes'][0]['plugin'] == 'MQTT'
        assert config_data['nodes'][1]['plugin'] == 'Modbus TCP'

        assert 'groups' in config_data
        assert len(config_data['groups']) == 1
        assert config_data['groups'][0]['driver'] == 'modbus'

        assert 'tags' in config_data
        assert len(config_data['tags']) == 1
        assert config_data['tags'][0]['driver'] == 'modbus'
        assert len(config_data['tags'][0]['tags']) == 2

        assert 'subscriptions' in config_data
        assert len(config_data['subscriptions']) == 1
        assert config_data['subscriptions'][0]['app'] == 'mqtt'

        assert 'settings' in config_data
        assert len(config_data['settings']) == 2
        assert config_data['settings'][0]['node'] == 'mqtt'

        response = api.put_global_config(json=global_config_del_modbus)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_global_config()
        assert 200 == response.status_code

        config_data = response.json()

        assert 'nodes' in config_data
        assert len(config_data['nodes']) == 1
        assert config_data['nodes'][0]['plugin'] == 'MQTT'

        assert 'groups' in config_data
        assert len(config_data['groups']) == 0

        assert 'tags' in config_data
        assert len(config_data['tags']) == 0

        assert 'subscriptions' in config_data
        assert len(config_data['subscriptions']) == 0

        assert 'settings' in config_data
        assert len(config_data['settings']) == 1
        assert config_data['settings'][0]['node'] == 'mqtt'

    @description(given="running neuron", when="get subscribe info", then="should success")
    def test_get_subscribe_info(self):
        app = 'MQTT-1'
        params = {'topic': '/neuron/test'}
        driver_1 = 'modbus-01'
        driver_2 = 'modbus-02'
        group_1 = 'group-01'
        group_2 = 'group-02'
        try:
            api.add_node_check(app, 'MQTT')
            api.add_node_check(driver_1, 'Modbus TCP')
            api.add_node_check(driver_2, 'Modbus TCP')
            api.add_group_check(driver_1, group_1, 1000)
            api.add_group_check(driver_1, group_2, 1000)
            api.add_group_check(driver_2, group_1, 1000)
            api.add_group_check(driver_2, group_2, 1000)
            api.subscribe_group_check(app, driver_1, group_1, params=params)
            api.subscribe_group_check(app, driver_1, group_2, params=params)
            api.subscribe_group_check(app, driver_2, group_1, params=params)
            api.subscribe_group_check(app, driver_2, group_2, params=params)

            # match all drivers
            response = api.get_subscribe_group(app, driver='modbus')
            assert 200 == response.status_code
            groups = sorted(response.json()['groups'], key=lambda g: (
                g['driver'], g['group']))
            assert len(groups) == 4
            assert groups[0]['driver'] == driver_1
            assert groups[0]['group'] == group_1
            assert groups[1]['driver'] == driver_1
            assert groups[1]['group'] == group_2
            assert groups[2]['driver'] == driver_2
            assert groups[2]['group'] == group_1
            assert groups[3]['driver'] == driver_2
            assert groups[3]['group'] == group_2

            # match one driver
            response = api.get_subscribe_group(app, driver=driver_1)
            assert 200 == response.status_code
            groups = sorted(response.json()['groups'], key=lambda g: (
                g['driver'], g['group']))
            assert len(groups) == 2
            assert groups[0]['driver'] == driver_1
            assert groups[0]['group'] == group_1
            assert groups[1]['driver'] == driver_1
            assert groups[1]['group'] == group_2

            # match none driver
            response = api.get_subscribe_group(app, driver='none')
            assert 200 == response.status_code
            assert len(response.json()['groups']) == 0

            # match all groups
            response = api.get_subscribe_group(app, group='group')
            assert 200 == response.status_code
            groups = sorted(response.json()['groups'], key=lambda g: (
                g['driver'], g['group']))
            assert len(groups) == 4
            assert groups[0]['driver'] == driver_1
            assert groups[0]['group'] == group_1
            assert groups[1]['driver'] == driver_1
            assert groups[1]['group'] == group_2
            assert groups[2]['driver'] == driver_2
            assert groups[2]['group'] == group_1
            assert groups[3]['driver'] == driver_2
            assert groups[3]['group'] == group_2

            # match one group
            response = api.get_subscribe_group(app, group=group_1)
            assert 200 == response.status_code
            groups = sorted(response.json()['groups'], key=lambda g: (
                g['driver'], g['group']))
            assert len(groups) == 2
            assert groups[0]['driver'] == driver_1
            assert groups[0]['group'] == group_1
            assert groups[1]['driver'] == driver_2
            assert groups[1]['group'] == group_1

            # match none group
            response = api.get_subscribe_group(app, group='none')
            assert 200 == response.status_code
            assert len(response.json()['groups']) == 0

            # match driver and group
            response = api.get_subscribe_group(
                app, driver=driver_1, group=group_1)
            assert 200 == response.status_code
            groups = sorted(response.json()['groups'], key=lambda g: (
                g['driver'], g['group']))
            assert len(groups) == 1
            assert groups[0]['driver'] == driver_1
            assert groups[0]['group'] == group_1
            response = api.get_subscribe_group(
                app, driver=driver_2, group=group_2)
            assert 200 == response.status_code
            groups = sorted(response.json()['groups'], key=lambda g: (
                g['driver'], g['group']))
            assert len(groups) == 1
            assert groups[0]['driver'] == driver_2
            assert groups[0]['group'] == group_2
        finally:
            api.del_node(app)
            api.del_node(driver_1)
            api.del_node(driver_2)

    @description(given="running neuron", when="get mqtt shcema", then="success")
    def test_get_plugin_schema(self):
        response = api.get_plugin_schema(plugin='mqtt')
        assert 200 == response.status_code

        response = api.get_plugin_schema(plugin='modbus-tcp')
        assert 200 == response.status_code

        response = api.get_plugin_schema(plugin='invalid')
        assert 404 == response.status_code
        assert response.json()['status'] == 'error'

    @description(given="running neuron", when="get plugin", then="success")
    def test_get_plugin(self):
        response = api.get_plugin()
        assert 200 == response.status_code

    @description(given="running neuron", when="get version", then="success")
    def test_get_version(self):
        response = api.get_version()
        assert 200 == response.status_code

    @description(given="running neuron", when="get status", then="success")
    def test_get_status(self):
        response = api.get_status()
        assert 200 == response.status_code

    @description(given="running neuron", when="test ndriver", then="failed because it is not supported yet")
    def test_ndriver(self):
        api.add_node_check(node='modbus', plugin=config.PLUGIN_MODBUS_TCP)
        api.add_group_check(node='modbus', group='group')
        api.add_tags_check(node='modbus', group='group', tags=hold_int16)
        response = api.subscribe_group(
            app='mqtt', driver='modbus', group='group')
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.post_ndriver_map(
            ndriver='mqtt', driver='modbus', group='group')
        assert 400 == response.status_code

        response = api.get_ndriver_map(ndriver='mqtt')
        assert 400 == response.status_code

        ndriver_tag_param = {"ndriver": "mqtt", "driver": "modbus", "group": "group", "tags": [
            {"name": "hold_int16", "params": {"address": "1!400002"}}]}
        response = api.put_ndriver_tag_param(json=ndriver_tag_param)
        assert 200 == response.status_code

        ndriver_tag_info = {"ndriver": "mqtt", "driver": "modbus", "group": "group", "tags": [
            {"name": "hold_int16", "address": "1!400002"}]}
        response = api.put_ndriver_tag_info(json=ndriver_tag_info)
        assert 200 == response.status_code

        response = api.get_ndriver_tag(
            ndriver='mqtt', driver='modbus', group='group')
        assert 200 == response.status_code

        response = api.delete_ndriver_map(
            ndriver='mqtt', driver='modbus', group='group')
        assert 200 == response.status_code

    @description(given="running neuron", when="test jwt error", then="failed")
    def test_jwt_err(self):
        response = api.change_password(new_password='123456', jwt='invalid')
        assert 401 == response.status_code

        response = api.get_hwtoken(jwt='invalid')
        assert 404 == response.status_code

        response = api.get_license(jwt='invalid')
        assert 404 == response.status_code

        response = api.upload_license(license='abaaba', jwt='invalid')
        assert 404 == response.status_code

        response = api.upload_virtual_license(license='abaaba', jwt='invalid')
        assert 404 == response.status_code

        response = api.delete_virtual_license(jwt='invalid')
        assert 404 == response.status_code
