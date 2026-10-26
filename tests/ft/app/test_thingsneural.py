import json
import time

import pytest

import neuron.api as api
import neuron.config as config
from neuron.common import description, process, random_port
from neuron.mqtt import Mock

APP = "tn-device"
DRIVER_A = "modbus-a"
DRIVER_B = "modbus-b"
GROUP_A = "ga"
GROUP_B = "gb"
INTERVAL = 100
PRODUCT_KEY = "pkdemo"
DEVICE_KEY = "dkdemo"
REPORT_TOPIC = f"/iot/thing/{PRODUCT_KEY}/{DEVICE_KEY}/property/report/request"
SET_REQ_TOPIC = f"/iot/thing/{PRODUCT_KEY}/{DEVICE_KEY}/property/set/request"
SET_RESP_TOPIC = f"/iot/thing/{PRODUCT_KEY}/{DEVICE_KEY}/property/set/response"
def tag(address):
    return {
        "name": "tag0",
        "address": address,
        "attribute": config.NEU_TAG_ATTRIBUTE_RW,
        "type": config.NEU_TYPE_INT16,
    }
POINTS = json.dumps(
    [
        {
            "property": "temperature",
            "node": DRIVER_A,
            "group": GROUP_A,
            "tag": "tag0",
        },
        {
            "property": "humidity",
            "node": DRIVER_B,
            "group": GROUP_B,
            "tag": "tag0",
        },
    ]
)


@pytest.fixture(autouse=True, scope="class")
def neuron_process():
    process.remove_persistence()
    neuron = process.NeuronProcess()
    neuron.start()
    deadline = time.time() + 10
    while time.time() < deadline:
        try:
            if api.ping().status_code == 200:
                break
        except Exception:
            time.sleep(0.2)
    else:
        neuron.stop()
        raise AssertionError("neuron did not listen on port 7000")
    yield neuron
    neuron.stop()


@pytest.fixture(autouse=True, scope="class")
def thingsneural_node(neuron_process):
    api.add_node_check(APP, "ThingsNeural")
    yield
    api.del_node(APP)


@pytest.fixture(autouse=True, scope="class")
def modbus_nodes(thingsneural_node):
    port = random_port()
    simulator = process.start_simulator(
        ["./modbus_simulator", "tcp", f"{port}", "ip_v4"]
    )
    try:
        for node, group, address in (
            (DRIVER_A, GROUP_A, "1!400001"),
            (DRIVER_B, GROUP_B, "1!400002"),
        ):
            api.add_node_check(node, "Modbus TCP")
            api.add_group_check(node, group, INTERVAL)
            api.add_tags_check(node, group, tags=[tag(address)])
            api.subscribe_group_check(APP, node, group, params={})
            response = api.modbus_tcp_node_setting(node, port)
            assert 200 == response.status_code
        yield
    finally:
        api.del_node(DRIVER_A)
        api.del_node(DRIVER_B)
        process.stop_simulator(simulator)


@pytest.fixture(autouse=True, scope="class")
def mocker():
    mock = Mock()
    yield mock
    mock.done()


def setting(mocker):
    return {
        "client-id": mocker.rand_id(),
        "qos": 0,
        "host": "127.0.0.1",
        "port": mocker.port,
        "ssl": False,
        "product-key": PRODUCT_KEY,
        "device-key": DEVICE_KEY,
        "points": POINTS,
    }


def collect_reports(mocker):
    messages = mocker.get(REPORT_TOPIC, num=6, timeout=8)
    assert messages, "no NLINK property reports"
    if isinstance(messages, dict):
        messages = [messages]
    return messages


class TestThingsNeural:
    @description(
        given="ThingsNeural plugin is loaded",
        when="list plugins",
        then="ThingsNeural is visible",
    )
    def test_plugin_listed(self):
        response = api.get_plugins()
        assert 200 == response.status_code
        names = [item["name"] for item in response.json().get("plugins", [])]
        assert "ThingsNeural" in names

    @description(
        given="two south groups mapped to thing-model properties",
        when="tags are reported",
        then="payload is NLINK and keys are property identifiers",
    )
    def test_uplink_nlink(self, mocker):
        api.node_setting_check(APP, setting(mocker))
        api.node_ctl(APP, config.NEU_CTL_START)
        mocker.assert_client_connected()

        properties = set()
        for message in collect_reports(mocker):
            assert "node" not in message
            assert "group" not in message
            assert message["version"] == "1.0"
            assert message["method"] == "nlink.thing.property.report"
            assert message["ack"] == 1
            values = message["params"]["values"]
            assert "tag0" not in values
            properties.update(values.keys())

        assert "temperature" in properties
        assert "humidity" in properties

    @description(
        given="a property set without node or group",
        when="temperature and an unknown property are written",
        then="only the mapped south tag changes",
    )
    def test_downlink_reverse_map(self, mocker):
        api.node_setting_check(APP, setting(mocker))
        api.node_ctl(APP, config.NEU_CTL_START)
        mocker.assert_client_connected()

        response = mocker.reqresp(
            SET_RESP_TOPIC,
            SET_REQ_TOPIC,
            {
                "id": "set-1",
                "version": "1.0",
                "method": "nlink.thing.property.set",
                "params": {"values": {"temperature": 1234, "humidity": 56}},
            },
            timeout=3,
        )
        assert response is not None
        assert response["id"] == "set-1"
        assert response["code"] == 200
        assert response["method"] == "nlink.thing.property.set"

        time.sleep(0.5)
        assert 1234 == api.read_tag(DRIVER_A, GROUP_A, "tag0")
        assert 56 == api.read_tag(DRIVER_B, GROUP_B, "tag0")

        unknown = mocker.reqresp(
            SET_RESP_TOPIC,
            SET_REQ_TOPIC,
            {
                "id": "set-unknown",
                "version": "1.0",
                "method": "nlink.thing.property.set",
                "params": {"values": {"pressure": 1}},
            },
            timeout=3,
        )
        assert unknown is not None
        assert unknown["code"] == 400
        assert 1234 == api.read_tag(DRIVER_A, GROUP_A, "tag0")
