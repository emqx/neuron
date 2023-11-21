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
        assert len(config_data['tags'][0]['tags']) == 1

        assert 'subscriptions' in config_data
        assert len(config_data['subscriptions']) == 1
        assert config_data['subscriptions'][0]['app'] == 'mqtt'

        assert 'settings' in config_data
        assert len(config_data['settings']) == 2
        assert config_data['settings'][0]['node'] == 'mqtt'