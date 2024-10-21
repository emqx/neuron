import neuron.api as api
from neuron.config import *
from neuron.common import *
from neuron.error import *


hold_int16 = [{"name": "hold_int16", "address": "1!400001",
               "attribute": NEU_TAG_ATTRIBUTE_RW, "type": NEU_TYPE_INT16}]
hold_int16_to_int32 = [{"name": "hold_int16", "address": "1!400002",
               "attribute": NEU_TAG_ATTRIBUTE_RW, "type": NEU_TYPE_INT32}]
hold_int16_bit = [{"name": "hold_int16_type", "address": "1!400001",
               "attribute": NEU_TAG_ATTRIBUTE_RW, "type": NEU_TYPE_INT16},
               {"name": "hold_bit_type", "address": "1!000001",
               "attribute": NEU_TAG_ATTRIBUTE_RW, "type": NEU_TYPE_BIT}]
hold_int16_description = [{"name": "hold_int16_description", "address": "1!400001",
               "attribute": NEU_TAG_ATTRIBUTE_RW, "type": NEU_TYPE_INT16, "description": "description info"}]
hold_int16_decimal = [{"name": "hold_int16_decimal", "address": "1!400001",
               "attribute": NEU_TAG_ATTRIBUTE_RW, "type": NEU_TYPE_INT16, "decimal": 0.1}]
hold_int16_bias = [{"name": "hold_int16_bias", "address": "1!400001",
               "attribute": NEU_TAG_ATTRIBUTE_READ, "type": NEU_TYPE_INT16, "bias": 1.0}]
none= [{"name": "none", "address": "1!400001",
               "attribute": NEU_TAG_ATTRIBUTE_RW, "type": NEU_TYPE_INT16}]

class TestTag:

    @description(given="multiple non-existing groups and multiple correct tags", 
                 when="adding", 
                 then="successfully added")
    def test_adding_correct_gtags_nonexisting_groups(self, class_setup_and_teardown):
        response = api.add_node(node='modbus-tcp-tag-test', plugin=PLUGIN_MODBUS_TCP)
        assert 200 == response.status_code
        assert 0 == response.json()['error']

        response = api.add_gtags(
            node='modbus-tcp-tag-test', 
            groups=[
                {
                    "group": "group_1",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400001",
                            "attribute": 3,
                            "type": 3,
                            "decimal": 0.01
                        },
                        {
                            "name": "tag2",
                            "address": "1!400002",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                },
                {
                    "group": "group_2",
                    "interval": 300,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400003",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        },
                        {
                            "name": "tag2",
                            "address": "1!400004",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                },
                {
                    "group": "group_3",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400001",
                            "attribute": 1,
                            "type": 3,
                            "bias": 1.0,
                        },
                        {
                            "name": "tag2",
                            "address": "1!400002",
                            "attribute": 1,
                            "type": 9,
                            "decimal": 0.01,
                            "bias": 1.0,
                        },
                    ]
                },
            ]
        )

        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        # check data persisted
        class_setup_and_teardown.restart()

        response = api.get_group()
        assert 200       == response.status_code
        assert 'group_1' == response.json()['groups'][0]['group']
        assert 3000      == response.json()['groups'][0]['interval']
        assert 2         == response.json()['groups'][0]['tag_count']
        assert 'group_2' == response.json()['groups'][1]['group']
        assert 300       == response.json()['groups'][1]['interval']
        assert 2         == response.json()['groups'][1]['tag_count']
        assert 'group_3' == response.json()['groups'][2]['group']
        assert 3000      == response.json()['groups'][2]['interval']
        assert 2         == response.json()['groups'][2]['tag_count']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_1')
        assert 200        == response.status_code
        assert 'tag1'     == response.json()['tags'][0]['name']
        assert '1!400001' == response.json()['tags'][0]['address']
        assert 0.01       == response.json()['tags'][0]['decimal']
        assert 3          == response.json()['tags'][0]['type']
        assert 3          == response.json()['tags'][0]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_1')
        assert 200        == response.status_code
        assert 'tag2'     == response.json()['tags'][1]['name']
        assert '1!400002' == response.json()['tags'][1]['address']
        assert 3          == response.json()['tags'][1]['precision']
        assert 9          == response.json()['tags'][1]['type']
        assert 3          == response.json()['tags'][1]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_2')
        assert 200        == response.status_code
        assert 'tag1'     == response.json()['tags'][0]['name']
        assert '1!400003' == response.json()['tags'][0]['address']
        assert 3          == response.json()['tags'][1]['precision']
        assert 9          == response.json()['tags'][1]['type']
        assert 3          == response.json()['tags'][1]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_2')
        assert 200        == response.status_code
        assert 'tag2'     == response.json()['tags'][1]['name']
        assert '1!400004' == response.json()['tags'][1]['address']
        assert 3          == response.json()['tags'][1]['precision']
        assert 9          == response.json()['tags'][1]['type']
        assert 3          == response.json()['tags'][1]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_3')
        assert 200        == response.status_code
        assert 'tag1'     == response.json()['tags'][0]['name']
        assert '1!400001' == response.json()['tags'][0]['address']
        assert 1.0        == response.json()['tags'][0]['bias']
        assert 3          == response.json()['tags'][0]['type']
        assert 1          == response.json()['tags'][0]['attribute']
        assert 'tag2'     == response.json()['tags'][1]['name']
        assert '1!400002' == response.json()['tags'][1]['address']
        assert 0.01       == response.json()['tags'][1]['decimal']
        assert 1.0        == response.json()['tags'][1]['bias']
        assert 9          == response.json()['tags'][1]['type']
        assert 1          == response.json()['tags'][1]['attribute']

    @description(given="multiple groups including partly existing ones and multiple correct tags", when="adding", then="successfully added")
    def test_adding_correct_gtags_existing_groups(self):
        response = api.add_gtags(
            node='modbus-tcp-tag-test', 
            groups=[
                {
                    "group": "group_1",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag3",
                            "address": "1!400005",
                            "attribute": 3,
                            "type": 3,
                            "decimal": 0.01
                        },
                        {
                            "name": "tag4",
                            "address": "1!400006",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                },
                {
                    "group": "group_4",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400007",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        },
                        {
                            "name": "tag2",
                            "address": "1!400008",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                }
            ]
        )

        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_group()
        assert 200       == response.status_code
        assert 'group_1' == response.json()['groups'][0]['group']
        assert 3000      == response.json()['groups'][0]['interval']
        assert 4         == response.json()['groups'][0]['tag_count']
        assert 'group_2' == response.json()['groups'][1]['group']
        assert 300       == response.json()['groups'][1]['interval']
        assert 2         == response.json()['groups'][1]['tag_count']
        assert 'group_3' == response.json()['groups'][2]['group']
        assert 3000      == response.json()['groups'][2]['interval']
        assert 2         == response.json()['groups'][2]['tag_count']
        assert 'group_4' == response.json()['groups'][3]['group']
        assert 3000      == response.json()['groups'][3]['interval']
        assert 2         == response.json()['groups'][3]['tag_count']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_1')
        assert 200        == response.status_code
        assert 'tag1'     == response.json()['tags'][0]['name']
        assert '1!400001' == response.json()['tags'][0]['address']
        assert 0.01       == response.json()['tags'][0]['decimal']
        assert 3          == response.json()['tags'][0]['type']
        assert 3          == response.json()['tags'][0]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_1')
        assert 200        == response.status_code
        assert 'tag2'     == response.json()['tags'][1]['name']
        assert '1!400002' == response.json()['tags'][1]['address']
        assert 3          == response.json()['tags'][1]['precision']
        assert 9          == response.json()['tags'][1]['type']
        assert 3          == response.json()['tags'][1]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_1')
        assert 200        == response.status_code
        assert 'tag3'     == response.json()['tags'][2]['name']
        assert '1!400005' == response.json()['tags'][2]['address']
        assert 0.01       == response.json()['tags'][2]['decimal']
        assert 3          == response.json()['tags'][2]['type']
        assert 3          == response.json()['tags'][2]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_1')
        assert 200        == response.status_code
        assert 'tag4'     == response.json()['tags'][3]['name']
        assert '1!400006' == response.json()['tags'][3]['address']
        assert 3          == response.json()['tags'][3]['precision']
        assert 9          == response.json()['tags'][3]['type']
        assert 3          == response.json()['tags'][3]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_2')
        assert 200        == response.status_code
        assert 'tag1'     == response.json()['tags'][0]['name']
        assert '1!400003' == response.json()['tags'][0]['address']
        assert 3          == response.json()['tags'][1]['precision']
        assert 9          == response.json()['tags'][1]['type']
        assert 3          == response.json()['tags'][1]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_2')
        assert 200        == response.status_code
        assert 'tag2'     == response.json()['tags'][1]['name']
        assert '1!400004' == response.json()['tags'][1]['address']
        assert 3          == response.json()['tags'][1]['precision']
        assert 9          == response.json()['tags'][1]['type']
        assert 3          == response.json()['tags'][1]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_3')
        assert 200        == response.status_code
        assert 'tag1'     == response.json()['tags'][0]['name']
        assert '1!400001' == response.json()['tags'][0]['address']
        assert 1.0        == response.json()['tags'][0]['bias']
        assert 3          == response.json()['tags'][0]['type']
        assert 1          == response.json()['tags'][0]['attribute']
        assert 'tag2'     == response.json()['tags'][1]['name']
        assert '1!400002' == response.json()['tags'][1]['address']
        assert 0.01       == response.json()['tags'][1]['decimal']
        assert 1.0        == response.json()['tags'][1]['bias']
        assert 9          == response.json()['tags'][1]['type']
        assert 1          == response.json()['tags'][1]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_4')
        assert 200        == response.status_code
        assert 'tag1'     == response.json()['tags'][0]['name']
        assert '1!400007' == response.json()['tags'][0]['address']
        assert 3          == response.json()['tags'][1]['precision']
        assert 9          == response.json()['tags'][1]['type']
        assert 3          == response.json()['tags'][1]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_4')
        assert 200        == response.status_code
        assert 'tag2'     == response.json()['tags'][1]['name']
        assert '1!400008' == response.json()['tags'][1]['address']
        assert 3          == response.json()['tags'][1]['precision']
        assert 9          == response.json()['tags'][1]['type']
        assert 3          == response.json()['tags'][1]['attribute']

    @description(given="multiple groups and tags with errors", when="adding", then="error reported, unable to add")
    def test_adding_gtags_type_err(self):
        response = api.add_gtags(
            node='modbus-tcp-tag-test', 
            groups=[
                {
                    "group": "group_4",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!000009",
                            "attribute": 3,
                            "type": 3,
                            "decimal": 0.01
                        },
                        {
                            "name": "tag4",
                            "address": "1!400010",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                },
                {
                    "group": "group_5",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400011",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        },
                        {
                            "name": "tag2",
                            "address": "1!400012",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                }
            ]
        )

        assert 400 == response.status_code
        assert NEU_ERR_TAG_TYPE_NOT_SUPPORT == response.json()['error']

        response = api.get_group()
        assert 200       == response.status_code
        assert 'group_1' == response.json()['groups'][0]['group']
        assert 3000      == response.json()['groups'][0]['interval']
        assert 4         == response.json()['groups'][0]['tag_count']
        assert 'group_2' == response.json()['groups'][1]['group']
        assert 300       == response.json()['groups'][1]['interval']
        assert 2         == response.json()['groups'][1]['tag_count']
        assert 'group_3' == response.json()['groups'][2]['group']
        assert 3000      == response.json()['groups'][2]['interval']
        assert 2         == response.json()['groups'][2]['tag_count']

    @description(given="multiple groups and conflicting tag names", when="adding", then="error reported, unable to add")
    def test_adding_gtags_name_err(self):
        response = api.add_gtags(
            node='modbus-tcp-tag-test', 
            groups=[
                {
                    "group": "group_1",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400001",
                            "attribute": 3,
                            "type": 3,
                            "decimal": 0.01
                        },
                        {
                            "name": "tag2",
                            "address": "1!400002",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                },
                {
                    "group": "group_2",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400003",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        },
                        {
                            "name": "tag2",
                            "address": "1!400004",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                }
            ]
        )

        assert 409 == response.status_code
        assert NEU_ERR_TAG_NAME_CONFLICT == response.json()['error']

        response = api.get_group()
        assert 200       == response.status_code
        assert 'group_1' == response.json()['groups'][0]['group']
        assert 3000      == response.json()['groups'][0]['interval']
        assert 4         == response.json()['groups'][0]['tag_count']
        assert 'group_2' == response.json()['groups'][1]['group']
        assert 300       == response.json()['groups'][1]['interval']
        assert 2         == response.json()['groups'][1]['tag_count']
        assert 'group_3' == response.json()['groups'][2]['group']
        assert 3000      == response.json()['groups'][2]['interval']
        assert 2         == response.json()['groups'][2]['tag_count']

    @description(given="multiple groups including same name tags in the first group", when="adding", then="add failed")
    def test_adding_same_name_gtags_first_group(self):
        response = api.add_gtags(
            node='modbus-tcp-tag-test', 
            groups=[
                {
                    "group": "group_f",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400001",
                            "attribute": 3,
                            "type": 3,
                            "decimal": 0.01
                        },
                        {
                            "name": "tag1",
                            "address": "1!400002",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                },
                {
                    "group": "group_s",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400001",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        },
                        {
                            "name": "tag2",
                            "address": "1!400002",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                }
            ]
        )

        assert 409 == response.status_code
        assert NEU_ERR_TAG_NAME_CONFLICT == response.json()['error']

    @description(given="multiple groups including same name tags in the second group", when="adding", then="add failed")
    def test_adding_same_name_gtags_second_group(self):
        response = api.add_gtags(
            node='modbus-tcp-tag-test', 
            groups=[
                {
                    "group": "group_f",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400001",
                            "attribute": 3,
                            "type": 3,
                            "decimal": 0.01
                        },
                        {
                            "name": "tag2",
                            "address": "1!400002",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                },
                {
                    "group": "group_s",
                    "interval": 3000,
                    "tags": [
                        {
                            "name": "tag1",
                            "address": "1!400001",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        },
                        {
                            "name": "tag1",
                            "address": "1!400002",
                            "attribute": 3,
                            "type": 9,
                            "precision": 3
                        }
                    ]
                }
            ]
        )

        assert 409 == response.status_code
        assert NEU_ERR_TAG_NAME_CONFLICT == response.json()['error']

    @description(given="tag name length exceeds 128", when="adding", then="add failed")
    def test_adding_tag_name_exceeds_128(self):
        response = api.add_gtags(
            node='modbus-tcp-tag-test', 
            groups=[{"group": "group","interval": 3000,
                     "tags": [{
                            "name": "Lt7Pa5Q0tKJN2gL3WiYJLnaFzqBq5q5xyJ1vYHxyf2j0IZR7y3RBYxXGQBHwV7dEzD7lIcF1yX9aR3vH8leYxXxHUILk2eVqQBlaQLaROZt9yZ9xQNxKkfy5mySHb79",
                            "address": "1!400001",
                            "attribute": 3,
                            "type": 3,
                            "decimal": 0.01}]}])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.add_gtags(
            node='modbus-tcp-tag-test', 
            groups=[{"group": "group","interval": 3000,
                     "tags": [{
                            "name": "Lt7Pa5Q0tKJN2gL3WiYJLnaFzqBq5q5xyJ1vYHxyf2j0IZR7y3RBYxXGQBHwV7dEzD7lIcF1yX9aR3vH8leYxXxHUILk2eVqQBlaQLaROZt9yZ9xQNxKkfy5mySHb79Y8",
                            "address": "1!400001",
                            "attribute": 3,
                            "type": 3,
                            "decimal": 0.01}]}])
        assert 400 == response.status_code
        assert NEU_ERR_TAG_NAME_TOO_LONG == response.json()['error']

    @description(given="non-existent node", when="add tags under non-existent node", then="add failed")
    def test_add_tags_non_existent_node(self):
        api.add_node(node='modbus-tcp', plugin=PLUGIN_MODBUS_TCP)
        api.add_group(node="modbus-tcp", group='group')

        response = api.add_tags(node="non-existent", group="group1", tags=hold_int16)
        assert 404 == response.status_code
        assert NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="non-existent group", when="add tags under non-existent group", then="add success")
    def test_add_tags_non_existent_group(self):
        response = api.add_tags(node="modbus-tcp", group="group1", tags=hold_int16)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="node group", when="add different types of tags", then="add success")
    def test_add_different_types_tags(self):
        response = api.add_tags(node="modbus-tcp", group="group1", tags=hold_int16_bit)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="node group", when="add a list of tags with errors", then="add failed")
    def test_add_error_tags(self):
        response = api.add_tags(node="modbus-tcp", group="group1", tags=hold_int16_bit)
        assert 409 == response.status_code
        assert NEU_ERR_TAG_NAME_CONFLICT == response.json()['error']

    @description(given="node group tags", when="get tags", then="get correct tags")
    def test_get_tags(self):
        response = api.get_tags(node="modbus-tcp", group="group1")
        assert 200 == response.status_code
        assert "hold_int16" == response.json()["tags"][0]["name"]
        assert "1!400001" == response.json()["tags"][0]["address"]
        assert NEU_TAG_ATTRIBUTE_RW == response.json()["tags"][0]["attribute"]
        assert NEU_TYPE_INT16 == response.json()["tags"][0]["type"]

    @description(given="node group tag", when="update tag", then="update success")
    def test_update_tag(self):
        response = api.update_tags(node="modbus-tcp", group="group1", tags=hold_int16_to_int32)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_tags(node="modbus-tcp", group="group1")
        assert 200 == response.status_code
        assert "hold_int16" == response.json()["tags"][0]["name"]
        assert "1!400002" == response.json()["tags"][0]["address"]
        assert NEU_TAG_ATTRIBUTE_RW == response.json()["tags"][0]["attribute"]
        assert NEU_TYPE_INT32 == response.json()["tags"][0]["type"]

    @description(given="non-existent group", when="delete tag from non-existent group", then="delete failed")
    def test_delete_tag_non_existent_group(self):
        response = api.del_tags(node="modbus-tcp", group="non-existent", tags=["hold_int16"])
        assert 404 == response.status_code
        assert NEU_ERR_GROUP_NOT_EXIST == response.json()['error']

    @description(given="node group tags", when="delete tags", then="delete success")
    def test_delete_tags(self):
        response = api.del_tags(node="modbus-tcp", group="group1", tags=["hold_int16", "hold_int16_static", "hold_int16_type", "hold_bit_type"])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_tags(node="modbus-tcp", group="group1")
        assert 200 == response.status_code
        assert [] == response.json()["tags"]

    @description(given="node group", when="add a tag with description", then="add success")
    def test_add_tag_with_description(self):
        response = api.add_tags(node="modbus-tcp", group="group1", tags=hold_int16_description)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_tags(node="modbus-tcp", group="group1")
        assert 200 == response.status_code
        assert "hold_int16_description" == response.json()["tags"][0]["name"]
        assert "1!400001" == response.json()["tags"][0]["address"]
        assert NEU_TAG_ATTRIBUTE_RW == response.json()["tags"][0]["attribute"]
        assert NEU_TYPE_INT16 == response.json()["tags"][0]["type"]
        assert "description info" == response.json()["tags"][0]["description"]

        response = api.del_tags(node="modbus-tcp", group="group1", tags=["hold_int16_description"])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(given="non-existent tag", when="update non-existent tag", then="update failed")
    def test_update_non_existent_tag(self):
        response = api.update_tags(node="modbus-tcp", group="group", tags=none)
        assert 404 == response.status_code
        assert NEU_ERR_TAG_NOT_EXIST == response.json()['error']

    @description(given="node group", when="add a tag with decimal", then="add success")
    def test_add_tag_with_decimal(self):
        response = api.add_tags(node="modbus-tcp", group="group1", tags=hold_int16_decimal)
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_tags(node="modbus-tcp", group="group1")
        assert 200 == response.status_code
        assert "hold_int16_decimal" == response.json()["tags"][0]["name"]
        assert "1!400001" == response.json()["tags"][0]["address"]
        assert NEU_TAG_ATTRIBUTE_RW == response.json()["tags"][0]["attribute"]
        assert NEU_TYPE_INT16 == response.json()["tags"][0]["type"]
        assert 0.1 == response.json()["tags"][0]["decimal"]

        response = api.del_tags(node="modbus-tcp", group="group1", tags=["hold_int16_decimal"])
        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

    @description(
        given="node group", when="add a tag with bias", then="add success"
    )
    def test_add_tag_with_bias(self):
        try:
            response = api.add_tags(
                node="modbus-tcp", group="group1", tags=hold_int16_bias
            )
            assert 200 == response.status_code
            assert NEU_ERR_SUCCESS == response.json()["error"]

            response = api.get_tags(node="modbus-tcp", group="group1")
            assert 200 == response.status_code
            assert "hold_int16_bias" == response.json()["tags"][0]["name"]
            assert "1!400001" == response.json()["tags"][0]["address"]
            assert (
                NEU_TAG_ATTRIBUTE_READ
                == response.json()["tags"][0]["attribute"]
            )
            assert NEU_TYPE_INT16 == response.json()["tags"][0]["type"]
            assert 1.0 == response.json()["tags"][0]["bias"]
        finally:
            api.del_tags(
                node="modbus-tcp", group="group1", tags=["hold_int16_bias"]
            )

    @description(
        given="node group",
        when="add a tag with bias out of range",
        then="add fail",
    )
    def test_add_tag_with_bias_out_of_range(self):
        tag = {
            **hold_int16_bias[0],
            "bias": 10000,
        }
        try:
            response = api.add_tags(
                node="modbus-tcp", group="group1", tags=[tag]
            )
            assert 400 == response.status_code
            assert (
                NEU_ERR_TAG_BIAS_INVALID == response.json()["error"]
            )
        finally:
            api.del_tags(node="modbus-tcp", group="group1", tags=[tag["name"]])

    @description(
        given="node group",
        when="add a tag with bias and write attr",
        then="add fail",
    )
    def test_add_tag_with_bias_and_write_attr(self):
        tag = {
            **hold_int16_bias[0],
            "attribute": NEU_TAG_ATTRIBUTE_RW,
        }
        try:
            response = api.add_tags(
                node="modbus-tcp", group="group1", tags=[tag]
            )
            assert 400 == response.status_code
            assert (
                NEU_ERR_TAG_BIAS_INVALID == response.json()["error"]
            )
        finally:
            api.del_tags(node="modbus-tcp", group="group1", tags=[tag["name"]])

    @description(
        given="node group",
        when="add a tag with bias and non-numeral type",
        then="add fail",
    )
    def test_add_tag_with_bias_and_bad_type(self):
        for typ in (
            NEU_TYPE_BIT,
            NEU_TYPE_BOOL,
            NEU_TYPE_BYTES,
            NEU_TYPE_WORD,
            NEU_TYPE_DWORD,
            NEU_TYPE_LWORD,
            NEU_TYPE_STRING,
            NEU_TYPE_TIME,
            NEU_TYPE_DATA_AND_TIME,
        ):
            tag = {
                **hold_int16_bias[0],
                "type": typ,
            }
            try:
                response = api.add_tags(
                    node="modbus-tcp", group="group1", tags=[tag]
                )
                assert 400 == response.status_code
                assert NEU_ERR_TAG_BIAS_INVALID == response.json()["error"]
            finally:
                api.del_tags(
                    node="modbus-tcp", group="group1", tags=[tag["name"]]
                )

    @description(
        given="node group tag with bias",
        when="update tag",
        then="update success",
    )
    def test_update_tag_with_bias(self):
        api.add_tags_check(
            node="modbus-tcp", group="group1", tags=hold_int16_bias
        )

        tag = {
            **hold_int16_bias[0],
            "bias": 100,
        }
        try:
            response = api.update_tags(
                node="modbus-tcp", group="group1", tags=[tag]
            )
            assert 200 == response.status_code
            assert NEU_ERR_SUCCESS == response.json()["error"]

            response = api.get_tags(node="modbus-tcp", group="group1")
            assert 200 == response.status_code
            assert tag["name"] == response.json()["tags"][0]["name"]
            assert tag["address"] == response.json()["tags"][0]["address"]
            assert tag["attribute"] == response.json()["tags"][0]["attribute"]
            assert tag["type"] == response.json()["tags"][0]["type"]
            assert tag["bias"] == response.json()["tags"][0]["bias"]
        finally:
            api.del_tags(node="modbus-tcp", group="group1", tags=[tag["name"]])

    @description(
        given="node group tag with bias",
        when="update tag bias out of range",
        then="update fail",
    )
    def test_update_tag_with_bias_out_of_range(self):
        api.add_tags_check(
            node="modbus-tcp", group="group1", tags=hold_int16_bias
        )

        tag = {
            **hold_int16_bias[0],
            "bias": -1001,
        }
        try:
            response = api.update_tags(
                node="modbus-tcp", group="group1", tags=[tag]
            )
            assert 400 == response.status_code
            assert (
                NEU_ERR_TAG_BIAS_INVALID == response.json()["error"]
            )

        finally:
            api.del_tags(node="modbus-tcp", group="group1", tags=[tag["name"]])

    @description(
        given="node group tag with bias",
        when="update tag with write attribute",
        then="update fail",
    )
    def test_update_tag_with_bias_and_write_attr(self):
        api.add_tags_check(
            node="modbus-tcp", group="group1", tags=hold_int16_bias
        )

        tag = {
            **hold_int16_bias[0],
            "attribute": NEU_TAG_ATTRIBUTE_RW,
        }
        try:
            response = api.update_tags(
                node="modbus-tcp", group="group1", tags=[tag]
            )
            assert 400 == response.status_code
            assert (
                NEU_ERR_TAG_BIAS_INVALID == response.json()["error"]
            )

        finally:
            api.del_tags(node="modbus-tcp", group="group1", tags=[tag["name"]])

    @description(
        given="node group tag with bias",
        when="update tag with bad type",
        then="update fail",
    )
    def test_update_tag_with_bias_and_bad_type(self):
        api.add_tags_check(
            node="modbus-tcp", group="group1", tags=hold_int16_bias
        )

        for typ in (
            NEU_TYPE_BIT,
            NEU_TYPE_BOOL,
            NEU_TYPE_BYTES,
            NEU_TYPE_WORD,
            NEU_TYPE_DWORD,
            NEU_TYPE_LWORD,
            NEU_TYPE_STRING,
            NEU_TYPE_TIME,
            NEU_TYPE_DATA_AND_TIME,
        ):
            tag = {
                **hold_int16_bias[0],
                "type": typ,
            }
            try:
                response = api.update_tags(
                    node="modbus-tcp", group="group1", tags=[tag]
                )
                assert 400 == response.status_code
                assert NEU_ERR_TAG_BIAS_INVALID == response.json()["error"]

            finally:
                api.del_tags(
                    node="modbus-tcp", group="group1", tags=[tag["name"]]
                )
