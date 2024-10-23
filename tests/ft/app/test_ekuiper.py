import json
import time
import subprocess

import pytest
import pynng

import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import (
    case_time,
    class_setup_and_teardown,
    compare_float,
    description,
    process,
    random_port,
)

otel_port = random_port()


NODE = "ekuiper"
PLUGIN = "eKuiper"
DRIVER = "modbus"
DRIVER_PORT = random_port()
GROUP = "group"
INTERVAL = 100
TAGS = [
    {
        "name": "tag0",
        "address": "1!400001.",
        "attribute": config.NEU_TAG_ATTRIBUTE_RW,
        "type": config.NEU_TYPE_INT16,
    },
    {
        "name": "tag1",
        "address": "1!000001",
        "attribute": config.NEU_TAG_ATTRIBUTE_RW,
        "type": config.NEU_TYPE_BIT,
    },
    {
        "name": "tag2",
        "address": "1!400017",
        "attribute": config.NEU_TAG_ATTRIBUTE_RW,
        "type": config.NEU_TYPE_FLOAT,
    },
    {
        "name": "tag3",
        "address": "1!400020.10",
        "attribute": config.NEU_TAG_ATTRIBUTE_RW,
        "type": config.NEU_TYPE_STRING,
    },
    {
        "name": "tag4",
        "address": "1!400040.4",
        "attribute": config.NEU_TAG_ATTRIBUTE_RW,
        "type": config.NEU_TYPE_BYTES,
    },
]


@pytest.fixture(autouse=True, scope="class")
def ekuiper_node():
    otel_p = subprocess.Popen(
        ["python3", "otel_server.py", f'{otel_port}'], stderr=subprocess.PIPE, cwd="simulator")
    api.add_node_check(NODE, PLUGIN)
    api.otel_start("127.0.0.1", otel_port)
    yield NODE
    api.del_node(NODE)
    api.otel_stop()
    otel_p.terminate()
    otel_p.wait()


@pytest.fixture(autouse=True, scope="class")
def modbus_node(ekuiper_node):
    try:
        api.add_node_check(DRIVER, "Modbus TCP")
        api.add_group_check(DRIVER, GROUP, INTERVAL)
        api.add_tags_check(DRIVER, GROUP, tags=TAGS)
        api.subscribe_group_check(ekuiper_node, DRIVER, GROUP)
        p = process.start_simulator(
            ["./modbus_simulator", "tcp", f"{DRIVER_PORT}", "ip_v4"]
        )
        response = api.modbus_tcp_node_setting(DRIVER, DRIVER_PORT)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()["error"]
        yield
    finally:
        # api.del_node(DRIVER)
        process.stop_simulator(p)


@pytest.fixture(scope="function")
def conf_base():
    return {
        "host": "0.0.0.0",
        "port": 7081,
    }


@pytest.fixture(scope="function")
def conf_empty_host(conf_base):
    return {
        **conf_base,
        "host": "",
    }


@pytest.fixture(scope="function")
def conf_bad_host(conf_base):
    return {
        **conf_base,
        "host": "8.8.8.8",
    }


@pytest.fixture(scope="function")
def conf_bad_port(conf_base):
    return {
        **conf_base,
        "port": 1_000_000,
    }


@pytest.fixture(scope="function")
def mocker():
    mock = Mocker(timeout=1000)
    yield mock
    mock.disconnect()


class Mocker:
    def __init__(self, timeout=1000):
        self.timeout = timeout
        self.sock_ = None

    @property
    def sock(self):
        if self.sock_:
            return self.sock_
        self.sock_ = pynng.Pair0(
            recv_timeout=self.timeout, send_timeout=self.timeout
        )
        return self.sock_

    @sock.setter
    def sock(self, value):
        if self.sock_:
            self.sock_.close()
        self.sock_ = value

    def connect(self, host, port):
        self.sock.dial(f"tcp://{host}:{port}", block=True)

    def disconnect(self):
        self.sock = None

    def recv(self):
        msg = self.sock.recv()
        msg = msg[26:]
        return json.loads(msg.decode())

    def send(self, data):
        msg = json.dumps(data).encode()
        trace = b'\x0A\xCE\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x21\x22\x23\x24\x25\x26\x27\x28'
        self.sock.send(trace + msg)


class TesteKuiper:
    @description(
        given="eKuiper node and {conf}",
        when="config node",
        then="should fail",
    )
    @pytest.mark.parametrize(
        "conf",
        [
            "conf_empty_host",
            "conf_bad_host",
            "conf_bad_port",
        ],
    )
    def test_ekuiper_conf_bad(self, request, ekuiper_node, conf):
        conf = request.getfixturevalue(conf)
        response = api.node_setting(ekuiper_node, conf)
        assert 400 == response.status_code
        assert error.NEU_ERR_NODE_SETTING_INVALID == response.json()["error"]

    @description(
        given="eKuiper node",
        when="config node with already in use address",
        then="should fail",
    )
    def test_ekuiper_conf_address_in_use(
        self, mocker, ekuiper_node, conf_base
    ):
        api.node_setting_check(ekuiper_node, conf_base)

        conf_new = {**conf_base, "port": conf_base["port"] + 1}
        node = ekuiper_node + "-2"
        api.add_node_check(node, PLUGIN)
        api.node_setting_check(node, conf_new)

        response = api.node_setting(ekuiper_node, conf_new)
        assert 400 == response.status_code
        assert error.NEU_ERR_NODE_SETTING_INVALID == response.json()["error"]

        # should be able to connect with old conf
        mocker.connect(**conf_base)

    @description(
        given="eKuiper node",
        when="configured",
        then="should allow connection from eKuiper client",
    )
    def test_ekuiper_connect(self, mocker, ekuiper_node, conf_base):
        api.node_setting_check(ekuiper_node, conf_base)
        api.node_ctl(ekuiper_node, config.NEU_CTL_START)

        mocker.connect(**conf_base)

    @description(
        given="eKuiper node",
        when="stop/start node",
        then="should disconnect/connect broker",
    )
    def test_ekuiper_start_stop(self, mocker, ekuiper_node, conf_base):
        api.node_setting_check(ekuiper_node, conf_base)
        api.node_ctl_check(ekuiper_node, config.NEU_CTL_STOP)
        with pytest.raises(Exception):
            mocker.connect(**conf_base)
        api.node_ctl_check(ekuiper_node, config.NEU_CTL_START)
        mocker.connect(**conf_base)

    @description(
        given="eKuiper node",
        when="configured",
        then="should upload data to eKuiper client",
    )
    def test_ekuiper_upload(self, mocker, ekuiper_node, conf_base):
        api.node_setting_check(ekuiper_node, conf_base)
        api.node_ctl(ekuiper_node, config.NEU_CTL_START)
        mocker.connect(**conf_base)

        time.sleep(INTERVAL * 5 / 1000)
        msg = mocker.recv()
        assert msg is not None
        assert DRIVER == msg["node_name"]
        assert GROUP == msg["group_name"]
        assert "timestamp" in msg
        assert len(TAGS) == len(msg["values"]) + len(msg["errors"])
        for tag in TAGS:
            assert tag["name"] in msg["values"] or tag["name"] in msg["errors"]

    @description(
        given="eKuiper node",
        when="subscribed driver renamed",
        then="should upload data to eKuiper client",
    )
    def test_ekuiper_upload_when_driver_rename(
        self, mocker, ekuiper_node, conf_base
    ):
        api.node_setting_check(ekuiper_node, conf_base)
        api.node_ctl(ekuiper_node, config.NEU_CTL_START)

        new_name = DRIVER + "-new"
        api.update_node_check(DRIVER, new_name)

        try:
            mocker.connect(**conf_base)
            time.sleep(INTERVAL * 5 / 1000)
            msg = mocker.recv()
            assert msg is not None
            assert new_name == msg["node_name"]
            assert GROUP == msg["group_name"]
            assert "timestamp" in msg
            assert len(TAGS) == len(msg["values"]) + len(msg["errors"])
            for tag in TAGS:
                assert (
                    tag["name"] in msg["values"]
                    or tag["name"] in msg["errors"]
                )
        finally:
            api.update_node_check(new_name, DRIVER)

    @description(
        given="eKuiper node",
        when="subscribed driver group renamed",
        then="should upload data to eKuiper client",
    )
    def test_ekuiper_upload_when_group_rename(
        self, mocker, ekuiper_node, conf_base
    ):
        api.node_setting_check(ekuiper_node, conf_base)
        api.node_ctl(ekuiper_node, config.NEU_CTL_START)

        new_name = GROUP + "-new"
        api.update_group_check(DRIVER, GROUP, new_name)

        try:
            mocker.connect(**conf_base)
            time.sleep(INTERVAL * 5 / 1000)
            msg = mocker.recv()
            assert msg is not None
            assert DRIVER == msg["node_name"]
            assert new_name == msg["group_name"]
            assert "timestamp" in msg
            assert len(TAGS) == len(msg["values"]) + len(msg["errors"])
            for tag in TAGS:
                assert (
                    tag["name"] in msg["values"]
                    or tag["name"] in msg["errors"]
                )
        finally:
            api.update_group_check(DRIVER, new_name, GROUP)

    @description(
        given="eKuiper node",
        when="driver group unsubscribed",
        then="should upload no data",
    )
    def test_ekuiper_upload_when_unsubscribe(
        self, mocker, ekuiper_node, conf_base
    ):
        api.node_setting_check(ekuiper_node, conf_base)
        api.node_ctl(ekuiper_node, config.NEU_CTL_START)

        api.unsubscribe_group_check(ekuiper_node, DRIVER, GROUP)

        mocker.connect(**conf_base)
        with pytest.raises(Exception):
            mocker.recv()
        api.subscribe_group_check(ekuiper_node, DRIVER, GROUP)

    @description(
        given="eKuiper node",
        when="subscribed driver deleted",
        then="should upload no data",
    )
    def test_gewu_upload_when_driver_deleted(
        self, mocker, ekuiper_node, conf_base
    ):
        api.node_setting_check(ekuiper_node, conf_base)
        api.node_ctl(ekuiper_node, config.NEU_CTL_START)

        api.del_node(DRIVER)

        mocker.connect(**conf_base)
        with pytest.raises(Exception):
            mocker.recv()

        api.add_node_check(DRIVER, "Modbus TCP")
        api.add_group_check(DRIVER, GROUP, INTERVAL)
        api.add_tags_check(DRIVER, GROUP, tags=TAGS)
        api.subscribe_group_check(ekuiper_node, DRIVER, GROUP)
        api.modbus_tcp_node_setting(DRIVER, DRIVER_PORT)

    @description(
        given="eKuiper node",
        when="send bad write req",
        then="should fail",
    )
    def test_ekuiper_write_bad_req(self, mocker, ekuiper_node, conf_base):
        api.node_setting_check(ekuiper_node, conf_base)
        api.node_ctl(ekuiper_node, config.NEU_CTL_START)

        req = {
            "node_name": DRIVER,
            "group_name": GROUP,
        }
        mocker.connect(**conf_base)
        mocker.send(req)

    @description(
        given="eKuiper node",
        when="write a driver tag",
        then="should success",
    )
    def test_ekuiper_write_tag(self, mocker, ekuiper_node, conf_base):
        api.node_setting_check(ekuiper_node, conf_base)
        api.node_ctl(ekuiper_node, config.NEU_CTL_START)

        req = {
            "node_name": DRIVER,
            "group_name": GROUP,
        }
        req_tags = {
            TAGS[0]["name"]: 1234,
            TAGS[1]["name"]: 1,
            TAGS[2]["name"]: 3.14,
            TAGS[3]["name"]: "hello",
            TAGS[4]["name"]: [1, 2, 3, 4],
        }

        mocker.connect(**conf_base)
        for name, value in req_tags.items():
            mocker.send({**req, "tag_name": name, "value": value})

        time.sleep(INTERVAL * 5 / 1000)
        resp = api.read_tags(DRIVER, GROUP)
        assert 200 == resp.status_code
        for tag in resp.json()["tags"]:
            if isinstance(tag["value"], float):
                assert compare_float(req_tags[tag["name"]], tag["value"])
            else:
                assert req_tags[tag["name"]] == tag["value"]

    @description(
        given="eKuiper node",
        when="write multiple driver tags",
        then="should success",
    )
    def test_ekuiper_write_tags(self, mocker, ekuiper_node, conf_base):
        api.node_setting_check(ekuiper_node, conf_base)
        api.node_ctl(ekuiper_node, config.NEU_CTL_START)

        req_tags = {
            TAGS[0]["name"]: 5678,
            TAGS[1]["name"]: 0,
            TAGS[2]["name"]: 6.28,
            TAGS[3]["name"]: "mulitple",
            TAGS[4]["name"]: [9, 8, 7, 6],
        }
        req = {
            "node_name": DRIVER,
            "group_name": GROUP,
            "tags": [
                {
                    "tag_name": name,
                    "value": value,
                }
                for name, value in req_tags.items()
            ]
        }

        mocker.connect(**conf_base)
        mocker.send(req)

        time.sleep(INTERVAL * 5 / 1000)
        resp = api.read_tags(DRIVER, GROUP)
        assert 200 == resp.status_code
        for tag in resp.json()["tags"]:
            if isinstance(tag["value"], float):
                assert compare_float(req_tags[tag["name"]], tag["value"])
            else:
                assert req_tags[tag["name"]] == tag["value"]
