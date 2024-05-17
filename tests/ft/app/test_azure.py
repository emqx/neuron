import json
import time

import pytest

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
from neuron.mqtt import Mock


NODE = "azure"
DRIVER = "modbus"
GROUP = "group"
INTERVAL = 100
TAG0 = {
    "name": "tag0",
    "address": "1!400001.",
    "attribute": config.NEU_TAG_ATTRIBUTE_RW,
    "type": config.NEU_TYPE_INT16,
}
TAGS = [
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
FORMAT_VALUES = 0
FORMAT_TAGS = 1
HUB = "127.0.0.1"
DEVICE_ID = "client-005"
USER = f"{HUB}/{DEVICE_ID}/?api-version=2021-04-12"
SAS = "neuron"
PASSWORD = SAS
D2C_TOPIC = f"devices/{DEVICE_ID}/messages/events/"
C2D_TOPIC = f"devices/{DEVICE_ID}/messages/devicebound/write"


@pytest.fixture(autouse=True, scope="class")
def azure_node(class_setup_and_teardown):
    api.add_node_check(NODE, "Azure IoT")
    yield
    api.del_node(NODE)


@pytest.fixture(autouse=True, scope="class")
def modbus_node(azure_node):
    try:
        api.add_node_check(DRIVER, "Modbus TCP")
        api.add_group_check(DRIVER, GROUP, INTERVAL)
        api.add_tags_check(DRIVER, GROUP, tags=[TAG0])
        api.subscribe_group_check(NODE, DRIVER, GROUP)
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
        process.stop_simulator(p)


@pytest.fixture(autouse=True, scope="class")
def mocker():
    mock = Mock(user=USER, password=PASSWORD)
    yield mock
    mock.done()


def reqresp(mocker, resp_topic, req_topic, payload, qos=0, timeout=1):
    try:
        mocker.client.subscribe(resp_topic)
        start = time.time_ns()
        # try request until response received
        while time.time_ns() - start <= timeout * 1_000_000_000:
            mocker.pub(req_topic, payload, qos=qos)
            msg = mocker.client.listen(resp_topic, timeout=0.1)
            if msg:
                msg = json.loads(msg)
                if "uuid" in msg:
                    return msg
    finally:
        mocker.client.unsubscribe(resp_topic)


@pytest.fixture(scope="function")
def conf_base(mocker):
    return {
        "device-id": DEVICE_ID,
        "qos": 0,
        "format": FORMAT_VALUES,
        "host": HUB,
        "auth": 0,
        "sas": SAS,
    }


@pytest.fixture(scope="function")
def conf_fmt_tags(conf_base):
    return {
        **conf_base,
        "format": FORMAT_TAGS,
    }


@pytest.fixture(scope="function")
def conf_ssl(conf_base, mocker):
    return {
        **conf_base,
        "auth": 1,
        "ca": mocker.ca_base64,
        "cert": mocker.client_cert_base64,
        "key": mocker.client_key_base64,
    }


@pytest.fixture(scope="function")
def conf_no_id(conf_base):
    del conf_base["device-id"]


@pytest.fixture(scope="function")
def conf_bad_id(conf_base):
    return {
        **conf_base,
        "device-id": "",
    }


@pytest.fixture(scope="function")
def conf_bad_qos(conf_base):
    return {
        **conf_base,
        "qos": 2,
    }


@pytest.fixture(scope="function")
def conf_bad_fmt(conf_base):
    return {
        **conf_base,
        "format": -1,
    }


@pytest.fixture(scope="function")
def conf_ssl_bad_ca(conf_ssl):
    return {
        **conf_ssl,
        "ca": "",
    }


@pytest.fixture(scope="function")
def conf_ssl_bad_cert(conf_ssl):
    return {
        **conf_ssl,
        "cert": "(@$&*)",
    }


@pytest.fixture(scope="function")
def conf_ssl_no_cert(conf_base, mocker):
    return {
        **conf_base,
        "port": mocker.ssl_port,
        "auth": 1,
        "ca": mocker.ca_base64,
    }


@pytest.fixture(scope="function")
def conf_ssl_no_key(conf_ssl, mocker):
    conf = {
        **conf_ssl,
    }
    del conf["key"]
    return conf


@pytest.fixture(scope="function")
def conf_ssl_bad_key(conf_ssl):
    return {
        **conf_ssl,
        "key": "",
    }


class TestAzure:
    @description(
        given="Azure node and {conf}",
        when="config node",
        then="should fail",
    )
    @pytest.mark.parametrize(
        "conf",
        [
            "conf_no_id",
            "conf_bad_id",
            "conf_bad_qos",
            "conf_bad_fmt",
            "conf_ssl_bad_ca",
            "conf_ssl_no_cert",
            "conf_ssl_bad_cert",
            "conf_ssl_no_key",
            "conf_ssl_bad_key",
        ],
    )
    def test_azure_conf(self, request, conf):
        conf = request.getfixturevalue(conf)
        response = api.node_setting(NODE, conf)
        assert 400 == response.status_code
        assert error.NEU_ERR_NODE_SETTING_INVALID == response.json()["error"]

    @description(
        given="Azure node and {conf}",
        when="after configuration",
        then="should connect to the broker",
    )
    @pytest.mark.parametrize(
        "conf",
        [
            "conf_base",
            "conf_ssl",
        ],
    )
    def test_azure_connect(self, request, mocker, conf):
        conf = request.getfixturevalue(conf)
        api.node_setting_check(NODE, conf)
        api.node_ctl(NODE, config.NEU_CTL_START)
        mocker.assert_client_connected()

    @description(
        given="Azure node",
        when="stop/start node",
        then="should disconnect/connect broker",
    )
    def test_azure_start_stop(self, request, mocker, conf_base):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl_check(NODE, config.NEU_CTL_STOP)
        mocker.assert_client_disconnected()
        api.node_ctl_check(NODE, config.NEU_CTL_START)
        mocker.assert_client_connected()

    @description(
        given="Azure node and {conf}",
        when="after configuration",
        then="broker should receive data on upload topic",
    )
    @pytest.mark.parametrize("conf", ["conf_base", "conf_fmt_tags"])
    def test_azure_upload(self, mocker, request, conf):
        fmt_tags = conf == "conf_fmt_tags"
        conf = request.getfixturevalue(conf)
        api.node_setting_check(NODE, conf)
        api.node_ctl(NODE, config.NEU_CTL_START)

        msg = mocker.get(D2C_TOPIC, timeout=3)
        assert msg is not None
        assert DRIVER == msg["node"]
        assert GROUP == msg["group"]
        if fmt_tags:
            assert TAG0["name"] in [tag["name"] for tag in msg["tags"]]
        else:
            assert (
                TAG0["name"] in msg["values"] or TAG0["name"] in msg["errors"]
            )

        try:
            api.add_tags_check(DRIVER, GROUP, TAGS)
            msg = mocker.get(D2C_TOPIC, timeout=3)
            assert msg is not None
            assert DRIVER == msg["node"]
            assert GROUP == msg["group"]
            if fmt_tags:
                len(TAGS) + 1 == len(msg["tags"])
            else:
                len(TAGS) + 1 == len(msg["values"]) + len(msg["errors"])
        finally:
            api.del_tags(DRIVER, GROUP, [tag["name"] for tag in TAGS])

    @description(
        given="Azure node",
        when="after renaming subscribed driver",
        then="broker should receive data on upload topic",
    )
    def test_azure_upload_when_driver_rename(self, mocker, conf_fmt_tags):
        api.node_setting_check(NODE, conf_fmt_tags)
        api.node_ctl(NODE, config.NEU_CTL_START)

        new_name = DRIVER + "-new"
        api.update_node_check(DRIVER, new_name)

        msg = mocker.get(D2C_TOPIC, timeout=3)
        try:
            assert msg is not None
            assert new_name == msg["node"]
            assert GROUP == msg["group"]
            assert TAG0["name"] in [tag["name"] for tag in msg["tags"]]
        finally:
            api.update_node_check(new_name, DRIVER)

    @description(
        given="Azure node",
        when="after renaming subscribed driver group",
        then="broker should receive data on upload topic",
    )
    def test_azure_upload_when_group_rename(self, mocker, conf_fmt_tags):
        api.node_setting_check(NODE, conf_fmt_tags)
        api.node_ctl(NODE, config.NEU_CTL_START)

        new_name = GROUP + "-new"
        api.update_group_check(DRIVER, GROUP, new_name)

        msg = mocker.get(D2C_TOPIC, timeout=3)
        try:
            assert msg is not None
            assert DRIVER == msg["node"]
            assert new_name == msg["group"]
            assert TAG0["name"] in [tag["name"] for tag in msg["tags"]]
        finally:
            api.update_group_check(DRIVER, new_name, GROUP)

    @description(
        given="Azure node",
        when="delete subscribed driver group and add back group",
        then="broker should receive data on upload topic",
    )
    def test_azure_upload_when_group_deleted(self, mocker, conf_fmt_tags):
        api.node_setting_check(NODE, conf_fmt_tags)
        api.node_ctl(NODE, config.NEU_CTL_START)

        # delete driver group
        api.del_group_check(DRIVER, GROUP)
        # add back driver group
        api.add_group_check(DRIVER, GROUP, INTERVAL)
        api.add_tags_check(DRIVER, GROUP, tags=[TAG0])
        # add back subscription on new topic
        api.subscribe_group_check(NODE, DRIVER, GROUP)

        msg = mocker.get(D2C_TOPIC, timeout=3)
        assert msg is not None
        assert DRIVER == msg["node"]
        assert GROUP == msg["group"]
        assert TAG0["name"] in [tag["name"] for tag in msg["tags"]]

    @description(
        given="Azure node",
        when="send bad write req",
        then="should fail",
    )
    @pytest.mark.parametrize(
        "req",
        [
            {
                "tag": TAG0["name"],
            },
            {
                "tags": [
                    {
                        "tag": TAG0["name"],
                    },
                ],
            },
        ],
    )
    def test_azure_write_bad_req(self, mocker, conf_base, req):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        mocker.assert_client_connected()

        req = {
            "uuid": "cd32be1b-c8b1-3257-94af-77f847b1ed3e",
            "node": DRIVER,
            "group": GROUP,
            **req,
        }
        mocker.pub(C2D_TOPIC, req)
        del req["uuid"]
        mocker.pub(C2D_TOPIC, req)

    @description(
        given="Azure node",
        when="write a driver tag",
        then="should success",
    )
    def test_azure_write_tag(self, mocker, conf_base):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        mocker.assert_client_connected()

        req = {
            "uuid": "cd32be1b-c8b1-3257-94af-77f847b1ed3e",
            "node": DRIVER,
            "group": GROUP,
            "tag": TAG0["name"],
            "value": 1234,
        }
        msg = reqresp(mocker, D2C_TOPIC, C2D_TOPIC, req, timeout=3)
        assert msg is not None
        assert req["uuid"] == msg["uuid"]
        assert error.NEU_ERR_SUCCESS == msg["error"]

        time.sleep(INTERVAL * 5 / 1000)
        assert 1234 == api.read_tag(DRIVER, GROUP, TAG0["name"])

    @description(
        given="Azure node",
        when="write driver many tags",
        then="should success",
    )
    def test_azure_write_tags(self, mocker, conf_base):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        mocker.assert_client_connected()

        req = {
            "uuid": "cd32be1b-c8b1-3257-94af-77f847b1ed3e",
            "node": DRIVER,
            "group": GROUP,
            "tags": [
                {
                    "tag": TAG0["name"],
                    "value": 1234,
                },
                {
                    "tag": "tag1",
                    "value": 1,
                },
                {
                    "tag": "tag2",
                    "value": 3.14,
                },
                {
                    "tag": "tag3",
                    "value": "hello",
                },
                {
                    "tag": "tag4",
                    "value": [1, 2, 3, 4],
                },
            ],
        }

        try:
            api.add_tags_check(DRIVER, GROUP, TAGS)
            msg = reqresp(mocker, D2C_TOPIC, C2D_TOPIC, req, timeout=3)
            assert msg is not None
            assert req["uuid"] == msg["uuid"]
            assert error.NEU_ERR_SUCCESS == msg["error"]

            time.sleep(INTERVAL * 5 / 1000)
            resp = api.read_tags(DRIVER, GROUP)
            assert 200 == resp.status_code
            resp_tags = sorted(resp.json()["tags"], key=lambda x: x["name"])
            print(resp_tags)
            req_tags = req["tags"]
            assert req_tags[0]["value"] == resp_tags[0]["value"]
            assert req_tags[1]["value"] == resp_tags[1]["value"]
            assert compare_float(req_tags[2]["value"], resp_tags[2]["value"])
            assert req_tags[3]["value"] == resp_tags[3]["value"]
            assert req_tags[4]["value"] == resp_tags[4]["value"]
        finally:
            api.del_tags(DRIVER, GROUP, [tag["name"] for tag in TAGS])
