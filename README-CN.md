# Neuron

[![GitHub Release](https://img.shields.io/github/release/neugates/neuron?color=brightgreen&label=Release)](https://github.com/neugates/neuron/releases)
[![Docker Pulls](https://img.shields.io/docker/pulls/emqx/neuron?label=Docker%20Pulls)](https://hub.docker.com/r/emqx/neuron)
[![Slack](https://img.shields.io/badge/Slack-Neuron-39AE85?logo=slack)](https://slack-invite.emqx.io/)
[![Discord](https://img.shields.io/discord/931086341838622751?label=Discord&logo=discord)](https://discord.gg/xYGf3fQnES)
[![Twitter](https://img.shields.io/badge/Follow-EMQ-1DA1F2?logo=twitter)](https://twitter.com/EMQTech)
[![YouTube](https://img.shields.io/badge/Subscribe-EMQ-FF0000?logo=youtube)](https://www.youtube.com/channel/UC5FjR77ErAxvZENEWzQaO5Q)



[English](https://github.com/neugates/neuron/blob/main/README.md) | 简体中文

Neuron 是一款开源的、轻量级工业协议网关软件，支持数十种工业协议的一站式设备连接、数据接入、MQTT 协议转换，为工业设备赋予工业 4.0 时代关键的物联网连接能力。

![neuron-overview](docs/pictures/neuron-final.png)

以下是 Neuron 的一些重要特性：

- 具有实时能力的边缘原生应用程序可以利用边缘端的低延迟网络。
- 松耦合模块化 [架构设计](https://neugates.io/docs/zh/latest/introduction/architecture/architecture.html) 通过可插拔模块扩展更多功能服务。
- 支持可以在运行时更新设备和应用程序模块的热插件。
- 支持多种工业设备协议，包括 Modbus、OPCUA、Ethernet/IP、IEC104、BACnet 等 [更多协议](https://neugates.io/docs/zh/latest/configuration/south-devices/south-devices.html)。
- 支持同时连接大量不同协议的工业设备。
- 结合[eKuiper](https://www.lfedge.org/projects/ekuiper)提供的规则引擎功能，快速实现基于规则的设备控制或 AI/ML 分析。
- 通过 [SparkplugB](https://neugates.io/docs/zh/latest/use-cases/use_cases.html#mqtt-sparkplugb-%E8%A7%A3%E5%86%B3%E6%96%B9%E6%A1%88) 解决方案支持对工业应用程序的数据访问，例如 MES 或 ERP、SCADA、historian 和数据分析软件。
- 具有非常低的内存占用，小于 10M 的内存占用和 CPU 使用率，可以在 ARM、x86 和 RISC-V 等资源有限的硬件上运行。
- 支持在本地安装可执行文件或部署在容器化环境中。
- 控制工业设备，通过[HTTP API](https://neugates.io/docs/zh/latest/http-api/http-api.html)和[MQTT API](https://neugates.io/docs/en/latest/north-apps/mqtt/api.html) 服务更改参数和数据标签。
- 与其他 EMQ 产品高度集成，包括  [EMQX](https://www.emqx.com/zh/products/emqx)、[NanoMQ](https://nanomq.io/zh)、[eKuiper](https://ekuiper.org/zh)。
- 核心框架和 Modbus、MQTT 和 eKuiper 的代码在 LGPLv3 的许可下开源。商业模块需要 [EMQ 许可证](https://neugates.io/docs/zh/latest/introduction/license-describe.html) 才能运行。

获取更多信息，请访问 [Neuron 官网](https://neugates.io/zh)。

## 快速开始

#### 使用 Docker 运行 Neuron

```
docker run -d --name neuron -p 7000:7000 -p 7001:7001 -p 9081:9081 --privileged=true --restart=always emqx/neuron:latest
```

接下来请参考 [快速入门](https://neugates.io/docs/zh/latest/quick-start/hardware-specifications.html) 开启您的 Neuron 之旅。

> **提示**
>
> Neuron 开源版只包含 Modbus 和 MQTT 驱动。
>
> 您可以[免费申请 15 天试用期限的 License](https://www.emqx.com/zh/apply-licenses/neuron)，加载所有的驱动。

#### 更多安装方式

您可以从 [neugates.io/zh/downloads?os=Linux](https://neugates.io/zh/downloads?os=Linux) 下载不同格式的 Neuron 安装包进行手动安装。

也可以直接访问 [Neuron 安装文档](https://neugates.io/docs/zh/latest/installation/installation.html) 查看不同安装方式的操作步骤。

## 文档

Neuron 文档地址：[https://neugates.io/docs/zh/latest/](https://neugates.io/docs/zh/latest/)。

## 社区

- 访问 [Neuron 论坛](https://askemq.com/c/neuron/8) 以获取帮助，也可以分享您的想法或项目。
- 添加小助手微信号 `emqmkt`，加入 Neuron 微信技术交流群。
- 加入我们的 [Discord](https://discord.gg/xYGf3fQnES)，参于实时讨论。
- 关注我们的 [bilibili](https://space.bilibili.com/522222081)，获取最新物联网技术分享。
- 关注我们的 [微博](https://weibo.com/emqtt) 或 [Twitter](https://twitter.com/EMQTech)，获取 Neuron 最新资讯。

## 从源码构建

**安装依赖**

请参考 [Install-dependencies](https://github.com/emqx/neuron/blob/main/Install-dependencies.md)

**编译**

```
$ git clone https://github.com/emqx/neuron
$ cd neuron
$ mkdir build && cd build
$ cmake .. && make
```

**安装 Dashboard**

在 [neuron-dashboard](https://github.com/emqx/neuron-dashboard/releases) 页面下载最新的 `neuron-dashboard.zip`，解压后放到 Neuron 可执行目录下的 dist 目录中。

**运行**

```
$ cd build
$ ./neuron
```

>**修改日志级别**
>
>在 zlog.conf 文件的规则中更改日志级别。可用的日志级别包括 INFO，DEBUG，NOTICE，WARN，ERROR 和 FATAL。

## 测试

### 单元测试

运行所有的单元测试

```shell
$ cd build
$ ctest --output-on-failure
```

### 功能测试

运行所有的功能测试

```shell
$ sudo apt-get install -y mosquitto
$ mosquitto -v &
$ python3 -m pip install -U pip
$ python3 -m pip install -r ft/requirements.txt
$ python3 -m robot --maxerroelines=600 -P ft/ -d ft/reports ft
```

### 压力测试

在目录`ft/data/persistence/`中有压力测试的数据集。

运行压力测试

```shell
# 安装依赖
$ pip3 install -r ft/requirements.txt

# 使用http接口在数据集total-10k上进行测试
$ python3 -m robot -P ft/ --variable neuron_api:http --variable dataset:total-10k -d ft/http-total-10k ft/pressure.test
# 使用http接口在数据集total-50k上进行测试
$ python3 -m robot -P ft/ --variable neuron_api:http --variable dataset:total-50k -d ft/http-total-50k ft/pressure.test

# 如果使用mqtt接口则需要MQTT服务器，此例为mosquitto
$ mosquitto -v &

# 使用mqtt接口在数据集simple-1k上进行测试
$ python3 -m robot -P ft/ --variable neuron_api:mqtt --variable dataset:simple-1k -d ft/mqtt-simple-1k ft/pressure.test
```

## 开源许可

详见 [LICENSE](./LICENSE)。
