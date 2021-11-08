# Neuron

[English](https://github.com/neugates/neuron/blob/main/README.md) | 简体中文

Neuron是一个工业物联网（IIoT）边缘工业网关，用于现代大数据技术，以发挥工业4.0的力量。它支持对多种工业协议的一站式访问，并将其转换为MQTT协议以访问工业物联网平台。

2.0版本删除1.x版本不必要的功能，聚焦在工业协议的数据采集和转发的功能上。Neuron 2.0支持同时采集多台设备的数据

## 功能

- 插件化的南向设备驱动和北向应用；
- 支持同时连接大量不同协议的设备；
- 和EMQ的其他产品高度融合，包括eKuiper、EMQX、Fabric等；
- 支持Neuron运行期更新设备驱动；

功能的完整列表，请参阅[应用和驱动使用介绍](https://github.com/neugates/nep/blob/main/docs/neuron2.x-driver.md)。

获取更多信息，请访问[官网](https://www.emqx.com/zh/products/neuron)。

## 安装

[脚本安装依赖库](https://github.com/neugates/neuron/blob/main/install-libs.sh)

```shell
# Script to install all libraries
cd neuron 
./install-libs.sh
```

[源代码安装依赖库](https://github.com/lixiumei123/neuron/blob/main/Install-dependent-libraries.md)

## Build

```shell
git clone https://github.com/neugates/neuron.git
# Get submodule
git submodule update --init
# build
cd neuron && mkdir build 
cmake .. && make
```

## 快速开始

```shell
# Start neuron
./neuron
```

## 测试

一次性测试所有内容

```shell
cd build
ctest --output-on-failure
```

## 社区
