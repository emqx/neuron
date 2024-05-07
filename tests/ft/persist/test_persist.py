import neuron.api as api
from neuron.config import *
from neuron.common import description
import neuron.process as process
from neuron.error import *
import shutil
import os
import time

hold_bit = [{"name": "hold_bit", "address": "1!400001.15",
             "attribute": NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": NEU_TYPE_BIT}]
hold_int16 = [{"name": "hold_int16", "address": "1!400001",
               "attribute": NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": NEU_TYPE_INT16}]
hold_uint16 = [{"name": "hold_uint16", "address": "1!400002",
                "attribute": NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": NEU_TYPE_UINT16}]
hold_int16_to_int32 = [{"name": "hold_int16", "address": "1!400002",
               "attribute": NEU_TAG_ATTRIBUTE_RW, "type": NEU_TYPE_INT32}]

hold_int8 = [{"name": "hold_int8", "address": "1!400001",
             "attribute": NEU_TAG_ATTRIBUTE_WRITE, "type": NEU_TYPE_INT8}]
hold_uint8 = [{"name": "hold_uint8", "address": "1!400001",
             "attribute": NEU_TAG_ATTRIBUTE_WRITE, "type": NEU_TYPE_UINT8}]
hold_bool = [{"name": "hold_bool", "address": "1!400001.15",
             "attribute": NEU_TAG_ATTRIBUTE_WRITE, "type": NEU_TYPE_BOOL}]
hold_word = [{"name": "hold_word", "address": "1!400001",
             "attribute": NEU_TAG_ATTRIBUTE_WRITE, "type": NEU_TYPE_WORD}]
hold_dword = [{"name": "hold_dword", "address": "1!400001",
             "attribute": NEU_TAG_ATTRIBUTE_WRITE, "type": NEU_TYPE_DWORD}]
hold_lword = [{"name": "hold_lword", "address": "1!400001",
             "attribute": NEU_TAG_ATTRIBUTE_WRITE, "type": NEU_TYPE_LWORD}]
hold_ptr = [{"name": "hold_ptr", "address": "1!400001",
             "attribute": NEU_TAG_ATTRIBUTE_WRITE, "type": NEU_TYPE_PTR}]
hold_dt = [{"name": "hold_dt", "address": "1!400001",
             "attribute": NEU_TAG_ATTRIBUTE_WRITE, "type": NEU_TYPE_DATA_AND_TIME}]
hold_t = [{"name": "hold_t", "address": "1!400001",
             "attribute": NEU_TAG_ATTRIBUTE_WRITE, "type": NEU_TYPE_TIME}]


class TestPersist:

    @description(given="add adapters and restart Neuron", when="get the newly adapters", then="get success")
    def test_get_node_after_restart(self):
        process.remove_persistence()
        p = process.NeuronProcess()
        p.start()

        response = api.add_node(node="modbus-tcp-1", plugin=PLUGIN_MODBUS_TCP)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']
        response = api.add_node(node="modbus-tcp-2", plugin=PLUGIN_MODBUS_TCP)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']  
        response = api.add_node(node="modbus-tcp-3", plugin=PLUGIN_MODBUS_TCP)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']  
        response = api.add_node(node="modbus-tcp-4", plugin=PLUGIN_MODBUS_TCP)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']
        response = api.add_node(node="mqtt-1", plugin=PLUGIN_MQTT)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response =api.get_nodes(type=NEU_NODE_DRIVER)
        assert 200 == response.status_code
        assert 'modbus-tcp-1' == response.json()['nodes'][0]['name']
        assert 'modbus-tcp-2' == response.json()['nodes'][1]['name']
        assert 'modbus-tcp-3' == response.json()['nodes'][2]['name']
        assert 'modbus-tcp-4' == response.json()['nodes'][3]['name']

        response =api.get_nodes(type=NEU_NODE_APP)
        assert 200 == response.status_code
        assert 'mqtt-1' == response.json()['nodes'][0]['name']

        p.stop()

    @description(given="delete adapters and restart Neuron", when="get the deleted adapters", then="get failed")
    def test_add_deleted_node_and_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.del_node(node="modbus-tcp-4")
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response =api.get_nodes(type=NEU_NODE_DRIVER)
        assert 200 == response.status_code
        node_names = [node['name'] for node in response.json()['nodes']]
        assert "modbus-tcp-4" not in node_names

        response =api.get_nodes(type=NEU_NODE_APP)
        assert 200 == response.status_code
        assert 'mqtt-1' == response.json()['nodes'][0]['name']

        p.stop()

    @description(given="add setted adapters and restart Neuron", when="get the setted adapters", then="return correct information")
    def test_get_setted_node_and_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.modbus_tcp_node_setting(node='modbus-tcp-1', port=502)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']
        response = api.modbus_tcp_node_setting(node='modbus-tcp-2', port=502)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']
        response = api.mqtt_node_setting(node='mqtt-1')
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response =api.get_node_setting(node='modbus-tcp-1')
        assert 200 == response.status_code
        assert 'modbus-tcp-1' == response.json()['node']
        assert 502 ==response.json()['params']['port']
        assert 3000 ==response.json()['params']['timeout']
        assert '127.0.0.1' ==response.json()['params']['host']

        response =api.get_node_setting(node='modbus-tcp-2')
        assert 200 == response.status_code
        assert 'modbus-tcp-2' == response.json()['node']
        assert 502 ==response.json()['params']['port']
        assert 3000 ==response.json()['params']['timeout']
        assert '127.0.0.1' ==response.json()['params']['host']

        response =api.get_node_setting(node='mqtt-1')
        assert 200 == response.status_code
        assert 'mqtt-1' == response.json()['node']
        assert 'neuron_aBcDeF' ==response.json()['params']['client-id']
        assert 100 ==response.json()['params']['cache-sync-interval']
        assert 0 ==response.json()['params']['format']
        assert False ==response.json()['params']['ssl']
        assert 'broker.emqx.io' ==response.json()['params']['host']
        assert 1883 ==response.json()['params']['port']
        assert '' ==response.json()['params']['username']
        assert '' ==response.json()['params']['password']
        assert False ==response.json()['params']['offline-cache']
        assert '/neuron/mqtt-1/write/req' ==response.json()['params']['write-req-topic']
        assert '/neuron/mqtt-1/write/resp' ==response.json()['params']['write-resp-topic']

        p.stop()

    @description(given="update adapter and restart Neuron", when="get the updated adapter", then="return correct information")
    def test_update_node_and_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.modbus_tcp_node_setting(node='modbus-tcp-1', port=60502)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response =api.get_node_setting(node='modbus-tcp-1')
        assert 200 == response.status_code
        assert 'modbus-tcp-1' == response.json()['node']
        assert 60502 ==response.json()['params']['port']
        assert 3000 ==response.json()['params']['timeout']
        assert '127.0.0.1' ==response.json()['params']['host']

        p.stop()

    @description(given="add groups and restart Neuron", when="get the groups", then="return correct information")
    def test_get_group_after_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.add_group(node='modbus-tcp-1', group='group-1', interval=1000)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']
        response = api.add_group(node='modbus-tcp-1', group='group-2', interval=2000)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']
        response = api.add_group(node='modbus-tcp-1', group='group-3')
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response =api.get_group(node='modbus-tcp-1')
        assert 200 == response.status_code
        assert 'group-1' == response.json()['groups'][0]['name']
        assert 1000 == response.json()['groups'][0]['interval']

        assert 'group-2' == response.json()['groups'][1]['name']
        assert 2000 == response.json()['groups'][1]['interval']

        assert 'group-3' == response.json()['groups'][2]['name']
        assert 100 == response.json()['groups'][2]['interval']

        p.stop()

    @description(given="delete group and restart Neuron", when="get the deleted group", then="get failed")
    def test_get_deleted_group_after_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.del_group(node='modbus-tcp-1', group='group-3')
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response =api.get_group(node='modbus-tcp-1')
        assert 200 == response.status_code
        assert 'group-1' == response.json()['groups'][0]['name']
        assert 1000 == response.json()['groups'][0]['interval']
        assert 'group-2' == response.json()['groups'][1]['name']
        assert 2000 == response.json()['groups'][1]['interval']

        group_names = [group['name'] for group in response.json()['groups']]
        assert "group-3" not in group_names

        p.stop()

    @description(given="add tags and restart Neuron", when="get the tags", then="return correct information")
    def test_get_tag_after_restart(self):
        p = process.NeuronProcess()
        p.start()

        api.add_tags_check(node='modbus-tcp-1', group='group-1', tags=hold_bit)
        api.add_tags_check(node='modbus-tcp-1', group='group-1', tags=hold_int16)
        api.add_tags_check(node='modbus-tcp-1', group='group-1', tags=hold_uint16)

        p.stop()
        p.start()

        response =api.get_tags(node='modbus-tcp-1', group='group-1')
        assert 200 == response.status_code
        assert 'hold_bit' == response.json()['tags'][0]['name']
        assert NEU_TYPE_BIT == response.json()['tags'][0]['type']
        assert NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE == response.json()['tags'][0]['attribute']
        assert '1!400001.15' == response.json()['tags'][0]['address']
        assert 0 == response.json()['tags'][0]['precision']
        assert 0.0 == response.json()['tags'][0]['decimal']
        assert '' == response.json()['tags'][0]['description']

        assert 'hold_int16' == response.json()['tags'][1]['name']
        assert NEU_TYPE_INT16 == response.json()['tags'][1]['type']
        assert NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE == response.json()['tags'][1]['attribute']
        assert '1!400001' == response.json()['tags'][1]['address']
        assert 0 == response.json()['tags'][1]['precision']
        assert 0.0 == response.json()['tags'][1]['decimal']
        assert '' == response.json()['tags'][1]['description']

        assert 'hold_uint16' == response.json()['tags'][2]['name']
        assert NEU_TYPE_UINT16 == response.json()['tags'][2]['type']
        assert NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE == response.json()['tags'][2]['attribute']
        assert '1!400002' == response.json()['tags'][2]['address']
        assert 0 == response.json()['tags'][2]['precision']
        assert 0.0 == response.json()['tags'][2]['decimal']
        assert '' == response.json()['tags'][2]['description']

        p.stop()

    @description(given="delete tag and restart Neuron", when="get the deleted tag", then="get failed")
    def test_get_deleted_tag_after_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.del_tags(node='modbus-tcp-1', group='group-1', tags=["hold_bit"])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response =api.get_tags(node='modbus-tcp-1', group='group-1')
        assert 'hold_int16' == response.json()['tags'][0]['name']
        assert NEU_TYPE_INT16 == response.json()['tags'][0]['type']
        assert NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE == response.json()['tags'][0]['attribute']
        assert '1!400001' == response.json()['tags'][0]['address']
        assert 0 == response.json()['tags'][0]['precision']
        assert 0.0 == response.json()['tags'][0]['decimal']
        assert '' == response.json()['tags'][0]['description']

        assert 'hold_uint16' == response.json()['tags'][1]['name']
        assert NEU_TYPE_UINT16 == response.json()['tags'][1]['type']
        assert NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE == response.json()['tags'][1]['attribute']
        assert '1!400002' == response.json()['tags'][1]['address']
        assert 0 == response.json()['tags'][1]['precision']
        assert 0.0 == response.json()['tags'][1]['decimal']
        assert '' == response.json()['tags'][1]['description']

        tag_names = [tag['name'] for tag in response.json()['tags']]
        assert "hold_bit" not in tag_names

        p.stop()

    @description(given="update tag and restart Neuron", when="get the updated tag", then="return correct information")
    def test_update_tag_and_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.update_tags(node="modbus-tcp-1", group="group-1", tags=hold_int16_to_int32)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response =api.get_tags(node='modbus-tcp-1', group='group-1')
        assert 'hold_int16' == response.json()['tags'][0]['name']
        assert NEU_TYPE_INT32 == response.json()['tags'][0]['type']
        assert NEU_TAG_ATTRIBUTE_RW == response.json()['tags'][0]['attribute']
        assert '1!400002' == response.json()['tags'][0]['address']
        assert 0 == response.json()['tags'][0]['precision']
        assert 0.0 == response.json()['tags'][0]['decimal']
        assert '' == response.json()['tags'][0]['description']

        assert 'hold_uint16' == response.json()['tags'][1]['name']
        assert NEU_TYPE_UINT16 == response.json()['tags'][1]['type']
        assert NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE == response.json()['tags'][1]['attribute']
        assert '1!400002' == response.json()['tags'][1]['address']
        assert 0 == response.json()['tags'][1]['precision']
        assert 0.0 == response.json()['tags'][1]['decimal']
        assert '' == response.json()['tags'][1]['description']

        p.stop()

    @description(given="add subscriptions and restart Neuron", when="get the subscriptions", then="return correct information")
    def test_get_subscription_after_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.subscribe_groups(app='mqtt-1', groups=[{"driver": "modbus-tcp-1", "group": "group-1", "params":{"topic":"/neuron"}},
                                                              {"driver": "modbus-tcp-1", "group": "group-2"}])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response = api.get_subscribe_group(app='mqtt-1')
        assert 200 == response.status_code
        assert 'modbus-tcp-1' == response.json()["groups"][0]["driver"]
        assert 'group-1' == response.json()["groups"][0]["group"]
        assert '/neuron' == response.json()["groups"][0]["params"]['topic']
        
        assert 'modbus-tcp-1' == response.json()["groups"][1]["driver"]
        assert 'group-2' == response.json()["groups"][1]["group"]

        p.stop()

    @description(given="update driver name and restart Neuron", when="get the setting, groups, tags and subscriptions", then="return correct information")
    def test_update_driver_name_and_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.update_node(node='modbus-tcp-1', new_name='modbus-tcp-new')
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response =api.get_node_setting(node='modbus-tcp-new')
        assert 200 == response.status_code
        assert 'modbus-tcp-new' == response.json()['node']
        assert 60502 ==response.json()['params']['port']
        assert 3000 ==response.json()['params']['timeout']
        assert '127.0.0.1' ==response.json()['params']['host']

        response =api.get_tags(node='modbus-tcp-new', group='group-1')
        assert 'hold_int16' == response.json()['tags'][0]['name']
        assert NEU_TYPE_INT32 == response.json()['tags'][0]['type']
        assert NEU_TAG_ATTRIBUTE_RW == response.json()['tags'][0]['attribute']
        assert '1!400002' == response.json()['tags'][0]['address']
        assert 0 == response.json()['tags'][0]['precision']
        assert 0.0 == response.json()['tags'][0]['decimal']
        assert '' == response.json()['tags'][0]['description']

        assert 'hold_uint16' == response.json()['tags'][1]['name']
        assert NEU_TYPE_UINT16 == response.json()['tags'][1]['type']
        assert NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE == response.json()['tags'][1]['attribute']
        assert '1!400002' == response.json()['tags'][1]['address']
        assert 0 == response.json()['tags'][1]['precision']
        assert 0.0 == response.json()['tags'][1]['decimal']
        assert '' == response.json()['tags'][1]['description']

        response = api.get_subscribe_group(app='mqtt-1')
        assert 200 == response.status_code
        assert 'modbus-tcp-new' == response.json()["groups"][0]["driver"]
        assert 'group-1' == response.json()["groups"][0]["group"]
        assert '/neuron' == response.json()["groups"][0]["params"]['topic']
        
        assert 'modbus-tcp-new' == response.json()["groups"][1]["driver"]
        assert 'group-2' == response.json()["groups"][1]["group"]

        p.stop()

    @description(given="update app name and restart Neuron", when="get the setting, and subscriptions", then="return correct information")
    def test_update_app_name_and_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.update_node(node='mqtt-1', new_name='mqtt-new')
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response =api.get_node_setting(node='mqtt-new')
        assert 200 == response.status_code
        assert 'mqtt-new' == response.json()['node']
        assert 'neuron_aBcDeF' ==response.json()['params']['client-id']
        assert 100 ==response.json()['params']['cache-sync-interval']
        assert 0 ==response.json()['params']['format']
        assert False ==response.json()['params']['ssl']
        assert 'broker.emqx.io' ==response.json()['params']['host']
        assert 1883 ==response.json()['params']['port']
        assert '' ==response.json()['params']['username']
        assert '' ==response.json()['params']['password']
        assert False ==response.json()['params']['offline-cache']
        assert '/neuron/mqtt-1/write/req' ==response.json()['params']['write-req-topic']
        assert '/neuron/mqtt-1/write/resp' ==response.json()['params']['write-resp-topic']

        response = api.get_subscribe_group(app='mqtt-new')
        assert 200 == response.status_code
        assert 'modbus-tcp-new' == response.json()["groups"][0]["driver"]
        assert 'group-1' == response.json()["groups"][0]["group"]
        assert '/neuron' == response.json()["groups"][0]["params"]['topic']
        
        assert 'modbus-tcp-new' == response.json()["groups"][1]["driver"]
        assert 'group-2' == response.json()["groups"][1]["group"]

        p.stop()

    @description(given="unsubscribe group and restart Neuron", when="get the subscriptions", then="return correct information")
    def test_unsubscribe_and_restart(self):
        p = process.NeuronProcess()
        p.start()

        response = api.unsubscribe_group(app='mqtt-new', driver='modbus-tcp-new', group='group-2')
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        p.stop()
        p.start()

        response = api.get_subscribe_group(app='mqtt-new')
        assert 200 == response.status_code
        assert 'modbus-tcp-new' == response.json()["groups"][0]["driver"]
        assert 'group-1' == response.json()["groups"][0]["group"]
        assert '/neuron' == response.json()["groups"][0]["params"]['topic']
        
        group_names = [group['group'] for group in response.json()['groups']]
        assert "group-2" not in group_names

        p.stop()

    @description(given="upload pem in certs", when="login", then="success")
    def test_jwt(self):
        if not os.path.exists('./build/certs/'):
            os.makedirs('./build/certs/')

        shutil.copyfile('./build/config/neuron.pem', './build/certs/neuron.pem')

        p = process.NeuronProcess()
        p.start()

        response = api.login()
        assert 200 == response.status_code

        shutil.rmtree('./build/certs/')

        p.stop()

    @description(given="new schema", when="upload schema , restart and add tags", then="success")
    def test_all_datatag_type(self):
        process.remove_persistence()
        dst_file = './build/plugins/schema/modbus-tcp.json'
        dst_backup = dst_file + '_backup'
        os.rename(dst_file, dst_backup)

        try:
            shutil.copy2('./tests/ft/tag/modbus-tcp.json', dst_file)
        except Exception as e:
            if os.path.exists(dst_backup):
                os.rename(dst_backup, dst_file)
            return
        
        p = process.NeuronProcess()
        p.start()

        api.add_node_check(node="modbus-tcp-1", plugin=PLUGIN_MODBUS_TCP)
        api.add_group_check(node='modbus-tcp-1', group='group-tag', interval=1000)

        api.add_tags(node='modbus-tcp-1', group='group-tag', tags=hold_int8)
        api.add_tags(node='modbus-tcp-1', group='group-tag', tags=hold_uint8)
        api.add_tags(node='modbus-tcp-1', group='group-tag', tags=hold_bool)
        api.add_tags(node='modbus-tcp-1', group='group-tag', tags=hold_word)
        api.add_tags(node='modbus-tcp-1', group='group-tag', tags=hold_dword)
        api.add_tags(node='modbus-tcp-1', group='group-tag', tags=hold_lword)
        api.add_tags(node='modbus-tcp-1', group='group-tag', tags=hold_ptr)
        api.add_tags(node='modbus-tcp-1', group='group-tag', tags=hold_dt)
        api.add_tags(node='modbus-tcp-1', group='group-tag', tags=hold_t)

        response = api.del_group(node="modbus-tcp-1", group='group-tag')
        assert 200 == response.status_code

        p.stop()

        os.remove(dst_file)
        os.rename(dst_backup, dst_file)