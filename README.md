# Neuron

[![GitHub Release](https://img.shields.io/github/release/emqx/neuron?color=brightgreen&label=Release)](https://github.com/emqx/neuron/releases)
[![Docker Pulls](https://img.shields.io/docker/pulls/emqx/neuron?label=Docker%20Pulls)](https://hub.docker.com/r/emqx/neuron)
[![codecov](https://codecov.io/gh/emqx/neuron/graph/badge.svg?token=X95RD0NUT0)](https://codecov.io/gh/emqx/neuron)
[![Slack](https://img.shields.io/badge/Slack-Neuron-39AE85?logo=slack)](https://slack-invite.emqx.io/)
[![Discord](https://img.shields.io/discord/931086341838622751?label=Discord&logo=discord)](https://discord.gg/xYGf3fQnES)
[![Twitter](https://img.shields.io/badge/Follow-EMQ-1DA1F2?logo=twitter)](https://twitter.com/EMQTech)
[![YouTube](https://img.shields.io/badge/Subscribe-EMQ-FF0000?logo=youtube)](https://www.youtube.com/channel/UC5FjR77ErAxvZENEWzQaO5Q)

Neuron is an Industrial IoT (IIoT) connectivity server that bridges industrial devices and modern data platforms. It supports dozens of industrial protocols and converts them into MQTT for seamless integration between IIoT platforms and shop-floor devices.

## About NeuronEX

[NeuronEX](https://www.emqx.com/en/products/emqx-neuron) is the commercial distribution of Neuron, offering extended capabilities and professional support. Compared to the open-source Neuron:

- **Extended protocol support**: Additional industrial protocols (e.g., OPC UA, Siemens S7, Ethernet/IP, etc.)
- **Advanced dashboard**: Full-featured web UI with enhanced visualization and management capabilities
- **Enterprise plugins**: More southbound device adapters, northbound application connectors, and rule engines
- **Cloud integration**: Direct connectors for major cloud platforms (AWS, Azure, Google Cloud, etc.)
- **Data security**: Enhanced encryption, auditing, and compliance features
- **Professional support**: 24/7 technical support and regular updates

For more details, visit the [NeuronEX documentation](https://docs.emqx.com/en/neuronex/latest/).

![neuron-overview](docs/pictures/neuron-final.png)

Key features:

- Edge-native application with real-time capability and low latency at the edge.
- Loosely coupled modular architecture with pluggable modules for easy extension.
- Hot-pluggable plugins: update device and application modules at runtime.
- Broad protocol support: Modbus, OPC UA, Ethernet/IP, IEC 60870-5-104, BACnet, and more.
- High concurrency: connect many devices with heterogeneous protocols simultaneously.
- Built-in stream processing via [eKuiper](https://www.lfedge.org/projects/ekuiper) for rules and AI/ML analytics.
- Northbound access for MES/ERP, SCADA, historians, and analytics via Sparkplug B.
- Tiny footprint (<10 MB) and low CPU usage; runs on ARM, x86, and RISC-V.
- Flexible deployments: native binaries or containers.
- Manage devices and tags via [HTTP API](docs/api/english/http.md) and [MQTT API](docs/api/english/mqtt.md).
- Works well with [EMQX](https://www.emqx.com/en/products/emqx), [NanoMQ](https://nanomq.io/), and [eKuiper](https://ekuiper.org/).
- Core framework and Modbus/MQTT/eKuiper plugins are available under LGPLv3.

Note: The open-source edition provides a subset of plugins. For more plugins and a full-featured Dashboard, see [NeuronEX](https://www.emqx.com/en/products/neuronex).

## Table of Contents

- [Quick Start](#quick-start)
- [Installation](#installation)
	- [Binaries](#binaries)
	- [Build from Source](#build-from-source)
- [Configuration](#configuration)
- [Documentation](#documentation)
- [Dashboard](#dashboard)
- [Community](#community)
- [Contributing](#contributing)
- [Security](#security)
- [License](#license)

## Quick Start

Default credentials: username `admin`, password `0000`.

### Download

You can download from [Release](https://github.com/emqx/neuron/releases).

```bash
$ tar xvf neuron-{version}-linux-amd64.tar.gz
$ cd neuron
$ ./neuron --log
```

Open a browser and navigate to `http://localhost:7000` to access the Neuron web interface.

## Installation

### Binaries

Download the latest release from [Releases](https://github.com/emqx/neuron/releases), then extract and run as shown above.

### Build from Source

1. [Install Dependencies](./Install-dependencies.md)

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

5. Open a browser and navigate to `http://localhost:7000` to access the Neuron web interface.

## Configuration

- Web UI: `http://localhost:7000` (default credentials above)
- Main config: `neuron.conf`
- Default plugins: `default_plugins.json`
- Logging: `zlog.conf` / `sdk-zlog.conf`

See the quick start for a hands-on walkthrough.

## Documentation

- Quick start: [Modbus TCP collection and MQTT publishing](./docs/quick_start/quick_start.md)
- APIs: [HTTP](docs/api/english/http.md), [MQTT](docs/api/english/mqtt.md)

## Dashboard

The open-source version of the [Dashboard](https://github.com/emqx/neuron-dashboard) is currently at version 2.6.3, which has been suspended for development and maintenance. This version is also the one integrated by default with Neuron. For a more complete and professional Dashboard, please use [NeuronEX](https://www.emqx.com/en/products/neuronex).

## Community

- Follow [@EMQTech on Twitter](https://twitter.com/EMQTech).
- If you have a specific question, check out our [discussion forums](https://github.com/emqx/neuron/discussions).
- For general discussions, join us on the [official Discord](https://discord.gg/xYGf3fQnES) team.
- Keep updated on [EMQ YouTube](https://www.youtube.com/channel/UC5FjR77ErAxvZENEWzQaO5Q) by subscribing.

## Contributing

Contributions are welcome! Feel free to open issues and pull requests to improve Neuron. If you plan a larger change, please start a discussion first to align on direction.

## Security

If you believe you have found a security vulnerability, please avoid creating a public issue. Instead, contact the maintainers privately (e.g., via GitHub Security Advisories) so we can investigate and fix it responsibly.


## License

See [LICENSE](./LICENSE).
