import subprocess
import time


class Broker:
    """Use Mosquitto as MQTT broker for testing."""

    def __init__(
        self,
        port=1883,
        user=None,
        password=None,
        ssl=None,
        ssl_port=8883,
        ca=None,
        cert=None,
        key=None,
    ):
        """
        `user`     MQTT user name if provided.
        `password` MQTT user password if provided.
        `ssl`      Enable ssl if true.
        `ca`       Broker ssl ca file.
        `cert`     Broker ssl cert file.
        `key`      Broker ssl key file.
        """
        self.port = port
        self.user = user
        self.password = password
        self.ssl = ssl
        self.ssl_port = ssl_port
        self.ca = ca
        self.cert = cert
        self.key = key
        self.conf_file = "/tmp/neuron-ft-mosquitto.conf"
        self.pass_file = "/tmp/neuron-ft-mosquitto.pass"

    def _write_conf(self):
        conf = [
            "sys_interval 1",
            "allow_anonymous true",
        ]

        if self.user:
            conf.append("password_file %s" % self.pass_file)
            subprocess.run(
                [
                    f"truncate --size 0 {self.pass_file}; mosquitto_passwd -b '{self.pass_file}' '{self.user}' '{self.password}'",
                ],
                check=True,
                shell=True,
            )

        conf.append("listener %s 127.0.0.1" % self.port)

        if self.ssl:
            conf.append(f"listener {self.ssl_port} 127.0.0.1")
            conf.append(f"cafile {self.ca}")
            conf.append(f"certfile {self.cert}")
            conf.append(f"keyfile {self.key}")

        with open(self.conf_file, "w") as f:
            f.write("\n".join(conf) + "\n")

    def start(self):
        """Start broker process."""
        self._write_conf()
        proc = subprocess.Popen(
            ["/usr/sbin/mosquitto", "-c", self.conf_file],
            stderr=subprocess.PIPE,
        )
        time.sleep(1)
        assert proc.poll() is None
        self.proc = proc

    def stop(self):
        """Stop broker process."""
        if self.proc:
            self.proc.terminate()
            # stderr = self.proc.stderr.read().decode()
            # print(stderr)


if __name__ == "__main__":
    broker = Broker(user="neuron", password="neuron")
    broker.start()
    time.sleep(60)
    broker.stop()
