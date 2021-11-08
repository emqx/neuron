# Neuron

English | [简体中文](https://github.com/neugates/neuron/blob/main/README-CN.md)

Neuron is a Industrial IoT (IIoT) edge industrial gateway for modern big data technology to leverage the power of Industrial 4.0. It supports one-stop access to dozens of industrial protocols and converts them into MQTT protocol to access the Industrial IoT platform.

Version 2.0 removes unnecessary features from version 1.x and focuses on data acquisition and forwarding of industrial protocols.Neuron 2.0 support simultaneous connection of a large number of devices with different protocols.

## Feature

- Plugged-in southbound driver and northbound application;
- Support simultaneous connection of a large number of devices with different protocols;
- Highly integrated with other EMQ products,including eKuiper, EMQX, Fabric;
- Support updating device drivers during Neuron runtime;

For full list of new features,plfaase read [Instructions for application and driver](https://github.com/neugates/nep/blob/main/docs/neuron2.x-driver.md)。

For more information,please visit [Homepage](https://www.emqx.com/zh/products/neuron)。

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
cmake .. && make
```

## Quick Start

```shell
# Start neuron
./neuron
```

## Test

To test everying in one go

```shell
ctest --output-on-failure
```

## Community
