# Neuron

English | [简体中文](https://github.com/neugates/neuron/blob/main/README-CN.md)

Neuron is a Industrial IoT (IIoT) edge industrial gateway for modern big data technology to leverage the power of Industrial 4.0. It supports one-stop access to dozens of industrial protocols and converts them into MQTT protocol to access the Industrial IoT platform.

Version 2.0 removes unnecessary features from version 1.x and focuses on data acquisition and forwarding of industrial protocols.Neuron 2.0 support simultaneous connection of a large number of devices with different protocols.

## Design and Features

The design goal of Neuron is to focus on data collection, forwarding and aggregation of the Industrial Internet of Things, and to support multiple industrial protocols. Convert the data of various industrial device into MQTT messages, integrate these independent device into the large industrial Internet of Things system through neuron, and the user can control industrial device and obtain information from it at the remote end. We hope that Neuron can run in low-end embedded linux devices, so it has very little memory footprint and very low CPU footprint. Otherwise, Neuron can also run on a Linux workstation with larger memory to support a large number of connected devices and a large number of data points, so it has good scalability.

Neuron has the following important features:

- Plugged-in southbound driver and northbound application;
- A light-weigh built-in web server, the user can configure, control and monitor device through browser.
- A replaceable MQTT Client, the user can control and read/write device by MQTT message.
- Support simultaneous connection of a large number of devices with different protocols;
- Highly integrated with other EMQ products,including eKuiper, EMQX, Fabric;
- Support updating device drivers during Neuron runtime;

For full list of new features,plfaase read [Instructions for application and driver](https://github.com/neugates/nep/blob/main/docs/neuron2.x-driver.md)。

For more information,please visit [Homepage](https://www.emqx.com/zh/products/neuron)。

## Architecture design

Most modern CPUs are already multi-core, even the lower-end ARM and Risc-V architecture CPUs used in embedded systems, all of which have multi-core chips. Therefore, we need to be able to make full use of these multi-core CPUs, that is, Neuron needs to have very good multi-core and multi-thread performance. We use library nng, which is an asynchronous concurrent library for multi-threaded IO processing and message passing, which can make full use of the multiple cores of the CPU.

We use the star bus mode as an organizational form, and there is a message routing center. This message routing is based on nng to provide a high-efficiency message forwarding. The sending and receiving of messages is communication between threads. By using shared buffers and smart pointers, there is no memory copy, which is very efficient. Around this routing center are nodes, every node include adapter and plugin. These nodes can be built-in with neuron, such as a lightweight web server, or they can be dynamically added, such as various device drivers, MQTT clients, eKuiper interface, and so on. By this design, it isolates the coupling between each device driver and northbound application. In addition, the subscription-publishing mechanism is used to realize the scattering and gathering of data streams, which makes Neuron extremely flexible. User can dynamically increase and decrease device-driven node according to the working load of the site, with good configurability. In addition, when the hardware CPU running Neuron has good performance, more cores, and large memory, Neuron can support more device driver nodes, massive data points, greater data throughput, and lower response time. Has good system scalability.

Neuron use a plugged-in mechanism to support changing user functional requirements. Users can dynamically load plug-ins with different functional according to the functional requirements of the application scenario. When the device driver has a bug fix and needs to be upgraded, you can also dynamically update the new plug-in to solve the problem and get the new feature. The running state of each node is independent. When the plug-in of one node is upgraded, it will not affect the runing state of other nodes, and Neuron does not need to be restarted.

An overview of Neuron's architecture is shown in the figure below:

![arch-overview](docs/pictures/neuron-arch-overview.png)

The topology of bus in Neuron and the scattering/gathering of data flow are shown in the figure below:

![neuron-bus-topo](docs/pictures/neuron-bus-topo.png)

![neuron-dataflow](docs/pictures/neuron-dataflow.png)

The hierarchical layer diagram of Neuron is shown in the figure below:

![neuron-layers](docs/pictures/neuron-layers.png)

## Installation

[Script to install dependent libraries](https://github.com/neugates/neuron/blob/main/install-libs.sh)

```shell
# Script to install all libraries
cd neuron 
./install-libs.sh
```

[Source code to install dependent libraries](https://github.com/neugates/neuron/blob/main/Install-dependent-libraries.md)

## Build

```shell
git clone https://github.com/neugates/neuron.git
# Get submodule
git submodule update --init
# build
cd neuron && mkdir build 
cd build
cmake .. && make
cd -
```

## Quick Start

```shell
# Start neuron
cd build
./neuron
cd -
```

## Test

To test everying in one go

```shell
cd build
ctest --output-on-failure
cd -
```

## Functional test

```shell
mosquitto -v &
pip3 install -r ft/requirements.txt
python3 -m robot -P ft/ --variable neuron_api:http -d ft/http_reports ft
python3 -m robot -P ft/ --variable neuron_api:mqtt -d ft/mqtt_reports ft

```

## Community
