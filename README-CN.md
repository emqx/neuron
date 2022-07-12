# Neuron

[![GitHub Release](https://img.shields.io/github/release/neugates/neuron?color=brightgreen&label=Release)](https://github.com/neugates/neuron/releases)
[![Slack](https://img.shields.io/badge/Slack-Neuron-39AE85?logo=slack)](https://slack-invite.emqx.io/)
[![Discord](https://img.shields.io/discord/931086341838622751?label=Discord&logo=discord)](https://discord.gg/xYGf3fQnES)
[![Twitter](https://img.shields.io/badge/Follow-EMQ-1DA1F2?logo=twitter)](https://twitter.com/EMQTech)
[![YouTube](https://img.shields.io/badge/Subscribe-EMQ-FF0000?logo=youtube)](https://www.youtube.com/channel/UC5FjR77ErAxvZENEWzQaO5Q)

[English](https://github.com/neugates/neuron/blob/main/README.md) | 简体中文

Neuron 是一个工业物联网（IIoT）边缘工业协议网关软件，用于现代大数据技术，以发挥工业 4.0 的力量。它支持对多种工业协议的一站式访问，并将其转换为标准 MQTT 协议以访问工业物联网平台。

Neuron 2.0 版本对 1.x 版本中的部分非必要功能进行了精简，重点聚焦在工业协议的数据采集和转发功能上，以期为工业物联网平台建设提供更加高效灵活的一站式协议接入与管理。

## 产品目标和功能

Neuron 的设计目标是专注于工业物联网的数据采集、转发和汇聚：通过将来自繁杂多样工业设备的、不同协议类型的数据转换为统一标准的物联网 MQTT 消息，从而实现这些独立设备的互联互通，更好地融入工业物联网的大系统之中，进行远程的直接控制和信息获取。

我们希望 Neuron 既可以在低端嵌入式 Linux 设备中运行，也可以运行在有较大内存的 Linux 主机中，以支持大规模的连接设备和海量的数据点位。因此 Neuron 的设计需要能够有极少的内存占用和极低的 CPU 占用，同时具有良好的可伸缩性，可满足不同运行资源条件下的需求。

Neuron 的主要功能和特性如下：

- 插件化的南向设备驱动和北向应用；
- 内置有轻量级的 Web Server，可以通过浏览器来配置和控制、监听设备；
- 可替换的 MQTT 客户端，可以通过 MQTT 接口来控制和读写设备；
- 支持同时连接大量不同协议的工业设备；
- 和 EMQ 的其他产品高度融合，包括 EMQ X、NanoMQ、eKuiper（由 EMQ 发起，现归属于 LF Edge 基金会维护运营） 等；
- 支持 Neuron 运行期更新设备驱动。

功能的完整列表，请参阅[应用和驱动使用介绍](https://neugates.io/plugins)。

获取更多信息，请访问[官网](https://neugates.io)。

## 架构设计

现代的 CPU 大都已经是多核心的了，即便是用在嵌入式系统中比较低端的 ARM 和 Risc-V 架构的 CPU，也都拥有多核心的芯片。因此我们需要能够充分利用这些多核心 CPU ，也就是说，Neuron 需要具有非常好的多核多线程性能。为此，我们使用了 NNG 这个基础消息库，这是一个多线程的 IO 处理和消息处理的异步并发库，能够充分利用 CPU 的多个核心。

我们采用了星型总线这一组织形式：有一个消息路由中心，该消息路由基于 NNG 提供高效率的消息转发。消息的发送和接收是线程间通讯，使用共享 Buffer 和智能指针，没有任何的内存拷贝，具有非常高的效率。围绕着这个路由中心，是各个具体功能的节点，由 Adapter 和 Plugin 组成。这些节点既可以是内置固有的，如轻量级的 Web Server；也可以是动态增加的，如各种设备驱动、MQTT 客户端、eKuiper 接口等。这样的设计隔离了各个设备驱动和北向应用的功能模块之间的耦合，另外采用订阅-发布机制来实现数据流的分发和聚合，使得 Neuron 有着极大的灵活性。用户可以根据现场的工作情况动态增加和减少设备驱动的节点，具有良好的可配置性。另外，当运行 Neuron 的硬件 CPU 性能好、核心多、内存大时，Neuron 能支持更多的设备驱动节点、海量的数据点位、更大的数据吞吐量、更低的响应时间，具有良好的系统可伸缩性。

Neuron 采用了插件机制来支持变化多端的用户功能需求，用户可以根据应用场景的功能需求动态的加载不同功能的插件，得到有不同功能的节点。当设备驱动有 Bug 需要修复，或者有新的功能需要升级时，可以通过动态更新插件来解决问题，升级新功能。每个节点的运行是独立的，当升级一个节点的插件时，不会影响其他节点的运行，Neuron 也不需要重新启动。

Neuron的总体架构概览如下图所示：

![arch-overview](docs/pictures/neuron-arch-overview.png)

Neuron的总线拓扑结构和数据流的分散/汇聚如下图所示：

![neuron-bus-topo](docs/pictures/neuron-bus-topo.png)

![neuron-dataflow](docs/pictures/neuron-dataflow.png)

Neuron的层次结构图如下图所示：

![neuron-layers](docs/pictures/neuron-layers.png)


## 安装

### 安装依赖

[Install Required Dependencies](https://github.com/neugates/neuron/blob/main/Install-dependencies.md)

### 编译

```shell
$ git clone https://github.com/emqx/neuron
$ cd neuron
$ git submodule update --init
$ mkdir build && cd build
$ cmake .. && make
```

## 前端
[下载前端包](https://github.com/emqx/neuron-dashboard/releases)。
下载后的前端包，解压放到Neuron可执行目录下的dist目录中。

## 快速开始

```shell
$ cd build
$ ./neuron
```

## 单元测试

运行所有单元测试

```shell
$ cd build
$ ctest --output-on-failure
```

## 功能测试

运行所有功能测试

```shell
mosquitto -v &
pip3 install -r ft/requirements.txt
python3 -m robot -P ft/ --variable neuron_api:http -d ft/http_reports ft

```

## 压力测试

目录`ft/data/persistence/`下存放有用于压力测试的数据集。

可运行以下命令进行压力测试

```shell
# 安装依赖
pip3 install -r ft/requirements.txt

# 使用http接口在数据集total-10k上进行测试
python3 -m robot -P ft/ --variable neuron_api:http --variable dataset:total-10k -d ft/http-total-10k ft/pressure.test
# 使用http接口在数据集total-50k上进行测试
python3 -m robot -P ft/ --variable neuron_api:http --variable dataset:total-50k -d ft/http-total-50k ft/pressure.test

# 如果使用mqtt接口则需要MQTT服务器，此例为mosquitto
mosquitto -v &

# 使用mqtt接口在数据集simple-1k上进行测试
python3 -m robot -P ft/ --variable neuron_api:mqtt --variable dataset:simple-1k -d ft/mqtt-simple-1k ft/pressure.test

```

## 社区

您可通过以下途径与 Neuron 社区及开发者联系。

- 论坛：https://askemq.com/c/neuron/8 
- GitHub Discussions：https://github.com/emqx/neuron/discussions
- Slack：https://slack-invite.emqx.io/
- Discord：https://discord.gg/xYGf3fQnES 

