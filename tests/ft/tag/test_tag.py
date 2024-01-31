import neuron.api as api
from neuron.config import *
from neuron.common import *
from neuron.error import *


class TestLog:

    @description(given="multiple non-existing groups and multiple correct tags", 
                 when="adding", 
                 then="successfully added")
    def test_adding_correct_gtags_nonexisting_groups(self):
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
                }
            ]
        )

        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

        response = api.get_group()
        assert 200       == response.status_code
        assert 'group_1' == response.json()['groups'][0]['group']
        assert 3000      == response.json()['groups'][0]['interval']
        assert 2         == response.json()['groups'][0]['tag_count']
        assert 'group_2' == response.json()['groups'][1]['group']
        assert 300       == response.json()['groups'][1]['interval']
        assert 2         == response.json()['groups'][1]['tag_count']

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
                    "group": "group_3",
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
        assert '1!400007' == response.json()['tags'][0]['address']
        assert 3          == response.json()['tags'][1]['precision']
        assert 9          == response.json()['tags'][1]['type']
        assert 3          == response.json()['tags'][1]['attribute']

        response = api.get_tags(node='modbus-tcp-tag-test', group='group_3')
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