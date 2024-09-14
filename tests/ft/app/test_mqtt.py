import time
import subprocess
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

otel_port = random_port()


NODE = "mqtt"
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
UPLOAD_TOPIC = f"/neuron/{NODE}/upload"
WRITE_REQ_TOPIC = f"/neuron/{NODE}/write/req"
WRITE_RESP_TOPIC = f"/neuron/{NODE}/write/resp"
READ_REQ_TOPIC = f"/neuron/{NODE}/read/req"
READ_RESP_TOPIC = f"/neuron/{NODE}/read/resp"
UPLOAD_DRV_STATE_TOPIC = f"/neuron/{NODE}/state/update"


@pytest.fixture(autouse=True, scope="class")
def mqtt_node():
    otel_p = subprocess.Popen(
        ["python3", "otel_server.py", f'{otel_port}'], stderr=subprocess.PIPE, cwd="simulator")
    api.add_node_check(NODE, "MQTT")
    api.otel_start("127.0.0.1", otel_port)
    yield
    api.del_node(NODE)
    api.otel_stop()
    otel_p.terminate()
    otel_p.wait()


@pytest.fixture(autouse=True, scope="class")
def modbus_node(mqtt_node):
    try:
        api.add_node_check(DRIVER, "Modbus TCP")
        api.add_group_check(DRIVER, GROUP, INTERVAL)
        api.add_tags_check(DRIVER, GROUP, tags=[TAG0])
        params = {"topic": UPLOAD_TOPIC}
        api.subscribe_group_check(NODE, DRIVER, GROUP, params)
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
    mock = Mock()
    yield mock
    mock.done()


@pytest.fixture(scope="function")
def conf_base(mocker):
    return {
        "client-id": mocker.rand_id(),
        "qos": 0,
        "format": FORMAT_VALUES,
        "write-req-topic": WRITE_REQ_TOPIC,
        "write-resp-topic": WRITE_RESP_TOPIC,
        "offline-cache": False,
        "host": "127.0.0.1",
        "port": mocker.port,
        "ssl": False,
        "version": 5
    }


@pytest.fixture(scope="function")
def conf_fmt_tags(conf_base):
    return {
        **conf_base,
        "format": FORMAT_TAGS,
    }


@pytest.fixture(scope="function")
def conf_user(conf_base, mocker):
    return {
        **conf_base,
        "username": mocker.user,
        "password": mocker.password,
    }


@pytest.fixture(scope="function")
def conf_ssl1(conf_base, mocker):
    return {
        **conf_base,
        "port": mocker.ssl_port,
        "ssl": True,
        "ca": mocker.ca_base64,
    }


@pytest.fixture(scope="function")
def conf_ssl2(conf_ssl1, mocker):
    return {
        **conf_ssl1,
        "cert": mocker.client_cert_base64,
        "key": mocker.client_key_base64,
    }


@pytest.fixture(scope="function")
def conf_user_ssl(conf_base, mocker):
    return {
        **conf_base,
        "username": mocker.user,
        "password": mocker.password,
        "port": mocker.ssl_port,
        "ssl": True,
        "ca": mocker.ca_base64,
        "cert": mocker.client_cert_base64,
        "key": mocker.client_key_base64,
    }


@pytest.fixture(scope="function")
def conf_emqx_ssl(conf_base, mocker):
    return {
        **conf_base,
        "host": "broker.emqx.io",
        "port": 8883,
        "ssl": True,
    }


@pytest.fixture(scope="function")
def conf_cache(conf_fmt_tags):
    conf = {
        **conf_fmt_tags,
        "cache-mem-size": 1,
        "cache-disk-size": 10,
        "cache-sync-interval": 200,
    }
    del conf["offline-cache"]
    return conf


@pytest.fixture(scope="function")
def conf_upload_driver_state(conf_base):
    return {
        **conf_base,
        "upload_drv_state": True,
        "upload_drv_state_topic": f"/neuron/{NODE}/state/update",
        "upload_drv_state_interval": 1,
    }


@pytest.fixture(scope="function")
def conf_bad_host(conf_base):
    return {
        **conf_base,
        "host": "",
    }


@pytest.fixture(scope="function")
def conf_bad_port(conf_base):
    return {
        **conf_base,
        "port": 1_000_000,
    }


@pytest.fixture(scope="function")
def conf_no_id(conf_base):
    del conf_base["client-id"]


@pytest.fixture(scope="function")
def conf_bad_id(conf_base):
    return {
        **conf_base,
        "client-id": "",
    }


@pytest.fixture(scope="function")
def conf_bad_qos(conf_base):
    return {
        **conf_base,
        "qos": -1,
    }


@pytest.fixture(scope="function")
def conf_bad_fmt(conf_base):
    return {
        **conf_base,
        "format": -1,
    }


@pytest.fixture(scope="function")
def conf_ssl_bad_ca(conf_ssl1):
    return {
        **conf_ssl1,
        "ca": "",
    }


@pytest.fixture(scope="function")
def conf_ssl_bad_cert(conf_ssl1):
    return {
        **conf_ssl1,
        "cert": "(@$&*)",
    }


@pytest.fixture(scope="function")
def conf_ssl_no_key(conf_ssl1, mocker):
    return {
        **conf_ssl1,
        "cert": mocker.client_cert_base64
    }


@pytest.fixture(scope="function")
def conf_ssl_bad_key(conf_ssl2):
    return {
        **conf_ssl2,
        "key": "",
    }


@pytest.fixture(scope="function")
def conf_cache_bad_mem(conf_cache):
    return {
        **conf_cache,
        "offline-cache": True,
        "cache-mem-size": 0,
    }


@pytest.fixture(scope="function")
def conf_cache_bad_disk(conf_cache):
    return {
        **conf_cache,
        "offline-cache": True,
        "cache-disk-size": 0,
    }


@pytest.fixture(scope="function")
def conf_cache_bad_sync(conf_cache):
    return {
        **conf_cache,
        "offline-cache": True,
        "cache-sync-interval": 0,
    }


@pytest.fixture(scope="function")
def conf_cache_mem_exceeds_disk(conf_cache):
    return {
        **conf_cache,
        "offline-cache": True,
        "cache-mem-size": 10,
        "cache-disk-size": 1,
    }


@pytest.fixture(scope="function")
def conf_cache_zero_mem(conf_cache):
    return {
        **conf_cache,
        "offline-cache": True,
        "cache-mem-size": 0,
        "cache-disk-size": 10,
    }


@pytest.fixture(scope="function")
def conf_cache_no_mem(conf_base):
    return {
        **conf_base,
        "offline-cache": True,
        "cache-disk-size": 1,
    }


@pytest.fixture(scope="function")
def conf_cache_no_disk(conf_base):
    return {
        **conf_base,
        "offline-cache": True,
        "cache-mem-size": 1,
    }


class TestMQTT:
    @description(
        given="MQTT node and {conf}",
        when="config node",
        then="should fail",
    )
    @pytest.mark.parametrize(
        "conf",
        [
            "conf_bad_host",
            "conf_bad_port",
            "conf_no_id",
            "conf_bad_id",
            "conf_bad_qos",
            "conf_bad_fmt",
            "conf_ssl_bad_ca",
            "conf_ssl_bad_cert",
            "conf_ssl_no_key",
            "conf_ssl_bad_key",
            "conf_cache_bad_mem",
            "conf_cache_bad_disk",
            "conf_cache_bad_sync",
            "conf_cache_mem_exceeds_disk",
            "conf_cache_zero_mem",
            "conf_cache_no_mem",
            "conf_cache_no_disk",
        ],
    )
    def test_mqtt_conf(self, request, conf):
        conf = request.getfixturevalue(conf)
        response = api.node_setting(NODE, conf)
        assert 400 == response.status_code
        assert error.NEU_ERR_NODE_SETTING_INVALID == response.json()["error"]

    @description(
        given="MQTT node and {conf}",
        when="after configuration",
        then="should connect to the broker",
    )
    @pytest.mark.parametrize(
        "conf",
        [
            "conf_base",
            "conf_user",
            "conf_ssl1",
            "conf_ssl2",
            "conf_user_ssl",
        ],
    )
    def test_mqtt_connect(self, request, mocker, conf):
        conf = request.getfixturevalue(conf)
        api.node_setting_check(NODE, conf)
        api.node_ctl(NODE, config.NEU_CTL_START)
        mocker.assert_client_connected()

    @description(
        given="MQTT node and conf_emqx_ssl",
        when="after configuration",
        then="should connect to the broker",
    )
    def test_mqtt_connect_emqx(self, request, mocker, conf_emqx_ssl):
        api.node_setting_check(NODE, conf_emqx_ssl)
        api.node_ctl(NODE, config.NEU_CTL_START)
        response = api.get_nodes_state(NODE)
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()["running"]
        # NOTE: unstable
        # assert config.NEU_NODE_LINK_STATE_CONNECTED == response.json()["link"]

    @description(
        given="MQTT node",
        when="stop/start node",
        then="should disconnect/connect broker",
    )
    def test_mqtt_start_stop(self, request, mocker, conf_base):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl_check(NODE, config.NEU_CTL_STOP)
        mocker.assert_client_disconnected()
        api.node_ctl_check(NODE, config.NEU_CTL_START)
        mocker.assert_client_connected()

    @description(
        given="MQTT node and {conf}",
        when="after configuration",
        then="broker should receive data on upload topic",
    )
    @pytest.mark.parametrize("conf", ["conf_base", "conf_fmt_tags"])
    def test_mqtt_upload(self, mocker, request, conf):
        fmt_tags = conf == "conf_fmt_tags"
        conf = request.getfixturevalue(conf)
        api.node_setting_check(NODE, conf)
        api.node_ctl(NODE, config.NEU_CTL_START)

        msg = mocker.get(UPLOAD_TOPIC, timeout=3)
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
            msg = mocker.get(UPLOAD_TOPIC, timeout=3)
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
        given="MQTT node",
        when="after renaming subscribed driver",
        then="broker should receive data on upload topic",
    )
    def test_mqtt_upload_when_driver_rename(self, mocker, conf_fmt_tags):
        api.node_setting_check(NODE, conf_fmt_tags)
        api.node_ctl(NODE, config.NEU_CTL_START)

        new_name = DRIVER + "-new"
        api.update_node_check(DRIVER, new_name)

        msg = mocker.get(UPLOAD_TOPIC, timeout=3)
        try:
            assert msg is not None
            assert new_name == msg["node"]
            assert GROUP == msg["group"]
            assert TAG0["name"] in [tag["name"] for tag in msg["tags"]]
        finally:
            api.update_node_check(new_name, DRIVER)

    @description(
        given="MQTT node",
        when="after renaming subscribed driver group",
        then="broker should receive data on upload topic",
    )
    def test_mqtt_upload_when_group_rename(self, mocker, conf_fmt_tags):
        api.node_setting_check(NODE, conf_fmt_tags)
        api.node_ctl(NODE, config.NEU_CTL_START)

        new_name = GROUP + "-new"
        api.update_group_check(DRIVER, GROUP, new_name)

        msg = mocker.get(UPLOAD_TOPIC, timeout=3)
        try:
            assert msg is not None
            assert DRIVER == msg["node"]
            assert new_name == msg["group"]
            assert TAG0["name"] in [tag["name"] for tag in msg["tags"]]
        finally:
            api.update_group_check(DRIVER, new_name, GROUP)

    @description(
        given="MQTT node",
        when="delete subscribed driver group and add back group",
        then="broker should receive data on upload topic",
    )
    def test_mqtt_upload_when_group_deleted(self, mocker, conf_fmt_tags):
        api.node_setting_check(NODE, conf_fmt_tags)
        api.node_ctl(NODE, config.NEU_CTL_START)

        # delete driver group
        api.del_group_check(DRIVER, GROUP)
        # add back driver group
        api.add_group_check(DRIVER, GROUP, INTERVAL)
        api.add_tags_check(DRIVER, GROUP, tags=[TAG0])
        # add back subscription on new topic
        topic = UPLOAD_TOPIC + "-new"
        params = {"topic": topic}
        api.subscribe_group_check(NODE, DRIVER, GROUP, params)

        msg = mocker.get(topic, timeout=3)
        try:
            assert msg is not None
            assert DRIVER == msg["node"]
            assert GROUP == msg["group"]
            assert TAG0["name"] in [tag["name"] for tag in msg["tags"]]
        finally:
            params["topic"] = UPLOAD_TOPIC
            api.subscribe_group_update(NODE, DRIVER, GROUP, params)

    @description(
        given="MQTT node and conf_cache",
        when="broken network connection restored",
        then="broker should receive cache data on upload topic",
    )
    def test_mqtt_cache(self, mocker, conf_cache):
        api.node_setting_check(NODE, conf_cache)
        api.node_ctl(NODE, config.NEU_CTL_START)
        mocker.assert_client_connected()

        mocker.offline(seconds=1.5)
        ts = (time.time_ns() // 1_000_000) - INTERVAL
        print("ts ", ts)

        for msg in mocker.get(UPLOAD_TOPIC, num=10, timeout=1.5):
            assert DRIVER == msg["node"]
            assert GROUP == msg["group"]
            assert TAG0["name"] in [tag["name"] for tag in msg["tags"]]
            if msg["timestamp"] < ts:
                break
        else:
            assert False, "no cached message upload"

    @description(
        given="MQTT node",
        when="send bad read req",
        then="should fail",
    )
    def test_mqtt_read_bad_req(self, mocker, conf_base):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        mocker.assert_client_connected()

        req = {
            "uuid": "bca54fe7-a2b1-43e2-a4b4-1da715d28eab",
            "node": DRIVER,
        }
        mocker.pub(READ_REQ_TOPIC, req)
        del req["uuid"]
        mocker.pub(READ_REQ_TOPIC, req)

    @description(
        given="MQTT node",
        when="read driver tag",
        then="should return correct data",
    )
    def test_mqtt_read_tags(self, mocker, conf_base):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        api.write_tag_check(DRIVER, GROUP, TAG0["name"], 123)

        time.sleep(INTERVAL * 5 / 1000)
        req = {
            "uuid": "bca54fe7-a2b1-43e2-a4b4-1da715d28eab",
            "node": DRIVER,
            "group": GROUP,
        }
        msg = mocker.reqresp(READ_RESP_TOPIC, READ_REQ_TOPIC, req, timeout=3)
        assert msg is not None
        assert req["uuid"] == msg["uuid"]
        tag2 = next(
            (tag for tag in msg["tags"] if tag["name"] == TAG0["name"]), None
        )
        assert tag2 is not None

    @description(
        given="MQTT node",
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
    def test_mqtt_write_bad_req(self, mocker, conf_base, req):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)
        mocker.assert_client_connected()

        req = {
            "uuid": "cd32be1b-c8b1-3257-94af-77f847b1ed3e",
            "node": DRIVER,
            "group": GROUP,
            **req,
        }
        mocker.pub(WRITE_REQ_TOPIC, req)
        del req["uuid"]
        mocker.pub(WRITE_REQ_TOPIC, req)

    @description(
        given="MQTT node",
        when="write a driver tag",
        then="should success",
    )
    def test_mqtt_write_tag(self, mocker, conf_base):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)

        req = {
            "uuid": "cd32be1b-c8b1-3257-94af-77f847b1ed3e",
            "node": DRIVER,
            "group": GROUP,
            "tag": TAG0["name"],
            "value": 1234,
        }
        msg = mocker.reqresp(WRITE_RESP_TOPIC, WRITE_REQ_TOPIC, req, timeout=3)
        assert msg is not None
        assert req["uuid"] == msg["uuid"]
        assert error.NEU_ERR_SUCCESS == msg["error"]

        time.sleep(INTERVAL * 5 / 1000)
        assert 1234 == api.read_tag(DRIVER, GROUP, TAG0["name"])

    @description(
        given="MQTT node",
        when="write driver many tags",
        then="should success",
    )
    def test_mqtt_write_tags(self, mocker, conf_base):
        api.node_setting_check(NODE, conf_base)
        api.node_ctl(NODE, config.NEU_CTL_START)

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
            msg = mocker.reqresp(
                WRITE_RESP_TOPIC, WRITE_REQ_TOPIC, req, timeout=3
            )
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

    @description(
        given="MQTT node",
        when="after configuration of upload_driver_state",
        then="broker should receive data on upload driver state topic",
    )
    def test_mqtt_upload_driver_state(self, mocker, conf_upload_driver_state):
        api.node_setting_check(NODE, conf_upload_driver_state)
        api.node_ctl(NODE, config.NEU_CTL_START)

        msg = mocker.get(UPLOAD_DRV_STATE_TOPIC, timeout=3)
        assert msg is not None

    @description(
        given="MQTT node and delete all drivers",
        when="after configuration of upload_driver_state",
        then="broker should not receive any data on upload driver state topic",
    )
    def test_mqtt_upload_driver_state_none(self, mocker, conf_upload_driver_state):
        api.del_node_check(DRIVER)
        api.node_setting_check(NODE, conf_upload_driver_state)
        api.node_ctl(NODE, config.NEU_CTL_START)

        msg = mocker.get(UPLOAD_DRV_STATE_TOPIC, timeout=3)
        assert msg is None