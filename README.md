# Neuron

[![GitHub Release](https://img.shields.io/github/release/emqx/neuron?color=brightgreen&label=Release)](https://github.com/emqx/neuron/releases)
[![Docker Pulls](https://img.shields.io/docker/pulls/emqx/neuron?label=Docker%20Pulls)](https://hub.docker.com/r/emqx/neuron)
[![codecov](https://codecov.io/gh/emqx/neuron/graph/badge.svg?token=X95RD0NUT0)](https://codecov.io/gh/emqx/neuron)
[![Slack](https://img.shields.io/badge/Slack-Neuron-39AE85?logo=slack)](https://slack-invite.emqx.io/)
[![Discord](https://img.shields.io/discord/931086341838622751?label=Discord&logo=discord)](https://discord.gg/xYGf3fQnES)
[![Twitter](https://img.shields.io/badge/Follow-EMQ-1DA1F2?logo=twitter)](https://twitter.com/EMQTech)
[![YouTube](https://img.shields.io/badge/Subscribe-EMQ-FF0000?logo=youtube)](https://www.youtube.com/channel/UC5FjR77ErAxvZENEWzQaO5Q)

English | [简体中文](https://github.com/emqx/neuron/blob/main/README-CN.md)


Neuron is an Industrial IoT (IIoT) connectivity server for modern big data and AI/ML technology to leverage the power of Industrial 4.0. It supports one-stop access to dozens of industrial protocols and converts them into MQTT protocol and realize the interconnection between IIoT platforms and various industrial devices.

![neuron-overview](docs/pictures/neuron-final.png)

The following are some important features of Neuron:

- Edge native application with real-time capability to leverage the low latency network of edge side.
- Loosely-coupled modularity architecture design for extending more functional services by pluggable modules.
- Support hot plugins that can update device and application modules during runtime.
- Support numerous protocols for industrial devices, including Modbus, OPCUA, Ethernet/IP, IEC104, BACnet and more.
- Support simultaneous connection of a large number of industrial devices with different protocols.
- Combine with the rule engine function provided by [eKuiper](https://www.lfedge.org/projects/ekuiper) to quickly implement rule-based device control or AI/ML analytics.
- Support data access to industrial applications, such as MES or ERP, SCADA, historian and data analytics software via SparkplugB solution.
- Has very low memory footprint, less than 10M, and CPU usage, can run on limited resource hardware like ARM, x86 and RISC-V.
- Support installation of native executable or deployed in containerized enviornment.
- Control industrial devices, and make changes to the parameters and data tags through [HTTP API](docs/api/english/http.md) and [MQTT API](docs/api/english/mqtt.md) services.
- Highly integrated with other EMQ products, including [EMQX](https://www.emqx.com/en/products/emqx), [NanoMQ](https://nanomq.io/), [eKuiper](https://ekuiper.org/).
- The code of the core framework and Modbus, MQTT and eKuiper are licensed under open source LGPLv3.

Neuron only provides limited plugins in the open source version. To use more plugins and a more complete Dashboard, please use [NeuronEX](https://www.emqx.com/en/products/neuronex).

## Quick Start

Default username is `admin` and password is `0000`.

### Download

You can download from [Release](https://github.com/emqx/neuron/releases).

```bash
$ tar xvf neuron-{version}-linux-amd64.tar.gz
$ cd neuron
$ ./neuron --log
```

Open a web browser and navigate to `http://localhost:7000` to access the Neuron web interface.

### Build and Run

1. [Install Dependencies](https://github.com/emqx/neuron/install-dependencies.md)

2. Build Neuron
```bash
$ git clone https://github.com/emqx/neuron
$ cd neuron && mkdir build && cd build
$ cmake .. && make
```

3. Download and Unzip Dashboard
```bash
$ wget https://github.com/emqx/neuron-dashboard/releases/download/2.6.3/neuron-dashboard.zip

# Unzip neuron-dashboard.zip to the build directory
$ unzip neuron-dashboard.zip
```

4. Run Neuron
```bash
$ ./neuron --log
```

5. Open a web browser and navigate to `http://localhost:7000` to access the Neuron web interface.

### Docker

```bash
$ docker run -d --name neuron -p 7000:7000 emqx/neuron:2.6.9
```

Currently, the latest image maintained for Neuron is `emqx/neuron:2.6.9`, and images for versions `2.7.x` and later are no longer provided.

### [Modbus TCP Data Collection and MQTT Transmission](./docs/quick_start/quick_start.md)

## Dashboard

The open-source version of the [Dashboard](https://github.com/emqx/neuron-dashboard) is currently at version 2.6.3, which has been suspended for development and maintenance. This version is also the one integrated by default with Neuron. For a more complete and professional Dashboard, please use [NeuronEX](https://www.emqx.com/en/products/neuronex).

## Get Involved

- Follow [@EMQTech on Twitter](https://twitter.com/EMQTech).
- If you have a specific question, check out our [discussion forums](https://github.com/emqx/neuron/discussions).
- For general discussions, join us on the [official Discord](https://discord.gg/xYGf3fQnES) team.
- Keep updated on [EMQ YouTube](https://www.youtube.com/channel/UC5FjR77ErAxvZENEWzQaO5Q) by subscribing.


## License

See [LICENSE](./LICENSE).
