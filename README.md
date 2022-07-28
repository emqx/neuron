# Neuron

[![GitHub Release](https://img.shields.io/github/release/neugates/neuron?color=brightgreen&label=Release)](https://github.com/neugates/neuron/releases)
[![Slack](https://img.shields.io/badge/Slack-Neuron-39AE85?logo=slack)](https://slack-invite.emqx.io/)
[![Discord](https://img.shields.io/discord/931086341838622751?label=Discord&logo=discord)](https://discord.gg/xYGf3fQnES)
[![Twitter](https://img.shields.io/badge/Follow-EMQ-1DA1F2?logo=twitter)](https://twitter.com/EMQTech)
[![YouTube](https://img.shields.io/badge/Subscribe-EMQ-FF0000?logo=youtube)](https://www.youtube.com/channel/UC5FjR77ErAxvZENEWzQaO5Q)

English | [简体中文](https://github.com/neugates/neuron/blob/main/README-CN.md)

Neuron is an Industrial IoT (IIoT) connectivity server for modern big data technology to leverage the power of Industrial 4.0. It supports one-stop access to dozens of industrial protocols and converts them into MQTT protocol to access the IIoT platform.

![neuron-overview](docs/pictures/neuron-final.png)

The following is some important features of Neuron:

- Edge native application to leverage the low latency network of edge side.
- Loosely-coupled modularity design for extending more functional services by pluggable modules.
- Support numerous protocols for industrial devices, including Modbus, OPCUA, Ethernet/IP, IEC104, BACnet and [more](https://neugates.io/docs/en/latest/module-plugins/module-list.html).
- Support simultaneous connection of a large number of industrial devices with different protocols.
- Combine with the rule engine function provided by [eKuiper](https://www.lfedge.org/projects/ekuiper) to quickly implement rule-based device control or AI/ML analytics.
- Has very low memory footprint, less than 10M, and CPU usage, can run on limited resource hardware like ARM, x86 and RISC-V.
- Support installation of native executable or deployed in containerized enviornment.
- Control industrial devices or make changes to the parameters or data tags through API and MQTT services.
- Highly integrated with other EMQ products, including EMQX, NanoMQ, eKuiper.
- Support updating module device drivers during Neuron runtime.

For more information, please visit our [Homepage](https://neugates.io).

## Prerequisite

[Install Required Dependencies](https://github.com/neugates/neuron/blob/main/Install-dependencies.md)

## Build

```shell
$ git clone https://github.com/emqx/neuron
$ cd neuron
$ git submodule update --init
$ mkdir build && cd build
$ cmake .. && make
```

## Quick Start

```shell
$ cd build
$ ./neuron
```

## Test

To run all unit testers

```shell
$ cd build
$ ctest --output-on-failure
```

## Functional test

To run all functional testers

```shell
mosquitto -v &
pip3 install -r ft/requirements.txt
python3 -m robot -P ft/ --variable neuron_api:http -d ft/http_reports ft

```

## Pressure test

There are datasets for pressure testing in directory `ft/data/persistence/`.

To run pressure tests

```shell
# python dependencies
pip3 install -r ft/requirements.txt

# through http api, on dataset total-10k
python3 -m robot -P ft/ --variable neuron_api:http --variable dataset:total-10k -d ft/http-total-10k ft/pressure.test
# through http api, on dataset total-50k
python3 -m robot -P ft/ --variable neuron_api:http --variable dataset:total-50k -d ft/http-total-50k ft/pressure.test

# A MQTT broker is needed if using the mqtt api, mosquitto in this example
mosquitto -v &

# through mqtt api, on dataset simple-1k
python3 -m robot -P ft/ --variable neuron_api:mqtt --variable dataset:simple-1k -d ft/mqtt-simple-1k ft/pressure.test

```

## Community

You can connect with the Neuron community and developers in the following ways.

- Discussions：https://github.com/emqx/neuron/discussions
- Slack：https://slack-invite.emqx.io/
- Discord：https://discord.gg/xYGf3fQnES 
