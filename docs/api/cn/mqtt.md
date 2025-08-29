# 数据上下行格式

以下内容描述 MQTT 插件如何上报采集的数据，以及如何通过 MQTT 插件实现读写点位数据。


## 数据上报

MQTT 插件将采集到的数据以 JSON 形式发布到指定的主题。
上报数据的具体格式由**上报数据格式**参数指定，有[**Values-format 格式**](#values-format-格式)、[**Tags-format 格式**](#tags-format-格式)、[**ECP-format 格式**](#ecp-format-格式)、[**Custom 自定义格式**](#custom-自定义格式)多种格式可选。

### 上报主题

上报主题在北向节点订阅中指定，其默认值为 **/neuron/{MQTT driver name}** ，见下图

### Values-format 格式

在 **Values-format** 格式中， 上报的数据由以下字段构成：
* `timestamp` : 数据采集时的 UNIX 时间戳。
* `node` : 被采集的南向节点的名字。
* `group` : 被采集的南向节点的点位组的名字。
* `values` : 存储采集成功的点位值的字典。
* `errors` : 存储采集失败的错误码的字典。
* `metas` : 驱动相关的元数据信息。

以下为使用 **values-format** 格式的数据样例，采集成功的点位数据值存放在`values`键中，采集失败的点位错误码存放在`errors`键中。
```json
{
    "timestamp": 1650006388943,
    "node": "modbus",
    "group": "grp",
    "values":
    {
        "tag0": 123
    },
    "errors":
    {
        "tag1": 2014
    },
    "metas":{}
}
```

::: tip 注意

当点位采集成功时，返回采集到的数据。当点位采集发生错误时，返回错误码，不再返回数值。

当 MQTT 插件配置项 **上报点位错误码** 为 **False** 时，不上报点位错误码。
:::

### Tags-format 格式

在 **Tags-format** 格式中， 上报的数据由以下字段构成：
* `timestamp` : 数据采集时的 UNIX 时间戳。
* `node` : 被采集的南向节点的名字。
* `group` : 被采集的南向节点的点位组的名字。
* `tags` : 点位数据数组，每个元素对应一个点位。
* `name` : 具体的点位名称字段。
* `value` : 具体点位对应的值。
* `error` : 具体点位对应的错误码。

以下为使用 **Tags-format** 格式的数据样例，其中所有点位数据存放在一个数组中，每个数组元素包含点位的名字，采集成功时的数据值或者采集失败时的错误码。

```json
{
  "timestamp": 1647497389075,
  "node": "modbus",
  "group": "grp",
  "tags": [
    {
      "name": "tag0",
      "value": 123,
    },
    {
      "name": "tag1",
      "error": 2014
    }
  ]
}
```

::: tip 注意

当点位采集成功时，返回采集到的数据。当点位采集发生错误时，返回错误码，不再返回数值。

当 MQTT 插件配置项 **上报点位错误码** 为 **False** 时，不上报点位错误码。
:::

### ECP-format 格式

在 **ECP-format** 格式中， 上报的数据由以下字段构成：
* `timestamp` : 数据采集时的 UNIX 时间戳。
* `node` : 被采集的南向节点的名字。
* `group` : 被采集的南向节点的点位组的名字。
* `tags` : 点位数据数组，每个元素对应一个点位。
* `name` : 具体的点位名称字段。
* `value` : 具体点位对应的值。
* `type` : 具体点位对应的数据类型，共包含布尔、整型、浮点型、字符串四种数据类型。


以下为使用 *ECP-format* 格式的数据样例，其中所有点位数据存放在一个数组中，每个数组元素包含点位的名字，点位的数据类型和采集成功时的数据值，不包含采集失败的点位。
数据类分为布尔、整型、浮点型、字符串四种。
* type = 1 布尔
* type = 2 整型
* type = 3 浮点型
* type = 4 字符串

```json
{
  "timestamp": 1647497389075,
  "node": "modbus",
  "group": "grp",
  "tags": [
    {
      "name": "tag_boolean",
      "value": true,
      "type": 1,
    },
    {
      "name": "tag_int32_",
      "value": 123,
      "type": 2,
    },
    {
      "name": "tag_float",
      "value": 1.23,
      "type": 3,
    },
    {
      "name": "tag_string",
      "value": "abcd",
      "type": 4,
    }
  ]
}
```

::: tip 注意
ECP-format 格式不支持上报点位错误码。
不管 MQTT 插件配置项 **上报点位错误码** 为 **False** 或为 **True** ，都不上报点位错误码。
:::

### Custom 自定义格式

在自定义格式中，可以使用内置支持的变量自定义数据上报格式。

![custom-format](./assets/custom-format.png)

#### 内置变量

| **变量名称** | **说明** |
| ------------------ | ---------------------- | 
| `${timestamp}` | 数据采集时的 UNIX 时间戳。 |
| `${node}` | 南向采集点位所属的南向驱动名称。<br> 示例：`modbus` |
| `${group}` | 南向采集点位所属的南向驱动的采集组的名称。<br> 示例：`group` |
| `${tags}` | 南向采集点位有效值的数组集合。<br> 示例：`[{"name": "tag1", "value": 123}, {"name": "tag2", "value": 456}]` |
| `${tag_values}` | 南向采集点位有效值的Json集合。<br> 示例：`{"tag1": 123, "tag2": 456}` |
| `${tag_errors}` | 南向采集点位报错值的数组集合。<br> 示例：`[{"name": "tag3", "error": 2014}, {"name": "tag4", "error": 2015}]` |
| `${tag_error_values}` | 南向采集点位报错值的Json集合。<br> 示例：`{"tag3": 2014, "tag4": 2015}` |
| `${static_tags}` | 订阅采集组时，自定义配置的静态点位的数组集合。<br> 示例： `[{"name": "static_tag1", "value": "abc"}, {"name": "static_tag2", "value": "def"}]` <br> 详见 [静态点位](#静态点位)|
| `${static_tag_values}` | 订阅采集组时，自定义配置的静态点位的Json集合。<br> 示例： `{"static_tag1": "abc", "static_tag2": "def"}`<br> 详见 [静态点位](#静态点位) |

::: tip 注意
`${tags}` 和 `${tag_values}` 是采集数据点的2种不同格式，`${tags}` 是数组格式，`${tag_values}` 是Json格式。一般在构建自定义数据上报格式时，使用 `${tags}` 或 `${tag_values}` 中的一种即可。

`${static_tags}` 和 `${static_tag_values}`、`${tag_errors}`和`${tag_error_values}` 同理，使用其中一种即可。

 MQTT 插件配置项 **上报点位错误码** 为 **False** 时，即使配置了`${tag_errors}` 和或 `${tag_error_values}`，也不上报点位错误码信息。

:::



上表中示例部分，原始南向驱动`modbus1`采集组`group1`下，包含4个点位，其中`tag1`和`tag2`的点位采集数据正常，`tag3`和`tag4`的点位采集数据异常。在北向节点订阅时，自定义配置的静态点位为`static_tag1`和`static_tag2`，其值分别为`abc`和`def`。

以上配置，以 Values-format 数据格式上报时，内容如下：


```json
{
    "timestamp": 1650006388943,
    "node": "modbus1",
    "group": "group1",
    "values": {"tag1": 123, "tag2": 456, "static_tag1": "abc", "static_tag2": "def"},
    "errors": {"tag3": 2014, "tag4": 2015},
    "metas":{}
}
```

以下示例通过设定不同的 Custom 配置，将上面 Json 报文输出为不同的数据上报格式。
#### 示例一

将数据点位转换为数组格式，并自定义Json 键名。

```json
{
    "timestamp": "${timestamp}",
    "node": "${node}",
    "group": "${group}",
    "custom_tag_name": "${tags}",
    "custom_tag_errors": "${tag_errors}",
}
```

实际输出结果如下：

```json
{
    "timestamp": 1650006388943,
    "node": "modbus1",
    "group": "group1",
    "custom_tag_name": [{"name": "tag1", "value": 123}, {"name": "tag2", "value": 456}],
    "custom_tag_errors": [{"name": "tag3", "error": 2014}, {"name": "tag4", "error": 2015}]
}
```

#### 示例二

通过自定义数据格式，添加全局静态点位。该静态点位在所有驱动采集组上报数据时，均会携带。如 NeuronEX 部署在一个物理网关上，可将网关的SN号、IP地址、位置信息等信息，添加到所有驱动采集组中。

```json
{
    "timestamp": "${timestamp}",
    "node": "${node}",
    "group": "${group}",
    "values": "${tag_values}",
    "gateway_info": {
      "ip": "192.168.1.100",
      "sn": "SN123456789",
      "location": "shanghai"
    }
}
```
实际输出结果如下：

```json
{
    "timestamp": 1650006388943,
    "node": "modbus1",
    "group": "group1",
    "values": {"tag1": 123, "tag2": 456},
    "gateway_info": {
      "ip": "192.168.1.100",
      "sn": "SN123456789",
      "location": "shanghai"
    }
}
```

#### 示例三

将采集数据点位、静态点位、驱动、采集组信息放入 Json 子节点中，可自定义 Json 节点名称。目前自定义数据结构做多支持三层子层级。

```json
{
    "timestamp": "${timestamp}",
    "data": {
      "child_node": {
        "node": "${node}",
        "group": "${group}",
        "tag_values": "${tag_values}",
        "static_tag_values": "${static_tag_values}",
        "tag_error_values": "${tag_error_values}"
      }
    }
}
```

数据上报的格式为：
```json
{
    "timestamp": 1650006388943,
    "data": {
      "child_node": {
        "node": "modbus",
        "group": "group",
        "tag_values": {"tag1": 123, "tag2": 456},
        "static_tag_values": {"static_tag1": "abc", "static_tag2": "def"},
        "tag_error_values": {"tag3": 2014, "tag4": 2015}
      }
    }
}
```


## 静态点位

静态点位功能，支持在南向驱动数据上报时，由用户在**北向应用组列表**页面手动配置静态点位，在数据上报时，将静态点位数据一并上报。

![static-tags](./assets/static-tags.png)

静态点位配置，需输入标准JSON格式内容，每个静态点位均为JSON对象中的键值对。静态点位将根据配置的数据上报格式（Values-format、Tags-format、ECP-format）与采集组数据一并上报。

```json
  {
      "location": "sh",
      "sn_number": "123456"
  }

```

每个南向采集组，可分别配置不同静态点位。表示该采集组代表的物理设备，具有哪些静态属性信息。

静态点位功能支持布尔、整型、浮点型、字符串四种数据类型。复杂数据类型，如数组、结构体等，将以字符串形式上报。

如果添加的静态点位，为 NeuronEX 实例中所有采集组共用，既可以勾选**所有采集组**，添加静态点位，也可以在 Custom 自定义格式中，统一添加静态点位，参考 [自定义格式：示例二](#自定义格式)。

::: tip 注意
静态点位命名，不能和南向驱动采集的点位名称相同。

如命名相同，则如果选择`Values-format`上报数据时，静态点位数据将覆盖南向驱动采集的点位数据.

如果选择`Tags-format`、`ECP-format`或`Custom`上报数据时，静态点位数据将和南向驱动采集的点位数据一起重复上报。
:::

## 读 Tags

### 请求

通过发送 JSON 形式的请求到 MQTT 主题 **/neuron/{node_name}/read/req**，您可以读取一组点位数据。

#### 请求体

读请求体由以下字段构成：
* `uuid` : 唯一的标志，会在响应中原样返回用以区分对应的请求。
* `node` : 某个南向节点名字。
* `group` : 南向节点的某个组的名字。

以下为一个读请求样例：

```json
{
    "uuid": "bca54fe7-a2b1-43e2-a4b4-1da715d28eab",
    "node": "modbus",
    "group": "grp"
}
```

### 响应

读响应会发布到 MQTT 主题 **/neuron/{node_name}/read/resp** 。

#### 响应体

读响应体由以下字段构成：
* `uuid` : 对应请求中所设置的唯一标志。
* `tags` : 点位数据数组，和 [tags 格式](#tags-格式)中的表示方法一样。

以下为一个读响应样例：

```json
{
  "uuid": "bca54fe7-a2b1-43e2-a4b4-1da715d28eab",
  "tags": [
    {
      "name": "tag0",
      "value": 4,
    },
    {
      "name": "tag1",
      "error": 2014
    }
  ]
}
```

::: tip
当点位采集成功时，返回采集到的数据。当点位采集发生错误时，返回错误码，不再返回数值。
:::

## 写 Tag

### 请求

通过发送 JSON 形式的请求到**写请求主题**参数指定的 MQTT 主题，您可以写一个点位数据。可在 MQTT 参数配置中配置写请求的主题，默认为为 **/neuron/{random_str}/write/req**。


#### 请求体

#### 写一个 Tag

写请求体由以下字段构成：
* `uuid` : 唯一的标志，会在响应中原样返回用以区分对应的请求。
* `node` : 某个南向节点名字。
* `group` : 南向节点的某个组的名字。
* `tag` : 要写入的点位名字。
* `value` : 要写入的数据值。

以下为一个写请求样例：

```json
{
    "uuid": "cd32be1b-c8b1-3257-94af-77f847b1ed3e",
    "node": "modbus",
    "group": "grp",
    "tag": "tag0",
    "value": 1234
}
```

#### 写多个 Tag

Neuron 提供了对多点位写入的支持。要在一次请求中可以写入多个点位数据, 写请求体应由以下字段构成：
* `uuid` : 唯一的标志，会在响应中原样返回用以区分对应的请求。
* `node` : 某个南向节点名字。
* `group` : 南向节点的某个组的名字。
* `tags` : 点位数据数组，每个元素对应一个点位。

以下为一个写请求样例：

```json
{
    "uuid": "cd32be1b-c8b1-3257-94af-77f847b1ed3e",
    "node": "modbus",
    "group": "grp",
    "tags": [
      {
        "tag": "tag0",
        "value": 1234
      },
      {
        "tag": "tag1",
        "value": 5678
      }
    ]
}
```

### 响应

写响应会发布到**写响应主题**参数指定的 MQTT 主题。默认**写响应主题**参数为 **/neuron/{random_str}//write/resp**。

#### 响应体

写响应体由以下字段构成：
* `uuid` : 对应请求中所设置的唯一标志。
* `error` : 错误码。0 代表成功，非零代表失败。

以下为一个读响应样例：
```json
{
  "uuid": "cd32be1b-c8b1-3257-94af-77f847b1ed3e",
  "error": 0
}
```

## 驱动状态上报

上报所有南向驱动状态到指定的 MQTT 主题。

### 状态上报主题

上报主题在北向节点配置中指定，其默认值为 **/neuron/{random_str}/state/update** 

### 状态上报间隔

状态上报间隔在北向节点配置中指定，指每条消息之间间隔的秒数，其默认值为 1，允许的范围为 1-3600。

### 上报消息格式

上报的数据由以下字段构成：
* `timestamp` : 数据采集时的 UNIX 时间戳。
* `states` : 节点状态信息的数组。

以下是一个驱动状态上报消息样例。

```json
{
  "timestamp": 1658134132237,
  "states": [
    {
      "node": "modbus-tcp",
      "link": 1,
      "running": 3
    },
    {
      "node": "modbus-rtu",
      "link": 1,
      "running": 3
    }
  ]
}
```
