import json
import os
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

KAFKA_BROKER = os.environ.get("KAFKA_BROKER", "127.0.0.1:9092")
NODE = "kafka"
DRIVER = "modbus"
GROUP = "group"
INTERVAL = 100
TOPIC = "neuron-ft-test"
FORMAT_VALUES = 0
FORMAT_TAGS = 1

TAG0 = {
    "name": "tag0",
    "address": "1!400001",
    "attribute": config.NEU_TAG_ATTRIBUTE_RW,
    "type": config.NEU_TYPE_INT16,
}
TAG_ERR = {
    "name": "tag_err",
    "address": "100!400001",
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
]


def kafka_consume_one(broker, topic, group_id, timeout=10):
    """Consume one message from kafka, return parsed JSON or None."""
    from confluent_kafka import Consumer, KafkaError

    consumer = Consumer(
        {
            "bootstrap.servers": broker,
            "group.id": group_id,
            "auto.offset.reset": "latest",
        }
    )
    consumer.subscribe([topic])
    consumer.poll(timeout=2.0)

    try:
        start = time.time()
        while time.time() - start < timeout:
            raw = consumer.poll(timeout=1.0)
            if raw is None:
                continue
            if raw.error():
                if raw.error().code() == KafkaError._PARTITION_EOF:
                    continue
                raise Exception(raw.error())
            return json.loads(raw.value())
        return None
    finally:
        consumer.close()


def kafka_consume_until(broker, topic, group_id, predicate, timeout=10):
    """Consume messages until predicate(msg) returns True."""
    from confluent_kafka import Consumer, KafkaError

    consumer = Consumer(
        {
            "bootstrap.servers": broker,
            "group.id": group_id,
            "auto.offset.reset": "latest",
        }
    )
    consumer.subscribe([topic])
    consumer.poll(timeout=2.0)

    try:
        start = time.time()
        while time.time() - start < timeout:
            raw = consumer.poll(timeout=1.0)
            if raw is None:
                continue
            if raw.error():
                if raw.error().code() == KafkaError._PARTITION_EOF:
                    continue
                raise Exception(raw.error())
            msg = json.loads(raw.value())
            if predicate(msg):
                return msg
        return None
    finally:
        consumer.close()


def kafka_drain(broker, topic, group_id, seconds=3):
    """Consume for a duration, return True if any message received."""
    from confluent_kafka import Consumer, KafkaError

    consumer = Consumer(
        {
            "bootstrap.servers": broker,
            "group.id": group_id,
            "auto.offset.reset": "latest",
        }
    )
    consumer.subscribe([topic])
    consumer.poll(timeout=2.0)

    got_msg = False
    try:
        start = time.time()
        while time.time() - start < seconds:
            raw = consumer.poll(timeout=0.5)
            if raw and not raw.error():
                got_msg = True
        return got_msg
    finally:
        consumer.close()


@pytest.fixture(autouse=True, scope="class")
def kafka_node():
    api.add_node_check(NODE, config.PLUGIN_KAFKA)
    yield
    api.del_node(NODE)


@pytest.fixture(autouse=True, scope="class")
def modbus_node(kafka_node):
    try:
        api.add_node_check(DRIVER, config.PLUGIN_MODBUS_TCP)
        api.add_group_check(DRIVER, GROUP, INTERVAL)
        api.add_tags_check(DRIVER, GROUP, tags=[TAG0])
        params = {"topic": TOPIC}
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


class TestKafka:

    # ─── config validation ────────────────────────────────────────────

    @description(
        given="Kafka node and {conf}",
        when="config node",
        then="should fail",
    )
    @pytest.mark.parametrize(
        "conf",
        [
            {"broker": "", "topic": TOPIC, "format": FORMAT_VALUES},
            {"broker": KAFKA_BROKER, "topic": "", "format": FORMAT_VALUES},
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": -1},
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": 99},
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_VALUES,
             "acks": 2},
        ],
        ids=[
            "empty-broker",
            "empty-topic",
            "bad-format-neg",
            "bad-format-big",
            "bad-acks",
        ],
    )
    def test_kafka_conf_invalid(self, conf):
        response = api.node_setting(NODE, conf)
        assert 400 == response.status_code
        assert error.NEU_ERR_NODE_SETTING_INVALID == response.json()["error"]

    @description(
        given="Kafka node and valid minimal config",
        when="config node",
        then="should succeed",
    )
    def test_kafka_conf_minimal(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_VALUES},
        )

    @description(
        given="Kafka node and config with all optional params",
        when="config node",
        then="should succeed",
    )
    def test_kafka_conf_full(self):
        api.node_setting_check(
            NODE,
            {
                "broker": KAFKA_BROKER,
                "topic": TOPIC,
                "format": FORMAT_TAGS,
                "upload_err": False,
                "compression": 4,
                "batch-max-messages": 5000,
                "linger-ms": 10,
                "message-timeout-ms": 10000,
                "acks": 1,
                "client-id": "neuron-ft",
            },
        )

    @description(
        given="Kafka node configured",
        when="read back setting",
        then="values should match",
    )
    def test_kafka_get_setting(self):
        api.node_setting_check(
            NODE,
            {
                "broker": KAFKA_BROKER,
                "topic": TOPIC,
                "format": FORMAT_VALUES,
                "compression": 2,
                "batch-max-messages": 8000,
                "linger-ms": 20,
                "message-timeout-ms": 15000,
                "acks": 0,
                "client-id": "neuron-test",
            },
        )
        response = api.get_node_setting(NODE)
        assert 200 == response.status_code
        params = response.json()["params"]
        assert KAFKA_BROKER == params["broker"]
        assert TOPIC == params["topic"]
        assert FORMAT_VALUES == params["format"]
        assert 2 == params["compression"]
        assert 8000 == params["batch-max-messages"]
        assert 20 == params["linger-ms"]
        assert 15000 == params["message-timeout-ms"]
        assert 0 == params["acks"]
        assert "neuron-test" == params["client-id"]

    # ─── node lifecycle ───────────────────────────────────────────────

    @description(
        given="Kafka node with valid config",
        when="start and stop node",
        then="state should change accordingly",
    )
    def test_kafka_start_stop(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_VALUES},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)
        response = api.get_nodes_state(NODE)
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_RUNNING == response.json()["running"]

        api.node_ctl(NODE, config.NEU_CTL_STOP)
        response = api.get_nodes_state(NODE)
        assert 200 == response.status_code
        assert config.NEU_NODE_STATE_STOPPED == response.json()["running"]

    @description(
        given="Kafka node running",
        when="re-configure with different topic",
        then="should succeed without restart",
    )
    def test_kafka_reconfig(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_VALUES},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)

        new_topic = TOPIC + "-reconf"
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": new_topic, "format": FORMAT_TAGS},
        )
        response = api.get_node_setting(NODE)
        assert 200 == response.status_code
        assert new_topic == response.json()["params"]["topic"]
        assert FORMAT_TAGS == response.json()["params"]["format"]

    @description(
        given="Kafka node with valid config",
        when="start node and wait",
        then="should connect to broker",
    )
    def test_kafka_connect(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_VALUES},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)

        connected = False
        for _ in range(10):
            time.sleep(1)
            response = api.get_nodes_state(NODE)
            assert 200 == response.status_code
            if (
                config.NEU_NODE_LINK_STATE_CONNECTED
                == response.json()["link"]
            ):
                connected = True
                break
        assert connected, "kafka node did not connect to broker"

    # ─── data upload ──────────────────────────────────────────────────

    @description(
        given="Kafka node with values-format",
        when="driver produces data",
        then="message should have values/errors structure",
    )
    def test_kafka_upload_values(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_VALUES},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)
        time.sleep(2)

        msg = kafka_consume_until(
            KAFKA_BROKER,
            TOPIC,
            "ft-values",
            lambda m: "values" in m or "errors" in m,
        )
        assert msg is not None, "no values-format message received from kafka"
        assert DRIVER == msg["node"]
        assert GROUP == msg["group"]
        assert "timestamp" in msg
        assert (
            TAG0["name"] in msg.get("values", {})
            or TAG0["name"] in msg.get("errors", {})
        )

    @description(
        given="Kafka node with tags-format",
        when="driver produces data",
        then="message should have tags array structure",
    )
    def test_kafka_upload_tags(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_TAGS},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)
        time.sleep(2)

        msg = kafka_consume_until(
            KAFKA_BROKER,
            TOPIC,
            "ft-tags",
            lambda m: "tags" in m,
        )
        assert msg is not None, "no tags-format message received from kafka"
        assert DRIVER == msg["node"]
        assert GROUP == msg["group"]
        assert "tags" in msg
        assert TAG0["name"] in [tag["name"] for tag in msg["tags"]]

    @description(
        given="Kafka node with values-format and a written tag",
        when="driver reports written value",
        then="kafka message should contain the exact value",
    )
    def test_kafka_upload_written_value(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_VALUES},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)
        api.write_tag_check(DRIVER, GROUP, TAG0["name"], 12345)
        time.sleep(INTERVAL * 5 / 1000)

        msg = kafka_consume_until(
            KAFKA_BROKER,
            TOPIC,
            "ft-write-val",
            lambda m: m.get("values", {}).get(TAG0["name"]) == 12345,
        )
        assert msg is not None, "did not receive message with written value"
        assert 12345 == msg["values"][TAG0["name"]]

    @description(
        given="Kafka node with multiple tags",
        when="driver produces data",
        then="all tags should appear in message",
    )
    def test_kafka_upload_multi_tags(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_TAGS},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)
        time.sleep(2)

        try:
            api.add_tags_check(DRIVER, GROUP, TAGS)

            msg = kafka_consume_until(
                KAFKA_BROKER,
                TOPIC,
                "ft-multi",
                lambda m: len(m.get("tags", [])) >= len(TAGS) + 1,
            )
            assert msg is not None, "no message with all tags"
            tag_names = [t["name"] for t in msg["tags"]]
            assert TAG0["name"] in tag_names
            for tag in TAGS:
                assert tag["name"] in tag_names
        finally:
            api.del_tags(DRIVER, GROUP, [t["name"] for t in TAGS])

    @description(
        given="Kafka node with upload_err=false and an error tag",
        when="driver reports error tag",
        then="error tag should be filtered from kafka message",
    )
    def test_kafka_upload_err_filter(self):
        api.node_setting_check(
            NODE,
            {
                "broker": KAFKA_BROKER,
                "topic": TOPIC,
                "format": FORMAT_TAGS,
                "upload_err": False,
            },
        )
        api.node_ctl(NODE, config.NEU_CTL_START)
        time.sleep(2)

        try:
            api.add_tags_check(DRIVER, GROUP, tags=[TAG_ERR])

            msg = kafka_consume_one(KAFKA_BROKER, TOPIC, "ft-err-filter")
            assert msg is not None, "no message received"
            tag_names = [t["name"] for t in msg.get("tags", [])]
            assert TAG_ERR["name"] not in tag_names
        finally:
            api.del_tags(DRIVER, GROUP, [TAG_ERR["name"]])

    # ─── topic routing ────────────────────────────────────────────────

    @description(
        given="Kafka node with custom topic per subscription",
        when="driver produces data",
        then="data should arrive on the custom topic",
    )
    def test_kafka_custom_topic(self):
        custom_topic = TOPIC + "-custom"

        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_VALUES},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)
        time.sleep(2)

        params = {"topic": custom_topic}
        api.subscribe_group_update(NODE, DRIVER, GROUP, params)

        try:
            msg = kafka_consume_one(KAFKA_BROKER, custom_topic, "ft-custom")
            assert msg is not None, "no message on custom topic"
            assert DRIVER == msg["node"]
            assert GROUP == msg["group"]
        finally:
            params = {"topic": TOPIC}
            api.subscribe_group_update(NODE, DRIVER, GROUP, params)

    @description(
        given="Kafka node subscribed to a driver",
        when="driver is renamed",
        then="data should still arrive with the new driver name",
    )
    def test_kafka_driver_rename(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_TAGS},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)
        time.sleep(2)

        new_name = DRIVER + "-new"
        api.update_node_check(DRIVER, new_name)

        try:
            msg = kafka_consume_until(
                KAFKA_BROKER,
                TOPIC,
                "ft-drv-rename",
                lambda m: m.get("node") == new_name,
            )
            assert msg is not None, "no message after driver rename"
            assert new_name == msg["node"]
            assert GROUP == msg["group"]
        finally:
            api.update_node_check(new_name, DRIVER)

    @description(
        given="Kafka node subscribed to a group",
        when="group is renamed",
        then="data should still arrive with the new group name",
    )
    def test_kafka_group_rename(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_TAGS},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)
        time.sleep(2)

        new_name = GROUP + "-new"
        api.update_group_check(DRIVER, GROUP, new_name)

        try:
            msg = kafka_consume_until(
                KAFKA_BROKER,
                TOPIC,
                "ft-grp-rename",
                lambda m: m.get("group") == new_name,
            )
            assert msg is not None, "no message after group rename"
            assert DRIVER == msg["node"]
            assert new_name == msg["group"]
        finally:
            api.update_group_check(DRIVER, new_name, GROUP)

    @description(
        given="Kafka node subscribed to a group",
        when="unsubscribe then re-subscribe with new topic",
        then="data should arrive on the new topic",
    )
    def test_kafka_unsubscribe_resubscribe(self):
        api.node_setting_check(
            NODE,
            {"broker": KAFKA_BROKER, "topic": TOPIC, "format": FORMAT_VALUES},
        )
        api.node_ctl(NODE, config.NEU_CTL_START)
        time.sleep(2)

        api.unsubscribe_group(NODE, DRIVER, GROUP)
        time.sleep(1)

        new_topic = TOPIC + "-resub"
        params = {"topic": new_topic}
        api.subscribe_group_check(NODE, DRIVER, GROUP, params)

        try:
            msg = kafka_consume_one(KAFKA_BROKER, new_topic, "ft-resub")
            assert msg is not None, "no message on re-subscribed topic"
            assert DRIVER == msg["node"]
            assert GROUP == msg["group"]
        finally:
            params = {"topic": TOPIC}
            api.subscribe_group_update(NODE, DRIVER, GROUP, params)
