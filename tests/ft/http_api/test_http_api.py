import copy

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
                    "attribute": 1,
                    "precision": 0,
                    "decimal": 0.0,
                    "bias": 1.0,
                    "address": "1!400001",
                    "description": ""
                },
                {
                    "type": 3,
                    "name": "hold_int16_s",
                    "attribute": 9,
                    "precision": 0,
                    "decimal": 0.0,
                    "bias": 1.0,
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

driver_1 = {
    "name": "modbus-driver-1",
    "plugin": "Modbus TCP",
    "params": {
        "connection_mode": 0,
        "transport_mode": 0,
        "interval": 0,
        "host": "127.0.0.1",
        "port": 502,
        "timeout": 3000,
        "max_retries": 2
    },
    "groups": [
        {
            "group": "group-01",
            "interval": 1000,
            "tags": [
                {
                    "type": 3,
                    "name": "hold_int16",
                    "attribute": 1,
                    "precision": 0,
                    "decimal": 0.0,
                    "bias": 1.0,
                    "address": "1!400001",
                    "description": ""
                },
                {
                    "type": 3,
                    "name": "hold_int16_s",
                    "attribute": 9,
                    "precision": 0,
                    "decimal": 0.0,
                    "bias": 1.0,
                    "address": "1!400002",
                    "description": "",
                    "value": 1
                }
            ],
        },
        {
            "group": "group-02",
            "interval": 1000,
            "tags": [],
        }
    ]
}

driver_2 = {
    "name": "modbus-driver-2",
    "plugin": "Modbus TCP",
    "params": {
        "connection_mode": 0,
        "transport_mode": 0,
        "interval": 0,
        "host": "127.0.0.1",
        "port": 502,
        "timeout": 3000,
        "max_retries": 2
    },
    "groups": [
        {
            "group": "group-01",
            "interval": 1000,
            "tags": [
                {
                    "type": 3,
                    "name": "hold_int16",
                    "attribute": 1,
                    "precision": 0,
                    "decimal": 0.0,
                    "bias": 1.0,
                    "address": "1!400001",
                    "description": ""
                },
                {
                    "type": 3,
                    "name": "hold_int16_s",
                    "attribute": 9,
                    "precision": 0,
                    "decimal": 0.0,
                    "bias": 1.0,
                    "address": "1!400002",
                    "description": "",
                    "value": 1
                }
            ],
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

        for node in global_config['nodes']:
            api.del_node(node['name'])

    @description(given="running neuron", when="put drivers with too long node name", then="should fail")
    def test_put_drivers_with_long_node_name(self):
        response = api.put_driver(
            name='long'*100, plugin=driver_1['plugin'], params=driver_1['params'])
        assert 400 == response.status_code
        assert error.NEU_ERR_NODE_NAME_TOO_LONG == response.json()['error']

    @description(given="running neuron", when="put drivers with duplicate node name", then="should fail")
    def test_put_drivers_with_duplicate_node(self):
        response = api.put_drivers([driver_1, driver_1])
        assert 400 == response.status_code
        assert error.NEU_ERR_BODY_IS_WRONG == response.json()['error']

    @description(given="running neuron", when="put drivers with non-existent plugin", then="should fail")
    def test_put_drivers_with_nonexistent_plugin(self):
        response = api.put_driver(
            name=driver_1['name'], plugin='unkown', params=driver_1['params'])
        assert 404 == response.status_code
        assert error.NEU_ERR_LIBRARY_NOT_FOUND == response.json()['error']

    @description(given="running neuron", when="put drivers with too long plugin name", then="should fail")
    def test_put_drivers_with_long_plugin_name(self):
        response = api.put_driver(
            name=driver_1['name'], plugin='long'*100, params=driver_1['params'])
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_NAME_TOO_LONG == response.json()['error']

    @description(given="running neuron", when="put drivers with north plugin", then="should fail")
    def test_put_drivers_with_north_plugin(self):
        response = api.put_driver(name='not-driver', plugin='MQTT', params={})
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TYPE_NOT_SUPPORT == response.json()[
            'error']

    @description(given="running neuron", when="put drivers with too long group name", then="should fail")
    def test_put_drivers_with_long_group_name(self):
        driver = copy.deepcopy(driver_1)
        driver["groups"][0]["group"] = 'long' * 100
        response = api.put_drivers([driver])
        assert 400 == response.status_code
        assert error.NEU_ERR_GROUP_NAME_TOO_LONG == response.json()['error']

    @description(given="running neuron", when="put drivers with duplicate group name", then="should fail")
    def test_put_drivers_with_duplicate_group(self):
        driver = copy.deepcopy(driver_1)
        driver["groups"].append(driver["groups"][0])
        response = api.put_drivers([driver])
        assert 400 == response.status_code
        assert error.NEU_ERR_BODY_IS_WRONG == response.json()['error']

    @description(given="running neuron", when="put drivers with invalid group interval", then="should fail")
    def test_put_drivers_with_invalid_group_interval(self):
        driver = copy.deepcopy(driver_1)
        driver["groups"][0]["interval"] = 10
        response = api.put_drivers([driver])
        assert 400 == response.status_code
        assert error.NEU_ERR_GROUP_PARAMETER_INVALID == response.json()[
            'error']

    @description(given="running neuron", when="put drivers with too many groups", then="should fail")
    def test_put_drivers_with_too_many_groups(self):

        driver = {
            "name": driver_1["name"],
            "plugin": driver_1["plugin"],
            "params": driver_1["params"],
            "groups": []
        }
        for i in range(1025):
            driver["groups"].append({"group": "grp" + str(i), "interval": 1000, "tags": []})

        response = api.put_drivers([driver])
        assert 400 == response.status_code
        assert error.NEU_ERR_GROUP_MAX_GROUPS == response.json()['error']

    @description(given="running neuron", when="put drivers with duplicate tag name", then="should fail")
    def test_put_drivers_with_duplicate_tag(self):
        driver = copy.deepcopy(driver_1)
        driver["groups"][0]["tags"].append(driver["groups"][0]["tags"][0])
        response = api.put_drivers([driver])
        assert 400 == response.status_code
        assert error.NEU_ERR_BODY_IS_WRONG == response.json()['error']

    @description(given="running neuron", when="put drivers", then="should success")
    def test_put_drivers(self):
        drivers = [driver_1, driver_2]
        response = api.put_drivers(drivers)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes_state(driver_1["name"])
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()['running']

        response = api.get_nodes_state(driver_2["name"])
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()['running']

        for driver in drivers:
            for group in driver["groups"]:
                response = api.get_tags(driver["name"], group["group"])
                assert 200 == response.status_code
                assert len(group["tags"]) == len(response.json()["tags"])

            api.del_node(driver["name"])

    @description(given="running neuron", when="put drivers that exists", then="should replace")
    def test_put_drivers_already_exist(self):
        response = api.put_drivers([driver_1])
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        driver = {
            "name": driver_1["name"],
            "plugin": driver_1["plugin"],
            "params": driver_1["params"],
            "groups": [
                {
                    "group": "grp",
                    "interval": 1000,
                    "tags": [],
                }
            ]
        }
        response = api.put_drivers([driver])
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_nodes_state(driver["name"])
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()['running']

        response = api.get_tags(driver["name"], driver["groups"][0]["group"])
        assert 0 == len(response.json()["tags"])

        api.del_node(driver["name"])

    @description(given="running neuron", when="put drivers", then="should be persisted")
    def test_put_drivers_persistence(self, class_setup_and_teardown):
        drivers = [driver_1, driver_2]
        response = api.put_drivers(drivers)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        class_setup_and_teardown.restart()

        for driver in drivers:
            for group in driver["groups"]:
                response = api.get_tags(driver["name"], group["group"])
                assert 200 == response.status_code
                assert len(group["tags"]) == len(response.json()["tags"])
            api.del_node(driver["name"])

    @description(given="running neuron", when="get drivers", then="should success")
    def test_get_drivers(self):
        drivers = [driver_1, driver_2]
        response = api.put_drivers(drivers)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

        # no matched drivers
        response = api.get_driver('no-such-driver')
        assert 200 == response.status_code
        resp_drivers = response.json()["nodes"]
        assert 0 == len(resp_drivers)

        # filter by driver_1
        response = api.get_driver(driver_1["name"])
        assert 200 == response.status_code
        resp_drivers = response.json()["nodes"]
        assert 1 == len(resp_drivers)
        assert driver_1 == resp_drivers[0]

        # filter by driver_2
        response = api.get_driver(driver_2["name"])
        assert 200 == response.status_code
        resp_drivers = response.json()["nodes"]
        assert 1 == len(resp_drivers)
        assert driver_2 == resp_drivers[0]

        # filter by driver_1 and driver_2
        response = api.get_drivers(names=[driver_1["name"], driver_2["name"]])
        assert 200 == response.status_code
        resp_drivers = sorted(
            response.json()["nodes"], key=lambda x: x["name"])
        assert 2 == len(resp_drivers)
        assert drivers == resp_drivers

        # no filter
        response = api.get_drivers()
        assert 200 == response.status_code
        resp_drivers = sorted(
            response.json()["nodes"], key=lambda x: x["name"])
        assert 2 == len(resp_drivers)
        assert drivers == resp_drivers

        for driver in drivers:
            api.del_node(driver["name"])

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
