# Neuron

[![GitHub Release](https://img.shields.io/github/release/emqx/neuron?color=brightgreen&label=Release)](https://github.com/emqx/neuron/releases)
[![Docker Pulls](https://img.shields.io/docker/pulls/emqx/neuron?label=Docker%20Pulls)](https://hub.docker.com/r/emqx/neuron)
[![Slack](https://img.shields.io/badge/Slack-Neuron-39AE85?logo=slack)](https://slack-invite.emqx.io/)
[![Discord](https://img.shields.io/discord/931086341838622751?label=Discord&logo=discord)](https://discord.gg/xYGf3fQnES)
[![Twitter](https://img.shields.io/badge/Follow-EMQ-1DA1F2?logo=twitter)](https://twitter.com/EMQTech)
[![YouTube](https://img.shields.io/badge/Subscribe-EMQ-FF0000?logo=youtube)](https://www.youtube.com/channel/UC5FjR77ErAxvZENEWzQaO5Q)

Neuron is an Industrial IoT (IIoT) connectivity server for modern big data and AI/ML technology to leverage the power of Industrial 4.0. It supports one-stop access to dozens of industrial protocols and converts them into MQTT protocol and realize the interconnection between IIoT platforms and various industrial devices.

![neuron-overview](docs/pictures/neuron-final.png)

The following are some important features of Neuron:

- Edge native application with real-time capability to leverage the low latency network of edge side.
- Loosely-coupled modularity [architecture design](https://neugates.io/docs/en/latest/architecture/architecture.html) for extending more functional services by pluggable modules.
- Support hot plugins that can update device and application modules during runtime.
- Support numerous protocols for industrial devices, including Modbus, OPCUA, Ethernet/IP, IEC104, BACnet and [more](https://neugates.io/docs/en/latest/introduction/module-list/module-list.html).
- Support simultaneous connection of a large number of industrial devices with different protocols.
- Combine with the rule engine function provided by [eKuiper](https://www.lfedge.org/projects/ekuiper) to quickly implement rule-based device control or AI/ML analytics.
- Support data access to industrial applications, such as MES or ERP, SCADA, historian and data analytics software via [SparkplugB](https://neugates.io/docs/en/latest/use-cases/use_cases.html) solution.
- Has very low memory footprint, less than 10M, and CPU usage, can run on limited resource hardware like ARM, x86 and RISC-V.
- Support installation of native executable or deployed in containerized enviornment.
- Control industrial devices, and make changes to the parameters and data tags through [API](https://neugates.io/docs/en/latest/http-api/configuration.html) and [MQTT](https://neugates.io/docs/en/latest/north-apps/mqtt/api.html) services.
- Highly integrated with other EMQ products, including [EMQX](https://www.emqx.com/en/products/emqx), [NanoMQ](https://nanomq.io/), [eKuiper](https://ekuiper.org/).
- The code of the core framework and Modbus, MQTT and eKuiper are licensed under open source LGPLv3. Commercial modules require a [EMQ License](https://neugates.io/docs/en/latest/introduction/license-describe.html) to run.

For more information, please visit [Neuron homepage](https://neugates.io/).

## Get Started

#### Run Neuron using Docker

```
docker run -d --name neuron -p 7000:7000 -p 7001:7001 -p 9081:9081 --privileged=true --restart=always emqx/neuron:latest
```

Next, please follow the [getting started guide](https://neugates.io/docs/en/latest/quick-start/installation.html) to tour the Neuron features.

> **Limitations**
>
> Neuron open source edition only includes Modbus and MQTT drivers.
>
> [Apply for a 15-day trial license](https://www.emqx.com/en/apply-licenses/neuron) to load all drivers.

#### More installation options

If you prefer to install and manage Neuron yourself, you can download the latest version from [neugates.io/downloads?os=Linux](https://neugates.io/downloads?os=Linux).

For more installation options, see the [Neuron installation documentation](https://neugates.io/docs/en/latest/quick-start/installation.html).

## Documentation

The Neuron documentation is available at [neugates.io/docs/en/latest/](https://neugates.io/docs/en/latest/).

## Get Involved

- Follow [@EMQTech on Twitter](https://twitter.com/EMQTech).
- If you have a specific question, check out our [discussion forums](https://github.com/emqx/neuron/discussions).
- For general discussions, join us on the [official Discord](https://discord.gg/xYGf3fQnES) team.
- Keep updated on [EMQ YouTube](https://www.youtube.com/channel/UC5FjR77ErAxvZENEWzQaO5Q) by subscribing.

## Build From Source

**Install required dependencies**

[Install-dependencies](https://github.com/emqx/neuron/blob/main/Install-dependencies.md)

**Build**

```
$ git clone https://github.com/emqx/neuron
$ cd neuron
$ mkdir build && cd build
$ cmake .. && make
```

**Install Dashboard**

Download the latest `neuron-dashboard.zip` from the [neuron-dashboard](https://github.com/emqx/neuron-dashboard/releases) page, unzip it and put it to the `dist` directory under the Neuron executable directory.

**Run**

```
$ cd build
$ ./neuron
```

>**Log level**
>
>Change the log level in the rules in the zlog.conf file. Available levels include INFO, DEBUG, NOTICE, WARN, ERROR and FATAL.
>

## Test

To run all unit testers

```shell
$ cd build
$ ctest --output-on-failure
```

### Functional test

To run all functional testers

```shell
$ sudo apt-get install -y mosquitto
$ mosquitto -v &
$ python3 -m pip install -U pip
$ python3 -m pip install -r ft/requirements.txt
$ python3 -m robot --maxerroelines=600 -P ft/ -d ft/reports ft
```

### Pressure test

There are datasets for pressure testing in directory `ft/data/persistence/`.

To run pressure tests

```shell
# python dependencies
$ pip3 install -r ft/requirements.txt

# through http api, on dataset total-10k
$ python3 -m robot -P ft/ --variable neuron_api:http --variable dataset:total-10k -d ft/http-total-10k ft/pressure.test
# through http api, on dataset total-50k
$ python3 -m robot -P ft/ --variable neuron_api:http --variable dataset:total-50k -d ft/http-total-50k ft/pressure.test

# A MQTT broker is needed if using the mqtt api, mosquitto in this example
$ mosquitto -v &

# through mqtt api, on dataset simple-1k
$ python3 -m robot -P ft/ --variable neuron_api:mqtt --variable dataset:simple-1k -d ft/mqtt-simple-1k ft/pressure.test
```

## License

See [LICENSE](./LICENSE).
