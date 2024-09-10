import asyncio
import time
from collections import defaultdict, deque

from gmqtt import Client as MQTTClient, Subscription
from gmqtt.mqtt.constants import MQTTv311, MQTTv50


def info(msg):
    print(msg)


class Client:
    """
    This library uses gmqtt: Python async MQTT client. For more information
    on underlying methods and documentation, see:
        https://github.com/wialon/gmqtt

    """

    def __init__(self, reconnect_retries=-1, reconnect_delay=0.1):
        """
        `reconnect_retries` number of reconnect attempt in case of lost
        connections, default to -1

        `reconnect_delay` delay between reconnect attemps in seconds,
        default to 0.1

        """
        self._client = None
        self._reconnect_retries = reconnect_retries
        self._reconnect_delay = reconnect_delay
        self._messages = defaultdict(deque)
        self._handlers = {}
        self._subcribed = asyncio.Event()
        self._unsubcribed = asyncio.Event()
        self._recved = asyncio.Event()
        self._run = asyncio.get_event_loop().run_until_complete

    def connect(
        self,
        host,
        port=1883,
        client_id="",
        username="",
        password="",
        keepalive=60,
        clean_session=True,
    ):
        """Connect to an MQTT broker. This is a pre-requisite step for publish
        and subscribe keywords.

        `host` MQTT broker host

        `port` broker port (default 1883)

        `client_id` if not specified, a random id is generated

        `username` user name, default to ""

        `password` user password, default to ""

        `keepalive` keepalive in seconds

        `clean_session` specifies the clean session flag for the connection

        """
        client = MQTTClient(client_id, clean_session=clean_session)
        client.on_connect = self._on_connect
        client.on_message = self._on_message
        client.on_disconnect = self._on_disconnect
        client.on_subscribe = self._on_subscribe
        client.on_unsubscribe = self._on_unsubscribe
        client.set_config(
            {
                "reconnect_retries": self._reconnect_retries,
                "reconnect_delay": self._reconnect_delay,
            }
        )
        self._client = client

        self._run(self._client.connect(host, port, version=MQTTv50))

    def disconnect(self):
        """Disconnect from MQTT Broker."""
        self._run(self._client.disconnect())
        self._messages.clear()
        self._handlers.clear()

    def unsubscribe_all_and_clear_messages(self):
        """Unsubscribe all subscriptions and clear all messages in queue."""
        for topic in self._messages:
            self.unsubscribe(topic)
        self._messages.clear()
        self._handlers.clear()

    def subscribe(
        self, topic, qos=0, timeout=1, handler=None, no_except=False
    ):
        """Subscribe to a topic and return a message payload received
            within the specified time.

        `topic` topic to subscribe to

        `qos` quality of service for the subscription, default to QoS 0

        `timeout` duration of subscription, default to 1 second

        `handler` callback on received message, should take one parameter `payload`

        `no_except` indicate to not throw exception

        """
        self._client.subscribe(Subscription(topic, qos))
        if self._wait(self._subcribed, timeout=timeout):
            if no_except:
                info(
                    f"WARN: MQTT client subscribe timeout, topic={topic} qos={qos}"
                )
            else:
                raise Exception(
                    f"MQTT client subscribe timeout, topic={topic} qos={qos}"
                )
        info(f"MQTT client subscribed to topic={topic} qos={qos}")
        if handler:
            self._handlers[topic] = handler

    def unsubscribe(self, topic, timeout=1, no_except=False):
        """Unsubscribe the client from the specified topic.

        `topic` topic to unsubscribe from

        `timeout` duration of unsubscription, default to 1 second

        `no_except` indicate to not throw exception

        """
        self._client.unsubscribe(topic)
        if self._wait(self._unsubcribed, timeout=timeout):
            if no_except:
                info(f"WARN: MQTT client unsubscribe timeout, topic={topic}")
            else:
                raise Exception(
                    f"MQTT client unsubscribe timeout, topic={topic}"
                )
        self._messages[topic].clear()
        self._handlers.pop(topic, None)
        info(f"MQTT client unsubscribed topic={topic}")

    def publish(self, topic, payload, qos=0, retain=False):
        """Publish a message to a topic with specified qos and retained flag.
        It is required that a connection has been established using `Connect`
        keyword before using this keyword.

        `topic` topic to which the message will be published

        `paylod` message payload to publish

        `qos` qos of the message, default to QoS 0

        `retain` retained flag

        """
        self._client.publish(topic, payload, qos=qos,
                             retain=retain, user_property=[('traceparent', '00-8fb7651916cd43dd8448eb211c80319c-0000000000000000-01'), ('tracestate', 'service.name=ex,a=b,c=d,e=f,g=h')])

    def listen(self, topic, timeout=1):
        """Listen to a topic and return a message payload received within the
        specified time. Requires an Subscribe to have been called previously.

        `topic` topic to listen to

        `timeout` duration to listen, default to 1 second

        """
        start = time.time_ns()
        while True:
            if topic in self._messages and self._messages[topic]:
                msg = self._messages[topic].popleft()
                return msg

            if (
                self._wait(self._recved, timeout=timeout)
                or (time.time_ns() - start) >= timeout * 1_000_000_000
            ):
                break

    def _wait(self, event, timeout):
        """Waits for the given event within timeout.
        Returns true on timeout, false otherwise."""
        try:
            self._run(asyncio.wait_for(event.wait(), timeout))
        except asyncio.TimeoutError:
            return True
        finally:
            event.clear()
        return False

    def _on_connect(self, client, flags, rc, properties):
        info("MQTT client connected")

    def _on_message(self, client, topic, payload, qos, properties):
        info(
            "MQTT client received: topic=%s qos=%s payload=%s"
            % (topic, qos, payload)
        )
        if topic in self._handlers:
            self._handlers[topic](payload=payload.decode())
        else:
            self._messages[topic].append(payload.decode())
        self._recved.set()
        return 0

    def _on_disconnect(self, client, packet, exc=None):
        info("MQTT client disconnected")

    def _on_subscribe(self, client, mid, qos, properties):
        self._subcribed.set()

    def _on_unsubscribe(self, client, mid, granted_qos):
        self._unsubcribed.set()


if __name__ == "__main__":
    host = "127.0.0.1"
    topic = "test/test_mqtt"
    client = Client()

    client.connect(host)
    client.subscribe(topic)

    for i in range(3):
        client.publish(topic, "hello world %s" % i)
        msg = client.listen(topic)
        print("MQTT client receive: topic=%s msg=%s" % (topic, msg))

    client.unsubscribe(topic)
    client.disconnect()
