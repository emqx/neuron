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

        assert 200 == response.status_code
        assert NEU_ERR_SUCCESS == response.json()['error']

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

        assert 200 == response.status_code
        assert NEU_ERR_TAG_TYPE_NOT_SUPPORT == response.json()['error']

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

        assert 200 == response.status_code
        assert NEU_ERR_TAG_NAME_CONFLICT == response.json()['error']
