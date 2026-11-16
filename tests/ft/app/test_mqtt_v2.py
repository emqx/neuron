import json
import threading
import time

import paho.mqtt.client as paho
import pytest

import neuron.api as api
import neuron.config as config
import neuron.error as error
from neuron.common import (
    case_time,
    class_setup_and_teardown,
    description,
    process,
    random_port,
)
from neuron.mqtt import Mock

NODE = "mqtt-v2"
PLUGIN = "MQTT V2"
DRIVER = "modbus"
GROUP = "group"
GROUP2 = "group2"
INTERVAL = 100
TAG0 = {
    "name": "tag0",
    "address": "1!400001",
    "attribute": config.NEU_TAG_ATTRIBUTE_RW,
    "type": config.NEU_TYPE_INT16,
    "description": "temperature",
    "unit": "C",
}
TAG1 = {
    "name": "tag1",
    "address": "1!400002",
    "attribute": config.NEU_TAG_ATTRIBUTE_READ,
    "type": config.NEU_TYPE_UINT16,
}
TAG2 = {
    "name": "tag2",
    "address": "1!400010",
    "attribute": config.NEU_TAG_ATTRIBUTE_RW,
    "type": config.NEU_TYPE_INT16,
}

APP_STATE_TOPIC = f"neuron/{NODE}/state"
DEV_STATE_TOPIC = f"neuron/{NODE}/{DRIVER}/state"
DATA_TOPIC = f"neuron/{NODE}/{DRIVER}/data"
WRITE_REQ_TOPIC = f"neuron/{NODE}/{DRIVER}/write/request"
WRITE_RESP_TOPIC = f"neuron/{NODE}/{DRIVER}/write/response"


def new_client():
    if hasattr(paho, "CallbackAPIVersion"):
        return paho.Client(paho.CallbackAPIVersion.VERSION2)
    return paho.Client()


class Watcher:
    """Records the messages on a topic filter, retained ones included."""

    def __init__(self, port, topic):
        self.messages = []
        self.cond = threading.Condition()
        self.client = new_client()
        self.client.on_message = self._on_message
        self.client.connect("127.0.0.1", port)
        self.client.subscribe(topic, qos=1)
        self.client.loop_start()

    def _on_message(self, client, userdata, msg):
        with self.cond:
            self.messages.append(msg)
            self.cond.notify_all()

    def wait(self, pred=lambda msg: True, timeout=5):
        deadline = time.time() + timeout
        with self.cond:
            while True:
                for msg in self.messages:
                    if pred(msg):
                        return msg
                remaining = deadline - time.time()
                if remaining <= 0:
                    return None
                self.cond.wait(remaining)

    def publish(self, topic, payload):
        self.client.publish(topic, payload, qos=1).wait_for_publish(2)

    def close(self):
        self.client.loop_stop()
        self.client.disconnect()


def payload_of(msg):
    return json.loads(msg.payload) if msg is not None and msg.payload else None


def retained(port, topic, timeout=1.5):
    """Payload the broker retains on `topic`, None if there is none."""
    watcher = Watcher(port, topic)
    try:
        msg = watcher.wait(lambda m: m.retain, timeout=timeout)
        return payload_of(msg) if msg is not None else None
    finally:
        watcher.close()


def wait_retained(port, topic, pred, timeout=5):
    deadline = time.time() + timeout
    while time.time() < deadline:
        payload = retained(port, topic, timeout=0.5)
        if pred(payload):
            return payload
        time.sleep(0.2)
    return retained(port, topic)


def write(watcher, payload, pred=lambda p: True, retries=1):
    text = json.dumps(payload) if isinstance(payload, dict) else payload
    for _ in range(retries):
        watcher.publish(WRITE_REQ_TOPIC, text)
        msg = watcher.wait(
            lambda m: m.topic == WRITE_RESP_TOPIC and pred(payload_of(m)),
            timeout=5 if 1 == retries else 1)
        if msg is not None:
            return payload_of(msg)
    assert False, "no write response"


@pytest.fixture(autouse=True, scope="class")
def mocker():
    mock = Mock()
    yield mock
    mock.done()


@pytest.fixture(autouse=True, scope="class")
def mqtt_v2_node(mocker):
    api.add_node_check(NODE, PLUGIN)
    yield
    api.del_node(NODE)


@pytest.fixture(autouse=True, scope="class")
def modbus_node(mqtt_v2_node):
    p = None
    try:
        api.add_node_check(DRIVER, "Modbus TCP")
        api.add_group_check(DRIVER, GROUP, INTERVAL)
        api.add_group_check(DRIVER, GROUP2, INTERVAL)
        api.add_tags_check(DRIVER, GROUP, tags=[TAG0, TAG1])
        api.add_tags_check(DRIVER, GROUP2, tags=[TAG2])
        port = random_port()
        p = process.start_simulator(
            ["./modbus_simulator", "tcp", f"{port}", "ip_v4"]
        )
        response = api.modbus_tcp_node_setting(DRIVER, port)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()["error"]
        yield
    finally:
        api.del_node(DRIVER)
        if p is not None:
            process.stop_simulator(p)


@pytest.fixture(scope="function")
def conf_base(mocker):
    return {
        "client-id": mocker.rand_id(),
        "host": "127.0.0.1",
        "port": mocker.port,
        "version": 5,
    }


@pytest.fixture(scope="function")
def conf_user(conf_base, mocker):
    return {**conf_base, "username": mocker.user, "password": mocker.password}


@pytest.fixture(scope="function")
def conf_ssl1(conf_base, mocker):
    return {**conf_base, "port": mocker.ssl_port, "ssl": True,
            "ca": mocker.ca_base64}


@pytest.fixture(scope="function")
def conf_ssl2(conf_ssl1, mocker):
    return {**conf_ssl1, "cert": mocker.client_cert_base64,
            "key": mocker.client_key_base64}


@pytest.fixture(scope="function")
def subscribed(mocker):
    # an empty topic selects the default data topic, as an absent one
    api.subscribe_group_check(NODE, DRIVER, GROUP, {"topic": ""})
    yield
    api.unsubscribe_group(NODE, DRIVER, GROUP)
    api.unsubscribe_group(NODE, DRIVER, GROUP2)


@pytest.fixture(scope="function")
def watcher(mocker):
    w = Watcher(mocker.port, "#")
    yield w
    w.close()


class TestMqttV2:
    @description(
        given="MQTT V2 node and invalid {field}",
        when="config node",
        then="should fail",
    )
    @pytest.mark.parametrize(
        "field,value",
        [
            ("client-id", ""),
            ("host", ""),
            ("port", 0),
            ("qos", 3),
            ("version", 3),
            ("topic-prefix", "neuron/+"),
            ("topic-prefix", "neuron/${device}"),
            ("data-template", "{\"a\": \"${tag.value}\"}"),
            ("data-template", "{\"a\": \"${nope}\"}"),
            ("data-template", "{\"a\": \"${properties}\"}"),
            ("data-template", "{\"a\": "),
            ("tag-template", "{\"a\": \"${tags}\"}"),
            ("offline-cache", 1),
        ],
    )
    def test_conf_invalid(self, conf_base, field, value):
        response = api.node_setting(NODE, {**conf_base, field: value})
        assert 400 == response.status_code
        assert error.NEU_ERR_NODE_SETTING_INVALID == response.json()["error"]

    @description(
        given="MQTT V2 node and {conf}",
        when="after configuration",
        then="should connect to the broker and publish the online state",
    )
    @pytest.mark.parametrize("conf", ["conf_base", "conf_user", "conf_ssl1",
                                      "conf_ssl2"])
    def test_connect(self, request, mocker, conf):
        # the state left by the previous case
        old = retained(mocker.port, APP_STATE_TOPIC, timeout=0.5)
        old_ts = old["timestamp"] if old else 0
        api.node_setting_check(NODE, request.getfixturevalue(conf))
        api.node_ctl(NODE, config.NEU_CTL_START)

        deadline = time.time() + 5
        link = None
        while time.time() < deadline:
            link = api.get_nodes_state(NODE).json()["link"]
            if config.NEU_NODE_LINK_STATE_CONNECTED == link:
                break
            time.sleep(0.2)
        assert config.NEU_NODE_LINK_STATE_CONNECTED == link

        state = wait_retained(mocker.port, APP_STATE_TOPIC,
                              lambda p: p and p["online"] and
                              p["timestamp"] > old_ts)
        assert state["online"] is True and state["timestamp"] > old_ts

    @description(
        given="MQTT V2 node",
        when="start, stop and start the node",
        then="the retained online state follows, without will messages",
    )
    def test_online_state(self, mocker, conf_base, watcher):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        state = wait_retained(mocker.port, APP_STATE_TOPIC,
                              lambda p: p and p["online"])
        assert state["online"] is True
        assert "timestamp" in state

        api.node_ctl_check(NODE, config.NEU_CTL_STOP)
        state = wait_retained(mocker.port, APP_STATE_TOPIC,
                              lambda p: p and not p["online"])
        assert state["online"] is False
        assert "timestamp" in state

        api.node_ctl_check(NODE, config.NEU_CTL_START)
        state = wait_retained(mocker.port, APP_STATE_TOPIC,
                              lambda p: p and p["online"])
        assert state["online"] is True

        # a graceful disconnect discards the will
        time.sleep(0.5)
        assert not any(
            m.topic == APP_STATE_TOPIC and payload_of(m) == {"online": False}
            for m in watcher.messages
        )

    @description(
        given="MQTT V2 node subscribing a group",
        when="the driver runs",
        then="the retained device state carries the driver states",
    )
    def test_device_state(self, mocker, conf_base, subscribed):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        state = wait_retained(mocker.port, DEV_STATE_TOPIC,
                              lambda p: p and 1 == p["state"])
        assert {"timestamp", "name", "state", "running"} == set(state)
        assert DRIVER == state["name"]
        assert config.NEU_NODE_LINK_STATE_CONNECTED == state["state"]
        assert config.NEU_NODE_STATE_RUNNING == state["running"]

        try:
            api.node_ctl_check(DRIVER, config.NEU_CTL_STOP)
            state = wait_retained(mocker.port, DEV_STATE_TOPIC,
                                  lambda p: p and 4 == p["running"])
            assert config.NEU_NODE_STATE_STOPPED == state["running"]
        finally:
            api.node_ctl(DRIVER, config.NEU_CTL_START)

        # the state is kept until every group of the device is unsubscribed,
        # unknown subscription parameters are ignored
        api.subscribe_group_check(NODE, DRIVER, GROUP2,
                                  {"properties": {"vendor": "acme"}})
        api.unsubscribe_group_check(NODE, DRIVER, GROUP)
        time.sleep(0.5)
        state = retained(mocker.port, DEV_STATE_TOPIC)
        assert state is not None
        assert {"timestamp", "name", "state", "running"} == set(state)

        api.unsubscribe_group_check(NODE, DRIVER, GROUP2)
        assert wait_retained(mocker.port, DEV_STATE_TOPIC,
                             lambda p: p is None) is None

    @description(
        given="MQTT V2 node with the default templates",
        when="the subscribed group reports",
        then="the data topic receives the device data",
    )
    def test_data_default(self, mocker, conf_base, subscribed, watcher):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)

        # early reports precede the device connection and the tag metadata
        msg = watcher.wait(
            lambda m: m.topic == DATA_TOPIC and "temperature" in str(m.payload)
            and "value" in str(m.payload),
            timeout=8)
        assert msg is not None and not msg.retain
        data = payload_of(msg)
        assert DRIVER == data["device"]
        assert GROUP == data["group"]
        assert isinstance(data["timestamp"], int)
        tags = {tag["name"]: tag for tag in data["tags"]}
        assert {"tag0", "tag1"} == set(tags)
        assert "temperature" == tags["tag0"]["description"]
        assert "C" == tags["tag0"]["unit"]
        assert "error" not in tags["tag0"]
        assert "description" not in tags["tag1"]

    @description(
        given="MQTT V2 node with custom templates and a group topic",
        when="the subscribed groups report",
        then="the messages follow the templates and topics",
    )
    def test_data_template(self, mocker, conf_base, subscribed, watcher):
        conf = {
            **conf_base,
            "data-template": json.dumps({
                "id": "${app}/${device}/${group}",
                "topic": "${sub_topic}",
                "values": "${tags_kv}",
                "tag0": "${tags[tag0].value}",
            }),
            "tag-template": "${tag.value}",
        }
        api.node_setting_check(NODE, conf)
        api.node_ctl(NODE, config.NEU_CTL_START)
        api.subscribe_group_check(NODE, DRIVER, GROUP2,
                                  {"topic": "plant/${device}/${group}"})

        def has_values(topic, *tags):
            return lambda m: (m.topic == topic and set(tags) <= set(
                (payload_of(m) or {}).get("values", {})))

        msg = watcher.wait(has_values(DATA_TOPIC, "tag0", "tag1"), timeout=8)
        data = payload_of(msg)
        assert f"{NODE}/{DRIVER}/{GROUP}" == data["id"]
        assert DATA_TOPIC == data["topic"]
        assert {"tag0", "tag1"} == set(data["values"])
        assert data["tag0"] == data["values"]["tag0"]

        topic = f"plant/{DRIVER}/{GROUP2}"
        msg = watcher.wait(has_values(topic, "tag2"), timeout=8)
        data = payload_of(msg)
        assert topic == data["topic"]
        assert {"tag2"} == set(data["values"])
        assert "tag0" not in data

    @description(
        given="MQTT V2 node",
        when="subscribe with invalid parameters",
        then="the subscription should be rejected",
    )
    @pytest.mark.parametrize(
        "params",
        [
            {"topic": "$SYS/a"},
            {"topic": "a/+/b"},
            {"topic": "a/${nope}"},
            {"topic": 1},
        ],
    )
    def test_subscribe_invalid(self, params):
        response = api.subscribe_group(NODE, DRIVER, GROUP, params)
        assert 400 == response.status_code
        assert error.NEU_ERR_GROUP_PARAMETER_INVALID == response.json()["error"]

    @description(
        given="MQTT V2 node subscribing the driver",
        when="publish write requests",
        then="should write the tags and respond",
    )
    def test_write(self, mocker, conf_base, subscribed, watcher):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        api.subscribe_group_check(NODE, DRIVER, GROUP2, {})
        wait_retained(mocker.port, DEV_STATE_TOPIC, lambda p: p and 1 == p["state"])

        # the reconfigured node reconnects asynchronously
        resp = write(watcher, {"id": "1", "version": "1.0",
                               "params": {"values": {"tag0": 40}}},
                     lambda p: p.get("id") == "1", retries=10)
        assert {"id": "1", "version": "1.0", "error": 0, "tags": {"tag0": 0}} == resp
        assert 40 == api.read_tag(DRIVER, GROUP, TAG0["name"], sync=True)

        resp = write(watcher, {"id": 2, "params": {"values": {"tag0": 41, "tag2": 7}}},
                     lambda p: p.get("id") == 2)
        assert 0 == resp["error"]
        assert {"tag0": 0, "tag2": 0} == resp["tags"]
        assert 7 == api.read_tag(DRIVER, GROUP2, TAG2["name"], sync=True)

        resp = write(watcher, {"id": "3", "version": "neuron",
                               "params": {"group": GROUP, "values": {"tag0": 42}}},
                     lambda p: p.get("id") == "3")
        assert 0 == resp["error"]

        resp = write(watcher, {"id": "4", "params": {"values": {"tag1": 1}}},
                     lambda p: p.get("id") == "4")
        assert error.NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE == resp["error"]

        resp = write(watcher, {"id": "5", "params": {"values": {"nope": 1}}},
                     lambda p: p.get("id") == "5")
        assert error.NEU_ERR_TAG_NOT_EXIST == resp["error"]
        assert {"nope": error.NEU_ERR_TAG_NOT_EXIST} == resp["tags"]

        resp = write(watcher, {"id": "6", "version": "2.0",
                               "params": {"values": {"tag0": 1}}},
                     lambda p: p.get("id") == "6")
        assert error.NEU_ERR_PARAM_IS_WRONG == resp["error"]

        resp = write(watcher, {"id": "7", "params": {"group": "nope",
                                                      "values": {"tag0": 1}}},
                     lambda p: p.get("id") == "7")
        assert error.NEU_ERR_GROUP_NOT_SUBSCRIBE == resp["error"]

        resp = write(watcher, "not json", lambda p: "id" not in p)
        assert error.NEU_ERR_BODY_IS_WRONG == resp["error"]


class TestMqttV2Rename:
    @description(
        given="MQTT V2 node subscribing a driver",
        when="rename the driver and the app",
        then="topics follow the names and old retained states are cleared",
    )
    def test_rename(self, mocker, conf_base, watcher):
        new_driver = DRIVER + "-new"
        new_app = NODE + "-new"
        new_dev_state = f"neuron/{NODE}/{new_driver}/state"

        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        api.subscribe_group_check(NODE, DRIVER, GROUP, {})
        assert wait_retained(mocker.port, DEV_STATE_TOPIC, lambda p: p is not None)

        try:
            api.update_node_check(DRIVER, new_driver)
            state = wait_retained(mocker.port, new_dev_state,
                                  lambda p: p is not None)
            assert new_driver == state["name"]
            assert wait_retained(mocker.port, DEV_STATE_TOPIC,
                                 lambda p: p is None) is None
        finally:
            api.update_node(new_driver, DRIVER)
        assert wait_retained(mocker.port, DEV_STATE_TOPIC, lambda p: p is not None)

        try:
            api.update_node_check(NODE, new_app)
            state = wait_retained(mocker.port, f"neuron/{new_app}/state",
                                  lambda p: p and p["online"])
            assert state["online"] is True
            assert wait_retained(mocker.port, f"neuron/{new_app}/{DRIVER}/state",
                                 lambda p: p is not None)
            assert wait_retained(mocker.port, APP_STATE_TOPIC,
                                 lambda p: p is None) is None
            assert wait_retained(mocker.port, DEV_STATE_TOPIC,
                                 lambda p: p is None) is None

            data_topic = f"neuron/{new_app}/{DRIVER}/data"
            assert watcher.wait(lambda m: m.topic == data_topic, timeout=8)

            watcher.publish(f"neuron/{new_app}/{DRIVER}/write/request",
                            json.dumps({"id": "r", "params": {"values": {"tag0": 9}}}))
            resp = watcher.wait(
                lambda m: m.topic == f"neuron/{new_app}/{DRIVER}/write/response")
            assert resp is not None and 0 == payload_of(resp)["error"]
        finally:
            api.update_node(new_app, NODE)


class TestMqttV2Delete:
    @description(
        given="MQTT V2 node subscribing a driver",
        when="delete the node",
        then="its retained states should be cleared",
    )
    def test_delete_clears_retained(self, mocker, conf_base):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        api.subscribe_group_check(NODE, DRIVER, GROUP, {})
        assert wait_retained(mocker.port, DEV_STATE_TOPIC, lambda p: p is not None)
        assert wait_retained(mocker.port, APP_STATE_TOPIC, lambda p: p is not None)

        api.del_node(NODE)
        assert wait_retained(mocker.port, APP_STATE_TOPIC, lambda p: p is None) is None
        assert wait_retained(mocker.port, DEV_STATE_TOPIC, lambda p: p is None) is None
