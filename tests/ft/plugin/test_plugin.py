import os
import sys
import json
import base64

import neuron.api as api
from neuron.config import *
from neuron.common import *
from neuron.error import *


class TestPlugin:

    with open('tests/ft/plugin/test_library_data.json') as f:
        test_data = json.load(f)
        test_data['so_file_c1_err'] = "asdf"
    with open('build/tests/plugins/libplugin-c1.so', 'rb') as f:
        test_data['so_file_c1'] = str(
            base64.b64encode(f.read()), encoding='utf-8')
    with open('build/tests/plugins/libplugin-c1_2.so', 'rb') as f:
        test_data['so_file_c1_2'] = str(
            base64.b64encode(f.read()), encoding='utf-8')
    with open('build/tests/plugins/libplugin-c1verr.so', 'rb') as f:
        test_data['so_file_c1verr'] = str(
            base64.b64encode(f.read()), encoding='utf-8')
    with open('build/tests/plugins/libplugin-c1_2verr.so', 'rb') as f:
        test_data['so_file_c1_2verr'] = str(
            base64.b64encode(f.read()), encoding='utf-8')
    with open('build/tests/plugins/libplugin-s1.so', 'rb') as f:
        test_data['so_file_s1'] = str(
            base64.b64encode(f.read()), encoding='utf-8')
    with open('build/tests/plugins/libplugin-s1_2.so', 'rb') as f:
        test_data['so_file_s1_2'] = str(
            base64.b64encode(f.read()), encoding='utf-8')
    with open('build/tests/plugins/libplugin-sc1.so', 'rb') as f:
        test_data['so_file_sc1'] = str(
            base64.b64encode(f.read()), encoding='utf-8')
    with open('build/tests/plugins/libplugin-sc1_2.so', 'rb') as f:
        test_data['so_file_sc1_2'] = str(
            base64.b64encode(f.read()), encoding='utf-8')
    with open('build/tests/plugins/schema/c1.json', 'rb') as f:
        test_data['schema_file'] = str(
            base64.b64encode(f.read()), encoding='utf-8')
    with open('tests/ft/plugin/plugin_data1.txt', 'rb') as f:
        test_data["so_file_arm64"] = str(f.read(), encoding='utf-8')
    with open('tests/ft/plugin/plugin_data2.txt', 'rb') as f:
        test_data["so_file_clib"] = str(f.read(), encoding='utf-8')

    @description(given="incorrect configuration", when="add plugin", then="fail")
    def test_01_add_plugin_fail0(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = test_data['so_file_c1_err']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_LIBRARY_MODULE_INVALID == response.json().get("error")

    @description(given="incorrect configuration", when="add plugin", then="fail")
    def test_02_add_plugin_fail1(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = ''
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_BODY_IS_WRONG == response.json().get("error")

    @description(given="incorrect configuration", when="add plugin", then="fail")
    def test_03_add_plugin_fail2(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = test_data['so_file_c1']
        plugin_data['schema_file'] = ''

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_BODY_IS_WRONG == response.json().get("error")

    @description(given="incorrect configuration", when="add plugin", then="fail")
    def test_04_add_plugin_fail3(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_sc1']
        plugin_data['so_file'] = test_data['so_file_sc1']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_LIBRARY_MODULE_KIND_NOT_SUPPORT == response.json().get("error")

    @description(given="correct configuration", when="add plugin", then="success")
    def test_05_add_plugin_success0(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = test_data['so_file_c1']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json().get("error")

    @description(given="correct configuration", when="add plugin", then="success")
    def test_06_add_plugin_success1(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_s1']
        plugin_data['so_file'] = test_data['so_file_s1']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json().get("error")

    @description(given="correct configuration", when="add plugin", then="fail")
    def test_04_add_plugin_fail4(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = test_data['so_file_c1']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 409 == response.status_code
        assert NEU_ERR_LIBRARY_NAME_CONFLICT == response.json().get("error")

    @description(given="incorrect configuration", when="add plugin", then="fail")
    def test_04_add_plugin_fail5(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = "libtest.so"
        plugin_data['so_file'] = test_data['so_file_c1']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_LIBRARY_NAME_NOT_CONFORM == response.json().get("error")

    @description(given="incorrect configuration", when="add plugin", then="fail")
    def test_04_add_plugin_fail6(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = "libplugin-test.so"
        plugin_data['so_file'] = test_data['so_file_arm64']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_LIBRARY_ARCH_NOT_SUPPORT == response.json().get("error")

    @description(given="incorrect configuration", when="add plugin", then="fail")
    def test_04_add_plugin_fail7(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = "libplugin-test.so"
        plugin_data['so_file'] = test_data['so_file_clib']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_LIBRARY_CLIB_NOT_MATCH == response.json().get("error")

    @description(given="incorrect configuration", when="add plugin", then="fail")
    def test_04_add_plugin_fail8(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = "libplugin-test.so"
        plugin_data['so_file'] = test_data['so_file_c1verr']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.add_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_LIBRARY_MODULE_VERSION_NOT_MATCH == response.json().get("error")

    @description(given="incorrect configuration", when="update plugin", then="fail")
    def test_07_update_plugin_fail0(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = test_data['so_file_sc1']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.updata_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_LIBRARY_UPDATE_FAIL == response.json().get("error")

    @description(given="incorrect configuration", when="update plugin", then="fail")
    def test_08_update_plugin_fail1(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = '123'
        plugin_data['so_file'] = test_data['so_file_c1_2']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.updata_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_LIBRARY_NAME_NOT_CONFORM == response.json().get("error")

    @description(given="incorrect configuration", when="update plugin", then="fail")
    def test_09_update_plugin_fail2(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = test_data['so_file_c1_2']
        plugin_data['schema_file'] = ''

        response = api.updata_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_BODY_IS_WRONG == response.json().get("error")

    @description(given="incorrect configuration", when="update plugin", then="success")
    def test_09_update_plugin_success0(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = test_data['so_file_c1_2']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.updata_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json().get("error")
        response = api.get_plugins()
        assert 200 == response.status_code
        for plugin in response.json().get("plugins"):
            if plugin['name'] == 'c1':
                assert plugin['version'] == '2.9.99'

    @description(given="incorrect configuration", when="update plugin", then="fail")
    def test_10_update_plugin_fail3(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = ''
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.updata_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_BODY_IS_WRONG == response.json().get("error")

    @description(given="incorrect configuration", when="update plugin", then="fail")
    def test_10_update_plugin_fail4(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = test_data['so_file_c1_2verr']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.updata_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 400 == response.status_code
        assert NEU_ERR_LIBRARY_MODULE_VERSION_NOT_MATCH == response.json().get("error")

    @description(given="correct configuration", when="update plugin", then="success")
    def test_11_update_plugin_success0(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_s1']
        plugin_data['so_file'] = test_data['so_file_s1_2']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.updata_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json().get("error")

    @description(given="correct configuration", when="add south node", then="success")
    def test_12_add_node_success(self):
        node_data = {}
        node_data['name'] = 'c1-a'
        node_data['plugin'] = 'c1'

        response = api.add_node(node_data['name'], node_data['plugin'])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json().get("error")

    @description(given="correct configuration", when="config node", then="success")
    def test_13_configure_node_success(self):
        test_data = TestPlugin.test_data
        node_data = test_data['node_config']

        response = api.node_setting(node_data['node'], node_data['params'])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json().get("error")

    @description(given="correct configuration", when="update plugin", then="success")
    def test_14_update_plugin_success1(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['library'] = test_data['library_c1']
        plugin_data['so_file'] = test_data['so_file_c1_2']
        plugin_data['schema_file'] = test_data['schema_file']

        response = api.updata_plugin(
            library_name=plugin_data['library'], so_file=plugin_data['so_file'], schema_file=plugin_data['schema_file'])
        assert 409 == response.status_code
        assert NEU_ERR_LIBRARY_IN_USE == response.json().get("error")

    @description(given="correct configuration", when="del plugin", then="fail")
    def test_15_del_plugin_fail0(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['plugin'] = 'c1'

        response = api.del_plugin(plugin_name=plugin_data['plugin'])
        assert 409 == response.status_code
        assert NEU_ERR_LIBRARY_IN_USE == response.json().get("error")

    @description(given="correct configuration", when="del plugin", then="fail")
    def test_16_del_plugin_fail0(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['plugin'] = 's1'

        response = api.del_plugin(plugin_name=plugin_data['plugin'])
        assert 409 == response.status_code
        assert NEU_ERR_LIBRARY_SYSTEM_NOT_ALLOW_DEL == response.json().get("error")

    @description(given="correct configuration", when="del south node", then="success")
    def test_17_del_node_success(self):
        node_data = {}
        node_data['name'] = 'c1-a'

        response = api.del_node(node_data['name'])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json().get("error")

    @description(given="correct configuration", when="del plugin", then="success")
    def test_18_del_plugin_sucess(self):
        test_data = TestPlugin.test_data
        plugin_data = {}
        plugin_data['plugin'] = 'c1'

        response = api.del_plugin(plugin_name=plugin_data['plugin'])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json().get("error")
