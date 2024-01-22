import json
import pathlib
import random
import string
import time

from neuron import data
from neuron.mqtt.client import Client
from neuron.mqtt.broker import Broker


def rand_client_id():
    return "neuron_" + "".join(
        random.choices(string.ascii_uppercase + string.digits, k=6)
    )


class Mock:
    def __init__(
        self, port=1883, ssl_port=8883, user="neuron", password="neuron"
    ):
        self.port = port
        self.ssl_port = ssl_port
        self.user = user
        self.password = password

        self.ca = data.ca
        self.ca_base64 = data.ca_base64
        self.broker_cert = data.server_cert
        self.broker_cert_base64 = data.server_cert_base64
        self.broker_key = data.server_key
        self.broker_key_base64 = data.server_key_base64
        self.client_cert = data.client_cert
        self.client_cert_base64 = data.client_cert_base64
        self.client_key = data.client_key
        self.client_key_base64 = data.client_key_base64

        self.broker = Broker(
            port=port,
            user=user,
            password=password,
            ssl=True,
            ssl_port=ssl_port,
            ca=self.ca,
            cert=self.broker_cert,
            key=self.broker_key,
        )
        self.broker.start()

        self.client = Client()
        self.client.connect("127.0.0.1")

    def done(self):
        self.client.disconnect()
        self.broker.stop()

    def rand_id(self):
        """Return a random MQTT client id."""
        return rand_client_id()

    def offline(self, seconds):
        self.client.disconnect()
        self.broker.stop()
        time.sleep(seconds)
        self.broker.start()
        self.client.connect("127.0.0.1")

    def assert_client_disconnected(self):
        """Return number of connected clients to the broker."""
        topic = "$SYS/broker/clients/connected"
        messages = self.get(topic, num=2, timeout=2)
        n = int(messages[-1]) - 1  # exclude out client
        assert n == 0, "there are connected mqtt clients"

    def assert_client_connected(self):
        """Return number of connected clients to the broker."""
        topic = "$SYS/broker/clients/connected"
        n = 0
        for _ in range(8):
            messages = self.get(topic, num=2, timeout=1.25)
            n = int(messages[-1]) - 1  # exclude out client
            if n != 0:
                break
        assert n > 0, "there are no connected mqtt clients"

    def sub(self, topic, handler, no_except=False):
        self.client.subscribe(topic, handler=handler, no_except=no_except)

    def unsub(self, topic, no_except=False):
        self.client.unsubscribe(topic, no_except=no_except)

    def get(self, topic, num=1, timeout=1, no_except=False):
        """Get message from the specified topic."""
        if num < 1:
            return None

        try:
            messages = []
            self.client.subscribe(topic)
            start = time.time_ns()
            while time.time_ns() - start <= timeout * 1_000_000_000:
                msg = self.client.listen(topic, timeout=0.1)
                if not msg:
                    continue
                msg = json.loads(msg)
                messages.append(msg)
                if len(messages) == num:
                    break
        except Exception as e:
            if no_except:
                print("WARN:", e)
            else:
                raise e
        finally:
            self.client.unsubscribe(topic, no_except=no_except)

        if num > 1:
            return messages
        else:
            return messages[0] if len(messages) == 1 else None

    def pub(self, topic, payload, qos=0):
        """Publish a message to the specified topic."""
        self.client.publish(topic, payload, qos=qos, retain=False)

    def reqresp(self, resp_topic, req_topic, payload, qos=0, timeout=1):
        """Publish to the given request topic until response received on the given response topic."""
        try:
            self.client.subscribe(resp_topic)
            start = time.time_ns()
            # try request until response received
            while time.time_ns() - start <= timeout * 1_000_000_000:
                self.pub(req_topic, payload, qos=qos)
                msg = self.client.listen(resp_topic, timeout=0.1)
                if msg:
                    msg = json.loads(msg)
                    return msg
        finally:
            self.client.unsubscribe(resp_topic)
