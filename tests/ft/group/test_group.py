import neuron.api as api
from neuron.config import *
from neuron.common import *
from neuron.error import *


class TestGroup:

    @description(given="non-existent node", when="add group under non-existent node", then="add failed")
    def test_add_group_non_existent_node(self):
        response = api.add_group(node="non-existent", group='group')
        assert 404 == response.status_code
        assert NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="add node", when="add group under existent node", then="add success")
    def test_add_group_existent_node(self):
        api.add_node(node='modbus-tcp', plugin=PLUGIN_MODBUS_TCP)
        response = api.add_group(node="modbus-tcp", group='group')
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="add node group", when="get group", then="return the group config success")
    def test_get_group(self):
        response = api.get_group(node='modbus-tcp')
        assert 200 == response.status_code
        assert 0 == response.json()['groups'][0]['tag_count']
        assert 100 == response.json()['groups'][0]['interval']
        assert 'group' == response.json()['groups'][0]['name']

    @description(given="non-existent group", when="delete non-existent group", then="delete failed")
    def test_delete_non_existent_group(self):
        response = api.del_group(node="modbus-tcp", group='non-existent')
        assert 404 == response.status_code
        assert NEU_ERR_GROUP_NOT_EXIST == response.json()['error']

    @description(given="non-existent node", when="delete group under non-existent node", then="delete failed")
    def test_delete_group_non_existent_node(self):
        response = api.del_group(node="non-existent", group='group')
        assert 404 == response.status_code
        assert NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="existent node group", when="delete group under existent node", then="delete success")
    def test_delete_group(self):
        api.add_group(node="modbus-tcp", group='group1')
        response = api.del_group(node="modbus-tcp", group='group1')
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="existent node", when="add group with interval < 100 under existent node", then="add failed")
    def test_add_group_wrong_interval(self):
        response = api.add_group(node="modbus-tcp", group='group', interval=99)
        assert 400 == response.status_code
        assert NEU_ERR_GROUP_PARAMETER_INVALID == response.json()['error']

    @description(given="add app node", when="add group under app node", then="add failed")
    def test_add_group_app_node(self):
        api.add_node(node='mqtt', plugin=PLUGIN_MQTT)
        response = api.add_group(node="mqtt", group='group')
        assert 409 == response.status_code
        assert NEU_ERR_GROUP_NOT_ALLOW == response.json()['error']

    @description(given="non-existent node", when="update group under non-existent node", then="update failed")
    def test_update_group_non_existent_node(self):
        response = api.update_group(node="non-existent", group='group', new_name="new_group")
        assert 404 == response.status_code
        assert NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="long node name", when="update group", then="update failed")
    def test_update_group_long_node_name(self):
        response = api.update_group(node='a'*200, group='group', new_name="new_group")
        assert 400 == response.status_code
        assert NEU_ERR_NODE_NAME_TOO_LONG == response.json()['error']

    @description(given="non-existent group", when="update group with non-existent group", then="update failed")
    def test_update_group_non_existent_group(self):
        response = api.update_group(node="modbus-tcp", group='non-existent', new_name="new_group")
        assert 404 == response.status_code
        assert NEU_ERR_GROUP_NOT_EXIST == response.json()['error']

    @description(given="long group name", when="update group", then="update failed")
    def test_update_group_long_group_name(self):
        response = api.update_group(node="modbus-tcp", group='group', new_name='a'*200)
        assert 400 == response.status_code
        assert NEU_ERR_GROUP_NAME_TOO_LONG == response.json()['error']

    @description(given="interval < 100", when="update group", then="update failed")
    def test_update_group_small_interval(self):
        response = api.update_group(node="modbus-tcp", group='group', interval=12345678910)
        assert 400 == response.status_code
        assert NEU_ERR_GROUP_PARAMETER_INVALID == response.json()['error']

    @description(given="valid interval", when="update group", then="update success")
    def test_update_group_valid_interval(self):
        response = api.update_group(node="modbus-tcp", group='group', interval=2000)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="null new name", when="update group", then="update failed")
    def test_update_group_empty_name(self):
        response = api.update_group(node="modbus-tcp", group='group', new_name="")
        assert 400 == response.status_code
        assert NEU_ERR_BODY_IS_WRONG == response.json()['error']

    @description(given="conflict group name", when="update group", then="update failed")
    def test_update_group_conflict_name(self):
        api.add_group(node="modbus-tcp", group='group1')
        response = api.update_group(node="modbus-tcp", group='group1', new_name="group")
        assert 409 == response.status_code
        assert NEU_ERR_GROUP_EXIST == response.json()['error']

    @description(given="valid new name", when="update group", then="update success")
    def test_update_group_name(self):
        response = api.update_group(node="modbus-tcp", group='group', new_name="group2")
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="valid new name and interval", when="update group", then="update success")
    def test_update_group_name_interval(self):
        response = api.update_group(node="modbus-tcp", group='group2', new_name="group", interval=3000)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="mqtt subscribes the group", when="update group", then="subscriptions should be updated")
    def test_update_group_subscription(self):
        response = api.subscribe_group(app="mqtt", driver="modbus-tcp", group="group")
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_subscribe_group(app="mqtt")
        assert 200 == response.status_code
        assert "group" == response.json()['groups'][0]['group']

        response = api.update_group(node="modbus-tcp", group='group', new_name="group2")
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_subscribe_group(app="mqtt")
        assert 200 == response.status_code
        assert "group2" == response.json()['groups'][0]['group']