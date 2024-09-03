# Neuron

[![GitHub Release](https://img.shields.io/github/release/emqx/neuron?color=brightgreen&label=Release)](https://github.com/emqx/neuron/releases)
[![Docker Pulls](https://img.shields.io/docker/pulls/emqx/neuron?label=Docker%20Pulls)](https://hub.docker.com/r/emqx/neuron)
[![codecov](https://codecov.io/gh/emqx/neuron/graph/badge.svg?token=X95RD0NUT0)](https://codecov.io/gh/emqx/neuron)
[![Slack](https://img.shields.io/badge/Slack-Neuron-39AE85?logo=slack)](https://slack-invite.emqx.io/)
[![Discord](https://img.shields.io/discord/931086341838622751?label=Discord&logo=discord)](https://discord.gg/xYGf3fQnES)
[![Twitter](https://img.shields.io/badge/Follow-EMQ-1DA1F2?logo=twitter)](https://twitter.com/EMQTech)
[![YouTube](https://img.shields.io/badge/Subscribe-EMQ-FF0000?logo=youtube)](https://www.youtube.com/channel/UC5FjR77ErAxvZENEWzQaO5Q)

Neuron is an Industrial IoT (IIoT) connectivity server for modern big data and AI/ML technology to leverage the power of Industrial 4.0. It supports one-stop access to dozens of industrial protocols and converts them into MQTT protocol and realize the interconnection between IIoT platforms and various industrial devices.

![neuron-overview](docs/pictures/neuron-final.png)

The following are some important features of Neuron:

- Edge native application with real-time capability to leverage the low latency network of edge side.
- Loosely-coupled modularity [architecture design](https://neugates.io/docs/en/latest/introduction/architecture/architecture.html) for extending more functional services by pluggable modules.
- Support hot plugins that can update device and application modules during runtime.
- Support numerous protocols for industrial devices, including Modbus, OPCUA, Ethernet/IP, IEC104, BACnet and [more](https://neugates.io/docs/en/latest/configuration/south-devices/south-devices.html).
- Support simultaneous connection of a large number of industrial devices with different protocols.
- Combine with the rule engine function provided by [eKuiper](https://www.lfedge.org/projects/ekuiper) to quickly implement rule-based device control or AI/ML analytics.
- Support data access to industrial applications, such as MES or ERP, SCADA, historian and data analytics software via [SparkplugB](https://neugates.io/docs/en/latest/use-cases/sparkplug/sparkplug.html) solution.
- Has very low memory footprint, less than 10M, and CPU usage, can run on limited resource hardware like ARM, x86 and RISC-V.
- Support installation of native executable or deployed in containerized enviornment.
- Control industrial devices, and make changes to the parameters and data tags through [HTTP API](https://neugates.io/docs/en/latest/api/configuration.html) and [MQTT API](https://neugates.io/docs/en/latest/configuration/north-apps/mqtt/api.html) services.
- Highly integrated with other EMQ products, including [EMQX](https://www.emqx.com/en/products/emqx), [NanoMQ](https://nanomq.io/), [eKuiper](https://ekuiper.org/).
- The code of the core framework and Modbus, MQTT and eKuiper are licensed under open source LGPLv3. Commercial modules require a [EMQ License](https://neugates.io/docs/en/latest/installation/license-install/license-install.html) to run.

For more information, please visit [Neuron homepage](https://neugates.io/).

## Get Started

#### Run Neuron using Docker

```
docker run -d --name neuron -p 7000:7000 -p 7001:7001 -p 9081:9081 --privileged=true --restart=always emqx/neuron:latest
```

Next, please follow the [getting started guide](https://neugates.io/docs/en/latest/quick-start/hardware-specifications.html) to tour the Neuron features.

> **Limitations**
>
> Neuron open source edition only includes Modbus and MQTT drivers.
>
> If you need more driver protocol support and edge computing services, you can choose Neuron's commercial version [NeuronEX](https://www.emqx.com/en/products/neuronex).
> NeuronEX comes with a built-in license for 30 data tags that can be applied to all southbound drivers. And NeuronEX also supports edge intelligent analysis of industrial data to enable you to quickly gain business insights. To download NeuronEX, please click [here](https://www.emqx.com/en/downloads-and-install/neuronex).

#### More installation options

If you prefer to install and manage Neuron yourself, you can download the latest version from [neugates.io/downloads?os=Linux](https://neugates.io/downloads?os=Linux).

For more installation options, see the [Neuron installation documentation](https://neugates.io/docs/en/latest/installation/installation.html).

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

## License

See [LICENSE](./LICENSE).
