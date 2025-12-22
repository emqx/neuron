import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import *

hold_int16 = [{"name": "hold_int16", "address": "1!400001",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]


class TestAppsExportImport:

    @description(given="mqtt node subscribed to modbus group",
                 when="export apps",
                 then="returns node config with subscriptions")
    def test_export_app_with_subscriptions(self):
        driver = 'modbus-export-test'
        app = 'mqtt-export-test'
        group = 'group1'

        try:
            api.add_node_check(node=driver, plugin=config.PLUGIN_MODBUS_TCP)
            api.modbus_tcp_node_setting(node=driver, port=502)
            api.add_group_check(node=driver, group=group)
            api.add_tags_check(node=driver, group=group, tags=hold_int16)

            api.add_node_check(node=app, plugin=config.PLUGIN_MQTT)
            api.subscribe_group_check(app=app, driver=driver, group=group)

            response = api.get_apps()
            assert 200 == response.status_code
            assert 'attachment' in response.headers.get('Content-Disposition', '')

            data = response.json()
            assert 'nodes' in data

            app_data = None
            for node in data['nodes']:
                if node['name'] == app:
                    app_data = node
                    break

            assert app_data is not None
            assert app_data['plugin'] == config.PLUGIN_MQTT
            assert 'groups' in app_data
            assert len(app_data['groups']) >= 1

            sub = app_data['groups'][0]
            assert sub['driver'] == driver
            assert sub['group'] == group

        finally:
            api.del_node(app)
            api.del_node(driver)

    @description(given="app node does not exist",
                 when="import new app node",
                 then="node created successfully")
    def test_import_new_app_node(self):
        app = 'mqtt-import-new'

        try:
            api.del_node(app)

            apps = [{
                "name": app,
                "plugin": config.PLUGIN_MQTT,
                "params": {
                    "client-id": "neuron_test_import",
                    "qos": 0,
                    "format": 0,
                    "write-req-topic": f"/neuron/{app}/write/req",
                    "write-resp-topic": f"/neuron/{app}/write/resp",
                    "offline-cache": False,
                    "host": "broker.emqx.io",
                    "port": 1883,
                    "ssl": False
                },
                "groups": []
            }]

            response = api.put_apps(apps)
            assert 200 == response.status_code
            assert error.NEU_ERR_SUCCESS == response.json()['error']

            response = api.get_nodes_state(app)
            assert 200 == response.status_code

        finally:
            api.del_node(app)

    @description(given="app node already exists",
                 when="import same app node",
                 then="old node deleted and new node created")
    def test_import_existing_app_node_overwrite(self):
        app = 'mqtt-import-overwrite'

        try:
            api.add_node_check(node=app, plugin=config.PLUGIN_MQTT)

            response = api.get_nodes_state(app)
            assert 200 == response.status_code

            apps = [{
                "name": app,
                "plugin": config.PLUGIN_MQTT,
                "params": {
                    "client-id": "neuron_overwrite_test",
                    "qos": 1,
                    "format": 0,
                    "write-req-topic": f"/neuron/{app}/write/req",
                    "write-resp-topic": f"/neuron/{app}/write/resp",
                    "offline-cache": False,
                    "host": "broker.emqx.io",
                    "port": 1883,
                    "ssl": False
                },
                "groups": []
            }]

            response = api.put_apps(apps)
            assert 200 == response.status_code
            assert error.NEU_ERR_SUCCESS == response.json()['error']

            response = api.get_nodes_state(app)
            assert 200 == response.status_code

            response = api.get_apps(names=[app])
            assert 200 == response.status_code
            data = response.json()
            assert len(data['nodes']) == 1
            assert 'neuron_overwrite_test' in str(data['nodes'][0].get('params', ''))

        finally:
            api.del_node(app)

    @description(given="subscription driver does not exist",
                 when="import app with subscription",
                 then="returns subscription error")
    def test_import_app_subscription_driver_not_exist(self):
        app = 'mqtt-import-sub-error'
        non_exist_driver = 'non-exist-driver-12345'

        try:
            api.del_node(app)

            apps = [{
                "name": app,
                "plugin": config.PLUGIN_MQTT,
                "params": {
                    "client-id": "neuron_sub_test",
                    "qos": 0,
                    "format": 0,
                    "write-req-topic": f"/neuron/{app}/write/req",
                    "write-resp-topic": f"/neuron/{app}/write/resp",
                    "offline-cache": False,
                    "host": "broker.emqx.io",
                    "port": 1883,
                    "ssl": False
                },
                "groups": [{
                    "driver": non_exist_driver,
                    "group": "group1"
                }]
            }]

            response = api.put_apps(apps)
            assert response.json()['error'] != error.NEU_ERR_SUCCESS

        finally:
            api.del_node(app)

    @description(given="configured app node",
                 when="export then delete then import",
                 then="node restored with same config")
    def test_export_import_roundtrip(self):
        driver = 'modbus-roundtrip'
        app = 'mqtt-roundtrip'
        group = 'group1'

        try:
            api.add_node_check(node=driver, plugin=config.PLUGIN_MODBUS_TCP)
            api.modbus_tcp_node_setting(node=driver, port=502)
            api.add_group_check(node=driver, group=group)
            api.add_tags_check(node=driver, group=group, tags=hold_int16)

            api.add_node_check(node=app, plugin=config.PLUGIN_MQTT)
            api.subscribe_group_check(app=app, driver=driver, group=group)

            response = api.get_apps(names=[app])
            assert 200 == response.status_code
            exported_data = response.json()

            assert 'nodes' in exported_data
            assert len(exported_data['nodes']) == 1
            app_config = exported_data['nodes'][0]
            assert app_config['name'] == app

            api.del_node(app)
            response = api.get_nodes_state(app)
            assert response.json()['error'] == error.NEU_ERR_NODE_NOT_EXIST

            response = api.put_apps(exported_data['nodes'])
            assert 200 == response.status_code
            assert error.NEU_ERR_SUCCESS == response.json()['error']

            response = api.get_nodes_state(app)
            assert 200 == response.status_code

            response = api.get_subscribe_group(app)
            assert 200 == response.status_code
            groups = response.json().get('groups', [])
            assert len(groups) >= 1

            found = False
            for g in groups:
                if g['driver'] == driver and g['group'] == group:
                    found = True
                    break
            assert found, "Subscription not restored"

        finally:
            api.del_node(app)
            api.del_node(driver)

