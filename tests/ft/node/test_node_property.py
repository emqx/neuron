import math

import pytest
import requests

import neuron.api as api
import neuron.config as config
from neuron.common import class_setup_and_teardown, case_time
from neuron.error import (
    NEU_ERR_BODY_IS_WRONG,
    NEU_ERR_NEED_TOKEN,
    NEU_ERR_NODE_NOT_EXIST,
    NEU_ERR_NODE_PROPERTY_EXIST,
    NEU_ERR_NODE_PROPERTY_NOT_EXIST,
    NEU_ERR_PARAM_IS_WRONG,
)


VALUES = {
    "string": "hello",
    "empty-string": "",
    "true": True,
    "false": False,
    "zero": 0,
    "integer-min": -(2 ** 63),
    "integer-max": 2 ** 63 - 1,
    "large-integer": 2 ** 53 + 1,
    "float": 1.25,
    "float-zero": 0.0,
    "negative-zero": -0.0,
    "float-max": 1.7976931348623157e308,
    "float-min": 5e-324,
    "strings": ["", "text", "中文"],
    "booleans": [False, True],
    "integers": [-(2 ** 63), 0, 2 ** 63 - 1],
    "floats": [0.0, -1.5, 2.5],
    "numbers": [0, 1.5, -2, 0.0],
    "empty-array": [],
    "转义\"\\\n": "中文\"\\\n\r\t\b\f\u0000end",
    "nul-array": ["\u0000", "a\u0000b"],
}


def assert_error(response, status, code):
    assert response.status_code == status, response.text
    assert response.json() == {"error": code}


def assert_value(actual, expected):
    assert type(actual) is type(expected)
    assert actual == expected
    if isinstance(expected, list):
        for actual_item, expected_item in zip(actual, expected):
            assert_value(actual_item, expected_item)
    elif isinstance(expected, float) and expected == 0:
        assert math.copysign(1, actual) == math.copysign(1, expected)


def assert_properties(node, expected):
    response = api.get_node_properties(node)
    assert response.status_code == 200, response.text
    assert set(response.json()) == {"properties"}
    properties = response.json()["properties"]
    assert len(properties) == len(expected)
    actual = {}
    for item in properties:
        assert set(item) == {"key", "value"}
        assert item["key"] not in actual
        actual[item["key"]] = item["value"]
    assert actual.keys() == expected.keys()
    for key, value in expected.items():
        assert_value(actual[key], value)


@pytest.fixture(params=[config.PLUGIN_MODBUS_TCP, config.PLUGIN_MQTT],
                ids=["southbound", "northbound"])
def property_node(request, class_setup_and_teardown):
    node = "property-driver" if request.param == config.PLUGIN_MODBUS_TCP else "property-app"
    api.add_node_check(node, request.param)
    state = {"name": node, "plugin": request.param}
    try:
        yield state
    finally:
        api.del_node_check(state["name"])


class TestNodeProperty:
    def test_crud_and_value_types(self, property_node):
        node = property_node["name"]
        assert_properties(node, {})
        api.add_node_properties_check(
            node, [{"key": key, "value": value} for key, value in VALUES.items()])
        assert_properties(node, VALUES)

        for value in VALUES.values():
            api.update_node_property_check(node, "string", value)
            assert_properties(node, dict(VALUES, string=value))
        api.update_node_property_check(node, "string", VALUES["string"])

        api.delete_node_properties_check(node, ["false", "zero", "absent"])
        remaining = {key: value for key, value in VALUES.items()
                     if key not in ("false", "zero")}
        assert_properties(node, remaining)
        api.delete_node_properties_check(node, ["absent"])
        assert_properties(node, remaining)
        api.delete_node_properties_check(node, list(remaining))
        assert_properties(node, {})
        api.add_node_properties_check(node, [{"key": "zero", "value": 0}])
        assert_properties(node, {"zero": 0})

    @pytest.mark.parametrize("conflict_first", [False, True])
    def test_post_conflict_is_atomic(self, property_node, conflict_first):
        node = property_node["name"]
        api.add_node_properties_check(node, [{"key": "existing", "value": False}])
        properties = [{"key": "new", "value": 0},
                      {"key": "existing", "value": "replacement"}]
        if conflict_first:
            properties.reverse()
        assert_error(api.add_node_properties(node, properties),
                     409, NEU_ERR_NODE_PROPERTY_EXIST)
        assert_properties(node, {"existing": False})
        assert_error(api.update_node_property(node, "missing", 0),
                     404, NEU_ERR_NODE_PROPERTY_NOT_EXIST)
        assert_properties(node, {"existing": False})

    def test_missing_node(self, property_node):
        node = property_node["name"] + "-missing"
        responses = [
            api.get_node_properties(node),
            api.add_node_properties(node, [{"key": "key", "value": 0}]),
            api.update_node_property(node, "key", False),
            api.delete_node_properties(node, ["missing"]),
        ]
        for response in responses:
            assert_error(response, 404, NEU_ERR_NODE_NOT_EXIST)

    def test_rename_delete_and_recreate(self, property_node):
        old_name = property_node["name"]
        values = {"false": False, "zero": 0, "empty": [], "escaped": "a\u0000b"}
        api.add_node_properties_check(
            old_name, [{"key": key, "value": value} for key, value in values.items()])
        new_name = old_name + "-renamed"
        api.update_node_check(old_name, new_name)
        property_node["name"] = new_name
        assert_error(api.get_node_properties(old_name), 404, NEU_ERR_NODE_NOT_EXIST)
        assert_properties(new_name, values)
        api.update_node_property_check(new_name, "false", True)
        values["false"] = True
        api.delete_node_properties_check(new_name, ["zero"])
        del values["zero"]
        assert_properties(new_name, values)

        api.add_node_check(old_name, property_node["plugin"])
        try:
            assert_properties(old_name, {})
        finally:
            api.del_node_check(old_name)
        api.del_node_check(new_name)
        assert_error(api.get_node_properties(new_name), 404, NEU_ERR_NODE_NOT_EXIST)
        api.add_node_check(new_name, property_node["plugin"])
        assert_properties(new_name, {})
        api.add_node_properties_check(new_name, [{"key": "false", "value": False}])
        assert_properties(new_name, {"false": False})

    def test_restart_persistence(self, property_node, class_setup_and_teardown):
        node = property_node["name"]
        api.add_node_properties_check(
            node, [{"key": key, "value": value} for key, value in VALUES.items()])
        api.update_node_property_check(node, "string", False)
        api.delete_node_properties_check(node, ["zero", "absent"])
        new_name = node + "-persisted"
        api.update_node_check(node, new_name)
        property_node["name"] = new_name
        class_setup_and_teardown.restart()
        expected = dict(VALUES, string=False)
        del expected["zero"]
        assert_properties(new_name, expected)
        assert_error(api.get_node_properties(node), 404, NEU_ERR_NODE_NOT_EXIST)

        api.del_node_check(new_name)
        api.add_node_check(new_name, property_node["plugin"])
        class_setup_and_teardown.restart()
        assert_properties(new_name, {})

    @pytest.mark.parametrize("value", [None, {}, {"a": 1}, [[1]], [None], [{}],
                                      [1, "1"], [True, 1], [False, 0.0],
                                      ["true", True], ["", []]])
    def test_invalid_value_is_atomic(self, property_node, value):
        node = property_node["name"]
        api.add_node_properties_check(node, [{"key": "existing", "value": False}])
        assert_error(api.add_node_properties(node, [
            {"key": "new", "value": 0}, {"key": "bad", "value": value}]),
            400, NEU_ERR_BODY_IS_WRONG)
        assert_error(api.update_node_property(node, "existing", value),
                     400, NEU_ERR_BODY_IS_WRONG)
        assert_properties(node, {"existing": False})

    @pytest.mark.parametrize("method,fields", [
        ("POST", {}),
        ("POST", {"properties": []}),
        ("POST", {"properties": None}),
        ("POST", {"properties": {}}),
        ("POST", {"properties": [None]}),
        ("POST", {"properties": [{"value": 1}]}),
        ("POST", {"properties": [{"key": "", "value": 1}]}),
        ("POST", {"properties": [{"key": "bad\u0000key", "value": 1}]}),
        ("POST", {"properties": [{"key": 1, "value": 1}]}),
        ("POST", {"properties": [{"key": "bad"}]}),
        ("POST", {"properties": [{"key": "new", "value": 1},
                                  {"key": "new", "value": 2}]}),
        ("PUT", {}),
        ("PUT", {"key": "existing"}),
        ("PUT", {"value": False}),
        ("PUT", {"key": "", "value": False}),
        ("PUT", {"key": "bad\u0000key", "value": False}),
        ("PUT", {"key": 1, "value": False}),
        ("DELETE", {}),
        ("DELETE", {"keys": []}),
        ("DELETE", {"keys": None}),
        ("DELETE", {"keys": "existing"}),
        ("DELETE", {"keys": ["existing", "existing"]}),
        ("DELETE", {"keys": ["existing", ""]}),
        ("DELETE", {"keys": ["existing", "bad\u0000key"]}),
        ("DELETE", {"keys": ["existing", None]}),
        ("DELETE", {"keys": ["existing", 1]}),
    ])
    def test_invalid_request_is_atomic(self, property_node, method, fields):
        node = property_node["name"]
        api.add_node_properties_check(node, [{"key": "existing", "value": False}])
        body = dict(fields, node=node)
        if (method == "POST" and isinstance(body.get("properties"), list)
                and body["properties"]):
            body["properties"] = [{"key": "valid", "value": 0}] + body["properties"]
        response = requests.request(
            method, config.BASE_URL + "/api/v2/node/properties",
            headers={"Authorization": config.default_jwt}, json=body)
        assert_error(response, 400, NEU_ERR_BODY_IS_WRONG)
        assert_properties(node, {"existing": False})

    @pytest.mark.parametrize("node", [None, "", 1, "bad\u0000node"])
    def test_invalid_node(self, property_node, node):
        for response in [
            api.add_node_properties(node, [{"key": "key", "value": 0}]),
            api.update_node_property(node, "key", False),
            api.delete_node_properties(node, ["key"]),
        ]:
            assert_error(response, 400, NEU_ERR_BODY_IS_WRONG)
        assert_properties(property_node["name"], {})

    @pytest.mark.parametrize("params", [{}, {"node": ""}, {"node": "bad\u0000node"}])
    def test_invalid_query(self, property_node, params):
        response = requests.get(
            config.BASE_URL + "/api/v2/node/properties",
            headers={"Authorization": config.default_jwt}, params=params)
        assert_error(response, 400, NEU_ERR_PARAM_IS_WRONG)
        assert_properties(property_node["name"], {})

    @pytest.mark.parametrize("method", ["GET", "POST", "PUT", "DELETE"])
    def test_jwt_required(self, property_node, method):
        node = property_node["name"]
        body = {"node": node, "properties": [{"key": "key", "value": 0}],
                "key": "key", "value": False, "keys": ["key"]}
        response = requests.request(
            method, config.BASE_URL + "/api/v2/node/properties",
            params={"node": node} if method == "GET" else None,
            json=body if method != "GET" else None)
        assert_error(response, 401, NEU_ERR_NEED_TOKEN)
        assert_properties(node, {})
