import asyncio
import time
from collections import defaultdict, deque

from robot.api import logger
from robot.api.deco import keyword, library
from gmqtt import Client as MQTTClient, Subscription
from gmqtt.mqtt.constants import MQTTv311

ROBOT_AUTO_KEYWORDS = False


@library
class Client:
    """A keyword library for Robot Framework. It provides keywords for
    performing various operations on an MQTT broker. See http://mqtt.org/
    for more details on MQTT specification.

    This library uses gmqtt: Python async MQTT client. For more information
    on underlying methods and documentation, see:
        https://github.com/wialon/gmqtt

    """

    ROBOT_LIBRARY_SCOPE = "GLOBAL"
    ROBOT_LIBRARY_VERSION = "0.1.0"

    def __init__(self, reconnect_retries=5, reconnect_delay=0):
        """
        `reconnect_retries` number of reconnect attempt in case of lost
        connections, default to 5

        `reconnect_delay` delay between reconnect attemps in seconds,
        default to 0

        """
        self._client = None
        self._reconnect_retries = reconnect_retries
        self._reconnect_delay = reconnect_delay
        self._messages = defaultdict(deque)
        self._subcribed = asyncio.Event()
        self._unsubcribed = asyncio.Event()
        self._recved = asyncio.Event()
        self._run = asyncio.get_event_loop().run_until_complete

    @keyword
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

        Examples:

        Connect to a broker with default port and client id
        | Connect | 127.0.0.1 |

        Connect to a broker by specifying the port and client id explicitly
        | Connect | 127.0.0.1 | 1883 | test.client |

        Connect to a broker with clean session flag set to false
        | Connect | 127.0.0.1 | clean_session=${false} |

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

        self._run(self._client.connect(host, port, version=MQTTv311))

    @keyword
    def disconnect(self):
        """Disconnect from MQTT Broker.

        Example:
        | Disconnect |

        """
        self._run(self._client.disconnect())
        self._messages.clear()

    @keyword
    def unsubscribe_all_and_clear_messages(self):
        """Unsubscribe all subscriptions and clear all messages in queue."""
        for topic in self._messages:
            self.unsubscribe(topic)
        self._messages.clear()

    @keyword
    def subscribe(self, topic, qos=0, timeout=1):
        """Subscribe to a topic and return a message payload received
            within the specified time.

        `topic` topic to subscribe to

        `qos` quality of service for the subscription, default to QoS 0

        `timeout` duration of subscription, default to 1 second

        Examples:

        Subscribe and get a list of all messages received within 5 seconds
        | ${messages}= | Subscribe | test/test | qos=1 | timeout=5 |

        """
        timeout = 1 if timeout < 1 else timeout
        self._client.subscribe(Subscription(topic, qos))
        if self._wait(self._subcribed, timeout=timeout):
            raise Exception(
                "MQTT client subscribe timeout topic=%s qos=%s" % (topic, qos))
        logger.info("MQTT client subscribed to topic=%s qos=%s" % (topic, qos))

    @keyword
    def unsubscribe(self, topic, timeout=1):
        """Unsubscribe the client from the specified topic.

        `topic` topic to unsubscribe from

        `timeout` duration of unsubscription, default to 1 second

        Example:
        | Unsubscribe | test/mqtt_test |

        """
        timeout = 1 if timeout < 1 else timeout
        self._client.unsubscribe(topic)
        if self._wait(self._unsubcribed, timeout=timeout):
            raise Exception("MQTT client unsubscribe timeout topic=%s" % topic)
        logger.info("MQTT client unsubscribed topic=%s" % topic)

    @keyword
    def publish(self, topic, payload, qos=0, retain=False):
        """Publish a message to a topic with specified qos and retained flag.
        It is required that a connection has been established using `Connect`
        keyword before using this keyword.

        `topic` topic to which the message will be published

        `paylod` message payload to publish

        `qos` qos of the message, default to QoS 0

        `retain` retained flag

        Examples:

        | Publish | test/test | test message | 1 | ${false} |

        """
        self._client.publish(topic, payload, qos=qos, retain=retain)

    @keyword
    def listen(self, topic, timeout=1):
        """Listen to a topic and return a message payload received within the
        specified time. Requires an Subscribe to have been called previously.

        `topic` topic to listen to

        `timeout` duration to listen, default to 1 second

        Examples:

        Listen and get a message received within 5 seconds
        | ${messages}= | Listen | test/test | timeout=5 |

        """
        timeout = 1 if timeout < 1 else timeout
        start = time.time()
        while True:
            if topic in self._messages and self._messages[topic]:
                msg = self._messages[topic].popleft()
                return msg

            if (
                self._wait(self._recved, timeout=timeout)
                or (time.time() - start) >= timeout
            ):
                break

    def _wait(self, event, timeout):
        """Waits for the given event within timeout.
        Returns true on timeout, false otherwise."""
        try:
            self._run(asyncio.wait_for(event.wait(), timeout))
            event.clear()
        except asyncio.TimeoutError:
            return True
        return False

    def _on_connect(self, client, flags, rc, properties):
        logger.info("MQTT client connected")

    def _on_message(self, client, topic, payload, qos, properties):
        logger.info("MQTT client received: topic=%s qos=%s payload=%s" %
                    (topic, qos, payload))
        self._messages[topic].append(payload.decode())
        self._recved.set()
        return 0

    def _on_disconnect(self, client, packet, exc=None):
        logger.info("MQTT client disconnected")

    def _on_subscribe(self, client, mid, qos, properties):
        self._subcribed.set()

    def _on_unsubscribe(self, client, mid, granted_qos):
        self._unsubcribed.set()


if __name__ == "__main__":
    # import logging
    # logging.basicConfig(level=logging.DEBUG)

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
