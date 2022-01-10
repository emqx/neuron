# Neuron

[English](https://github.com/neugates/neuron/blob/main/README.md) | 简体中文

Neuron是一个工业物联网（IIoT）边缘工业网关，用于现代大数据技术，以发挥工业4.0的力量。它支持对多种工业协议的一站式访问，并将其转换为MQTT协议以访问工业物联网平台。

2.0版本删除1.x版本不必要的功能，聚焦在工业协议的数据采集和转发的功能上。Neuron 2.0支持同时采集多台设备的数据

## 设计目标和功能

Neuron的设计目标是专注于工业物联网的数据采集、转发和汇聚，支持多种工业协议。将各种不同的工业设备的数据转换为MQTT消息，将这些独立的设备通过neuron融入到工业物联网的大系统中，可以在远端直接控制和获取信息。我们希望Neuron可以在低端嵌入式linux设备中运行，因此其有极少的内存占用，极低的CPU占用。同时Neuron也可以运行在有较大内存的linux主机中，以支持大量的连接设备和海量的数据点位，因此其具有良好的可伸缩性。

Neuron有如下主要的功能和特性：

- 插件化的南向设备驱动和北向应用；
- 内置有轻量级的web server，可以通过浏览器来配置和控制、监听设备；
- 可替换的MQTT客户端，可以通过MQTT接口来控制和读写设备；
- 支持同时连接大量不同协议的工业设备；
- 和EMQ的其他产品高度融合，包括eKuiper、EMQX、Fabric、NanoMQ等；
- 支持Neuron运行期更新设备驱动；

功能的完整列表，请参阅[应用和驱动使用介绍](https://github.com/neugates/nep/blob/main/docs/neuron2.x-driver.md)。

获取更多信息，请访问[官网](https://www.emqx.com/zh/products/neuron)。

## 架构设计

现代的CPU的大都已经是多核心的了，包括用在嵌入式系统中的比较低端的ARM和Risc-V架构的CPU，都有多核心的芯片。因此我们需要能够充分的利用这些多核心的CPU，也即Neuron需要能够具有非常好的多核多线程性能。为此，我们使用了nng这个基础消息库，这是一个多线程的IO处理和消息处理的异步并发库，能够充分利用CPU的多个核心 。

我们采用了星型总线这一组织形式，有一个消息路由中心，这个消息路由以nng为基础来提供一个高效率的消息转发。消息的发送和接收是线程间通讯，通过使用共享buffer和智能指针，没有任何的内存拷贝，具有非常高的效率。围绕着这个路由中心，是各个具体功能的节点，由adapter和plugin组成。这些节点可以是内置固有的，比如轻量级的web server，也可以是动态增加的，比如各种设备驱动、MQTT客户端、eKuiper接口等。这样的设计隔离了各个设备驱动和北向应用的功能模块之间的耦合，另外采用订阅-发布机制来实现数据流的分发和聚合，使得Neuron有着极大的灵活性。用户可以根据现场的工作情况动态的增加和减少设备驱动的节点，具有良好的可配置性。另外，当运行Neuron的硬件CPU性能好，核心多，内存大时，Neuron能支持更多的设备驱动节点，海量的数据点位，更大的数据吞吐量，更低的响应时间。具有良好的系统可伸缩性。

Neuron采用了插件机制来支持变化多端的用户功能需求，用户可以根据应用场景的功能需求动态的加载不同功能的插件，得到有不同功能的节点。当设备驱动有bug修复，需要升级时，也可以动态的更新新插件来解决问题，升级新功能。每个节点的运行是独立的，当升级一个节点的插件时，不会影响其他节点的运行，Neuron也不需要重新启动。

Neuron的总体架构概览如下图所示：

![arch-overview](docs/pictures/neuron-arch-overview.png)

Neuron的总线拓扑结构和数据流的分散/汇聚如下图所示：

![neuron-bus-topo](docs/pictures/neuron-bus-topo.png)

![neuron-dataflow](docs/pictures/neuron-dataflow.png)

Neuron的层次结构图如下图所示：

![neuron-layers](docs/pictures/neuron-layers.png)


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
cd build
cmake .. && make
cd -
```

## 快速开始

```shell
# Start neuron
cd build
./neuron
cd -
```

## 测试

一次性测试所有内容

```shell
cd build
ctest --output-on-failure
cd -
```

## 功能测试

```shell
mosquitto -v &
pip3 install -r ft/requirements.txt
python3 -m robot -P ft/ --variable neuron_api:http -d ft/http_reports ft
python3 -m robot -P ft/ --variable neuron_api:mqtt -d ft/mqtt_reports ft

```



## 社区
