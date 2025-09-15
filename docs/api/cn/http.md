# 配置

Neuron 将为 IIoT 平台提供一系列 API 服务，用于查询基本信息、控制网关行为或设置轮询配置。 IIoT 平台必须通过向 Neuron 发送请求消息来启动通信。 通过返回，Neuron 将返回所需的信息或执行相应的操作。 如果有错误，将返回一个错误代码来说明失败的原因。

## 常量定义

### 数据类型

* INT8   = 1
* UINT8  = 2
* INT16  = 3
* UINT16 = 4
* INT32  = 5
* UINT32 = 6
* INT64  = 7
* UINT64 = 8
* FLOAT  = 9
* DOUBLE = 10
* BIT    = 11
* BOOL   = 12
* STRING = 13
* BYTES  = 14
* ERROR = 15
* WORD = 16
* DWORD = 17
* LWORD = 18

### 波特率

* 115200 = 0
* 57600  = 1
* 38400  = 2
* 19200  = 3
* 9600   = 4
* 4800   = 5
* 2400   = 6
* 1800   = 7
* 1200   = 8
* 600    = 9

### 校验位

* NONE   = 0
* ODD    = 1
* EVEN   = 2
* MARK   = 3
* SPACE  = 4

### 停止位

* Stop_1 = 0
* Stop_2 = 1

### 数据位

* Data_5 = 0
* Data_6 = 1
* Data_7 = 2
* Data_8 = 3

### 点位属性

* READ = 0x01
* WRITE = 0x02
* SUBSCRIBE = 0x04

### Node 类型

* DRIVER = 1
* APP = 2

### Node 控制

* START = 0
* STOP = 1

### Node 状态

* INIT = 1
* READY = 2
* RUNNING = 3
* STOPPED = 4

### Node 连接状态

* DISCONNECTED = 0
* CONNECTED = 1

### 插件类型

* STATIC = 0
* SYSTEM = 1
* CUSTOM = 2


## Ping

*POST*  **/api/v2/ping**

### 响应状态

* 200 OK

## 登录

*POST*   **/api/v2/login**

### 请求头部

**Content-Type**          application/json

### 响应状态

* 200 OK
* 401
  * 1004, 缺少令牌
  * 1005, 解码令牌错误
* 403
  * 1006, 令牌过期
  * 1007, 验证令牌错误
  * 1008, 无效令牌

### 请求体

```json
{
    "name": "admin",
    "pass": "0000"
}
```

### 响应

```json
{
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE2MzcyODcxNjMsImlhdCI6MTYzNzIwMDc2MywiaXNzIjoiRU1RIFRlY2hub2xvZ2llcyBDby4sIEx0ZCBBbGwgcmlnaHRzIHJlc2VydmVkLiIsInBhc3MiOiIwMDAwIiwidXNlciI6ImFkbWluIn0.2EZzPC9djErrCeYNrK2av0smh-eKxDYeyu7cW4MyknI"
}
```

## 更改密码

*POST*   **/api/v2/password**

### 请求头部

**Content-Type** application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 401
  * 1004, 缺少令牌
  * 1005, 解码令牌错误
  * 1012, 密码长度太短或太长
  * 1013, 密码重复
* 403
  * 1006, 令牌过期
  * 1007, 验证令牌错误
  * 1008, 无效令牌

### 请求体

```json
{
    "name": "admin",
    "old_pass": "01234",
    "new_pass": "56789"
}
```

### 响应

```json
{
    "error": 0
}
```

## 添加 Node

*POST*  **/api/v2/node**

### 请求头部

**Content-Type** application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 400
  * 2001 node 类型无效
  * 2004 node 配置无效
* 404
  * 2301 未找到插件库
* 409
  * 2002 node 不存在

### 请求体

```json
{
    //node name
    "name": "modbus-tcp-node",
    //plugin name
    "plugin": "Modbus TCP"
    //setting (optional)
    "params": {
        "param1": 1,
        "param2": "1.1.1.1",
        "param3": true,
        "param4": 11.22
    }
}
```

### 响应

```json
{
    "error": 0
}
```

## 删除 Node

*Delete* /api/v2/node

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2003 node not exist

### 请求体

```json
{
    //node name
    "name": "modbus-tcp-node"
}
```

### 响应

```json
{
    "error": 0
}
```

## 更新 Node

*PUT* **/api/v2/node**

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 400
  * 2013 node 不允许更新
  * 2015 node 名称不允许为空
* 404
  * 2003 node 不存在
* 409
  * 2002 node 已存在
* 500
  * 1001 内部错误
  * 1010 程序繁忙

### 请求体

```json
{
    "name": "modbus-node"
    "new_name": "modbus-tcp-node"
}
```

### 响应

```json
{
    "error": 0
}
```

## 获取 Node

*GET*  /api/v2/node

### 请求参数

**type**  必需

**plugin** 可选

**node** 可选

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK

### 响应

```json
{
    "nodes": [
        {
            //node name
            "name": "sample-driver-adapter",
            //plugin name
            "plugin": "Modbus TCP",
            "support_import_tags": false
        },
        {
            "name": "modbus-tcp-adapter",
            "plugin": "Modbus TCP",
            "support_import_tags": false
        }
    ]
}
```

## 配置 Node

*POST*  /api/v2/node/setting

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 400
  * 2003 node 不存在
  * 2004 node 配置无效

### 请求体

```json
//The parameter fields in json fill in different fields according to different plugins
{
    //node name
    "node": "modbus-node",
    "params": {
        "param1": 1,
        "param2": "1.1.1.1",
        "param3": true,
        "param4": 11.22
    }
}
```

:::tip
每个插件的配置参数具体可参考 [插件设置](./plugin-setting.md)。
:::

### 响应

```json
{
    "error": 0
}
```

## 获取 Node 配置

*GET*  /api/v2/node/setting

### 请求参数

**node**  必需

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
  * 2005 node 配置未发现
* 404
  * 2003 node 不存在

### 响应

```json
//The parameter fields in json fill in different fields according to different plugins
{
    "node": "modbus-node",
    "params": {
        "param1": "1.1.1.1",
        "param2": 502
    }
}
```

## 控制 Node

*POST*  /api/v2/node/ctl

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 请求状态

* 200 OK
* 409
  * 2006 node not ready
  * 2007 node is running
  * 2008 node not running
  * 2009 node is stopped

### 请求体

```json
{
    //node name
    "node": "modbus-node",
    //0 start, 1 stop
    "cmd": 0
}
```

### 响应

```json
{
    "error": 0
}
```

## 获取 Node 状态

*GET*  /api/v2/node/state

### 请求参数

**node**  optional

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK

### 响应

```json
{
    "states": [
        {
            //node name
            "node": "modbus-node1",
            //running state
            "running": 2,
            //link state
            "link": 1,
            //average round trip time communicating with devices
            "rtt": 100,
            //log level
            "log_level": "notice"
        },
        {
            "node": "modbus-node2",
            "running": 1,
            "link": 0,
            "rtt": 9999,
            "log_level": "notice"
        }
    ],
    //log level of neuron.log
    "neuron_core": "notice"
}
```

## 添加 Group

*POST*  /api/v2/group

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2003 node not exist
* 409
  * 2103 group not allow

### 请求体

```json
{
    //group name
    "group": "gconfig1",
    //node name
    "node": "modbus-node",
    //read/upload interval(ms)
    "interval": 10000
}
```

### 响应

```json
{
    "error": 0
}
```

## 删除 Group

*DELETE*  /api/v2/group

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 412
  * 2101 group already subscribed
* 404
  * 2003 node not exist
  * 2101 group not exist

### 请求体

```json
{
    //node name
    "node": "modbus-node",
    //group name
    "group": "gconfig1"
}
```

### 响应

```json
{
    "error": 0
}
```

## 更新 Group

*PUT*  /api/v2/group

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2003 node not exist
  * 2106 group not exist
* 409
  * 2104 group exist

### 请求体

```json
{
    //node name
    "node": "node1",
    //group name
    "group": "group",
    //read/upload interval(ms)
    "interval": 20000
}
```

更新 group name:
```json
{
    //node name
    "node": "modbus-node",
    //group name
    "group": "gconfig1",
    //group new name
    "new_name": "group1"
}
```

更新 group interval:
```json
{
    //node name
    "node": "modbus-node",
    //group name
    "group": "gconfig1",
    //read/upload interval(ms)
    "interval": 10000
}
```

同时更新 group name 和 interval:
```json
{
    //node name
    "node": "modbus-node",
    //group name
    "group": "gconfig1",
    //group new name
    "new_name": "group1",
    //read/upload interval(ms)
    "interval": 10000
}
```

### 响应

```json
{
    "error": 0
}
```

## 获取 Group

*GET*  /api/v2/group

### 请求参数

**node**  可选

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK

### 响应

````json
{
    "groups": [
        {
            //group name
            "name": "config_modbus_tcp_sample_2",
            //read/upload interval(ms)
            "interval": 2000,
            //tag count
            "tag_count": 0
        },
        {
            "name": "gconfig1",
            "interval": 10000,
            "tag_count": 0
        }
    ]
}
````

## 添加 Tag

*POST*  /api/v2/tags

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 206
  * 2202 tag name conflict
  * 2203 tag attribute not support
  * 2204 tag type not support
  * 2205 tag address format invalid
* 404
  * 2003 node not exist

### 请求体

```json
{
   //node name
    "node": "modbus-node",
   //group name
    "group": "config_modbus_tcp_sample_2",
    "tags": [
        {
           //tag name
            "name": "tag1",
           //tag address
            "address": "1!400001",
           //tag attribute
            "attribute": 8,
           //tag type
            "type": 4,
           //optional, float/double precision, optional(0-17)
            "precision": 0,
           //optional, decimal
            "decimal": 0,
           //optional, description
           "description": "",
           //optional, when the attribute is static,the value field needs to be added.
           "value": 12
        },
        {
            "name": "tag2",
            "address": "1!00001",
            "attribute": 3,
            "type": 3,
            "decimal": 0.01
        },
        {
            "name": "tag3",
            "address": "1!400009",
            "attribute": 3,
            "type": 9,
            "precision": 3
        },
        {
            "name": "static_tag",
            "address": "",
            "attribute": 10,
            "type": 1,
            "description": "It is a static tag",
            "value": 42
        }
    ]
}
```

### 响应

```json
{
    "index": 1,
    "error": 0
}
```

## 添加多组 Tag

*POST*  /api/v2/gtags

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 206
  * 2202 tag name conflict
  * 2203 tag attribute not support
  * 2204 tag type not support
  * 2205 tag address format invalid
* 404
  * 2003 node not exist
  * 2106 group not exist

### 请求体

```json
{
    //node name
    "node": "modbus-node",
    "groups": [
        {
            //group name
            "group": "group_1",
            //group interval
            "interval": 3000,
            "tags": [
                {
                    //tag name
                    "name": "tag1",
                    //tag address
                    "address": "1!400001",
                    //tag attribute
                    "attribute": 3,
                    //tag type
                    "type": 3,
                    //optional, float/double precision, optional(0-17)
                    "precision": 0,
                    //optional, decimal
                    "decimal": 0,
                    //optional, description
                    "description": "",
                    //optional, when the attribute is static,the value field needs to be added.
                    "value": 12
                },
                {
                    "name": "tag2",
                    "address": "1!400002",
                    "attribute": 3,
                    "type": 9,
                    "precision": 3
                }
            ]
        },
        {
            "group": "group_2",
            "interval": 3000,
            "tags": [
                {
                    "name": "tag1",
                    "address": "1!400003",
                    "attribute": 3,
                    "type": 9,
                    "precision": 3
                },
                {
                    "name": "tag2",
                    "address": "1!400004",
                    "attribute": 3,
                    "type": 9,
                    "precision": 3
                }
            ]
        }
    ]
}

```


### 响应

```json
{
    //tags count
    "index": 4,
    "error": 0
}
```

## 导入内置 Tag

*POST*  /api/v2/tags/import

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 206
  * 2202 tag name conflict
  * 2203 tag attribute not support
  * 2204 tag type not support
  * 2205 tag address format invalid
* 404
  * 2003 node not exist
  * 3029 not support import tags

### 请求体

```json
{
    //node name
    "node": "cnc-node"
}

```


### 响应

```json
{
    //tags count
    "index": 4,
    "error": 0
}
```

## 获取 Tag

*GET*  /api/v2/tags

### 请求参数

**node**  必需

**group**  必需

**name** 可选

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2003 node 不存在

### 响应

```json
{
    "tags": [
        {
            //tag name
            "name": "tag1",
            //tag type
            "type": 4,
            //tag address
            "address": "1!400001",
            //tag attribute
            "attribute": 8,
            //description
            "description": "",
            //float/double precision
            "precison": 0,
            //decimal
            "decimal": 0,
            //optional, when the attribute is static
            "value": 12
        },
        {
            "name": "tag2",
            "type": 14,
            "address": "1!00001",
            "attribute": 3,
            "description": "",
            "precison": 0,
            "decimal": 0,
        },
        {
            "name": "tag3",
            "type": 11,
            "address": "1!400009",
            "attribute": 3,
            "description": "",
            "precison": 0,
            "decimal": 0,
        }
    ]
}
```

## 更新 Tag

*PUT*  /api/v2/tags

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 206
  * 2201 tag 不存在
  * 2202 tag 名字冲突
  * 2203 tag 属性不支持
  * 2204 tag 类型不支持
  * 2205 tag 地址格式无效
* 404
  * 2003 node 不存在
  * 2106 group 不存在

### 请求体

```json
{
    //node name
    "node": "modbus-tcp-test",
    //group name
    "group": "group1",
    "tags": [
        {
            //tag name
            "name": "tag1",
            //tag type
            "type": 8,
            //tag attribute
            "attribute": 0,
            //tag address
            "address": "1!400001",
            //description
            "description":"",
            //float/double precision
            "precison": 0,
            //decimal
            "decimal": 0,
            //when the attribute is static,the value field needs to be added.
            "value": 12
        },
        {
            "name": "tag2",
            "type": 6,
            "attribute": 0,
            "address": "1!400002",
            "description":"",
            "precison": 0,
            "decimal": 0,
        },
        {
            "name": "static_tag",
            "address": "",
            "attribute": 10,
            "type": 1,
            "description":"",
            "precison": 0,
            "decimal": 0,
            "value": 42
        }
    ]
}
```

### 响应

```json
{
    "error": 0,
    "index": 1
}
```

## 删除 Tag

*DELETE*  /api/v2/tags

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2003 node 不存在

### 请求体

```json
{
    //group name
    "group": "config_modbus_tcp_sample_2",
    //node name
    "node": "modbus-node",
    //tag names
    "tags": [
        "tag1",
        "tag2"
    ]
}
```

### 响应

```json
{
    "error": 0
}
```

## 添加插件

*POST*  /api/v2/plugin

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK

* 400
  
  * 2302 库信息无效
  * 2303 库名称冲突
  * 2304 库文件打开失败
  * 2305 库文件插件无效
  * 2307 库文件插件实例化失败
  * 2308 库文件架构不支持
  * 2310 库文件添加失败
  * 2311 库文件插件已存在
  * 2313 库文件插件类型不支持

### 请求体

```json
{
    //plugin library name
    "library": "plugin_name.so",
    // base64 content of schema json file
    "schema_file":"...",
    // base64 content of library file
    "so_file":"..."
}
```

### 响应

```json
{
    "error": 0
}
```

## 更新插件

*PUT*  /api/v2/plugin

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK

* 400
  
  * 2302 库文件不存在
  * 2302 库信息无效
  * 2304 库文件打开失败
  * 2305 库文件插件无效
  * 2307 库文件插件实例化失败
  * 2308 库文件架构不支持
  * 2310 库文件添加失败
  * 2312 库文件插件不存在
  * 2313 库文件插件类型不支持

### 请求体

```json
{
    //plugin library name
    "library": "plugin_name.so",
    // base64 content of schema json file
    "schema_file":"...",
    // base64 content of library file
    "so_file":"..."
}
```

### 响应

```json
{
    "error": 0
}
```



## 删除插件

*DELETE*  /api/v2/plugin

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK

* 400
* 
  * 2306 系统插件不允许删除
  * 2309 插件使用中

### 请求体

```json
{
    //plugin name
    "plugin": "modbus-tcp"
}
```

### 响应

```json
{
    "error": 0
}
```

## 获取插件

*GET*  /api/v2/plugin

### 请求参数

**plugin**  optional

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK

### 响应

```json
{
    "plugins": [
        {
            //plugin kind
            "kind": 1,
            //node type
            "node_type": 1,
            //plugin name
            "name": "Modbus TCP",
            //plugin library name
            "library": "libplugin-modbus-tcp.so",
            "description": "description",
            "description_zh": "描述",
            "schema": "modbus-tcp"
        },
        {
            "kind": 1,
            "node_type": 2,
            "name": "MQTT",
            "library": "libplugin-mqtt.so",
            "description": "Neuron northbound MQTT plugin bases on NanoSDK.",
            "description_zh": "基于 NanoSDK 的 Neuron 北向应用 MQTT 插件",
            "schema": "mqtt"
        }
    ]
}
```

## 获取插件 Schema

*GET*  /api/v2/schema

### 请求参数

**schema_name**  必需

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK

### 响应

```json
{
    "tag_regex": [
        {
            "type": 3,
            "regex": "^[0-9]+![3-4][0-9]+(#B|#L|)$"
        },
        {
            "type": 4,
            "regex": "^[0-9]+![3-4][0-9]+(#B|#L|)$"
        },
        {
            "type": 5,
            "regex": "^[0-9]+![3-4][0-9]+(#BB|#BL|#LL|#LB|)$"
        },
        {
            "type": 6,
            "regex": "^[0-9]+![3-4][0-9]+(#BB|#BL|#LL|#LB|)$"
        },
        {
            "type": 7,
            "regex": "^[0-9]+![3-4][0-9]+(#B|#L|)$"
        },
        {
            "type": 8,
            "regex": "^[0-9]+![3-4][0-9]+(#B|#L|)$"
        },
        {
            "type": 9,
            "regex": "^[0-9]+![3-4][0-9]+(#BB|#BL|#LL|#LB|)$"
        },
        {
            "type": 10,
            "regex": "^[0-9]+![3-4][0-9]+(#B|#L|)$"
        },
        {
            "type": 11,
            "regex": "^[0-9]+!([0-1][0-9]+|[3-4][0-9]+\\.([0-9]|[0-1][0-5]))$"
        },
        {
            "type": 13,
            "regex": "^[0-9]+![3-4][0-9]+\\.[0-9]+(H|L|)$"
        }
    ],
    "group_interval": 1000,
    "connection_mode": {
        "name": "Connection Mode",
        "name_zh": "连接模式",
        "description": "Neuron as the client, or as the server",
        "description_zh": "Neuron 作为客户端或服务端",
        "attribute": "required",
        "type": "map",
        "default": 0,
        "valid": {
            "map": [
                {
                    "key": "Client",
                    "value": 0
                },
                {
                    "key": "Server",
                    "value": 1
                }
            ]
        }
    },
    "interval": {
        "name": "Send Interval",
        "name_zh": "指令发送间隔",
        "description": "Send reading instruction interval(ms)",
        "description_zh": "发送读指令时间间隔，单位为毫秒",
        "attribute": "required",
        "type": "int",
        "default": 20,
        "valid": {
            "min": 0,
            "max": 3000
        }
    },
    "host": {
        "name": "IP Address",
        "name_zh": "IP地址",
        "description": "Local IP in server mode, remote device IP in client mode",
        "description_zh": "服务端模式中填写本地 IP，客户端模式中填写目标设备 IP",
        "attribute": "required",
        "type": "string",
        "valid": {
            "regex": "/^((2[0-4]\\d|25[0-5]|[01]?\\d\\d?)\\.){3}(2[0-4]\\d|25[0-5]|[01]?\\d\\d?)$/",
            "length": 30
        }
    },
    "port": {
        "name": "Port",
        "name_zh": "端口号",
        "description": "Local port in server mode, remote device port in client mode",
        "description_zh": "服务端模式中填写本地端口号，客户端模式中填写远程设备端口号",
        "attribute": "required",
        "type": "int",
        "default": 502,
        "valid": {
            "min": 1,
            "max": 65535
        }
    },
    "timeout": {
        "name": "Connection Timeout",
        "name_zh": "连接超时时间",
        "description": "Connection timeout(ms)",
        "description_zh": "连接超时时间，单位为毫秒",
        "attribute": "required",
        "type": "int",
        "default": 3000,
        "valid": {
            "min": 1000,
            "max": 65535
        }
    }
}
```

## 订阅

*POST*  /api/v2/subscribe

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2106 group 不存在

### 请求体

```json
{
    //app name
    "app": "mqtt",
    //driver name
    "driver": "modbus-tcp",
    //driver node group name
    "group": "group-1",
    //when using the MQTT plugin, the topic field needs to be added
    "params": {
        "topic": "/neuron/mqtt/group-1"
    },
    //optional, static tags
    "static_tags": {
        "static_tag1": "aabb", "static_tag2": 1.401, "static_tag3": false, "static_tag4": [1,2,4]
    }
}
```

### 响应

```json
{
    "error": 0
}
```

## 订阅多个组

*POST*  /api/v2/subscribes

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2106 group 不存在

### 请求体

```json
{
  //app name
  "app": "mqtt",
  "groups": [
    {
      //driver name
      "driver": "modbus1",
      //group name
      "group": "group1",
      //optional, depends on plugins
      "params": {
        //when using the MQTT plugin, the topic key is the upload topoic
        "topic": "/neuron/mqtt/modbus1/group1"
      },
      //optional, static tags
      "static_tags": {
        "static_tag1": "aabb", "static_tag2": 1.401, "static_tag3": false, "static_tag4": [1,2,4]
     }
    },
    {
      "driver": "modbus2",
      "group": "group2",
      "params": {
        "topic": "/neuron/mqtt/modbus2/group2"
      }
    }
  ]
}
```

### 响应

```json
{
    "error": 0
}
```

## 更新订阅参数

*PUT*  /api/v2/subscribe

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2106 group 不存在

### 请求体

```json
{
    //app name
    "app": "mqtt",
    //driver name
    "driver": "modbus-tcp",
    //driver node group name
    "group": "group-1",
    "params": {
        //when using the MQTT plugin, the topic key is the upload topic
        "topic": "/neuron/mqtt/group-1"
    },
    //optional, static tags
    "static_tags": {
        "static_tag1": "aabb", "static_tag2": 1.401, "static_tag3": false, "static_tag4": [1,2,4]
    }
}
```

### 响应

```json
{
    "error": 0
}
```

## 取消订阅

*DELETE*  /api/v2/subscribe

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2106 group 不存在

### 请求体

```json
{
    //app name
    "app": "mqtt",
    //driver name
    "driver": "modbus-tcp",
    //driver node group name
    "group": "group-1",
    //optional, when using the MQTT plugin, the topic field needs to be added
    "params": {
        "topic": "/neuron/mqtt/group-1"
    }
}
```

### 响应

```json
{
    "error": 0
}
```

## 获取订阅的 Group

*GET*  /api/v2/subscribe

### 请求参数

**app**  必需

**driver**  可选，对南向节点名字进行子字符串匹配过滤。

**group**  可选，对组名字进行子字符串匹配过滤。

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200
* 400

### 响应

```json
{
    "groups": [
        {
            //driver name
            "driver": "modbus-tcp",
            //group name
            "group": "group-1",
            //optional, when using the MQTT plugin, the topic field needs to be added
            "params": {
                "topic": "/neuron/mqtt/group-1"
            }
            //optional, static tags
            "static_tags": {
                "static_tag1": "aabb", "static_tag2": 1.401, "static_tag3": false, "static_tag4": [1,2,4]
            }
        },
        {
            //driver name
            "driver": "modbus-tcp",
            //group name
            "group": "group-2",
            //when using the MQTT plugin, the topic field needs to be added
            "params": {
                "topic": "/neuron/mqtt/group-2"
            }
        }
    ]
}
```

## 获取组订阅状态

*GET*  /api/v2/subscribes

### 请求参数

**app**  必需

**name**  可选，对南向节点名称或组名称进行过滤

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200
* 400

### 响应

```json
{
    "drivers": [
        {
            "name": "modbus",
            "groups": [
                {
                    "name": "g3",
                    "subscribed": true
                }
            ]
        }
    ]
}
```

## 获取版本信息

*GET*  /api/v2/version

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200
* 500
  * 1001 服务器内部错误

### 响应

```json
{
    "build_date": "2022-06-01",
    "revision": "99e2184+dirty", // dirty 表示有未提交的更改
    "version": "2.4.0"
}
```

## 上传 License

*POST*  /api/v2/license

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200
  * 0    OK
  * 2402 license 过期
* 400
  * 2401 license 无效
* 500
  * 1001 服务器内部错误

### 请求体

```json
{
    "license": "-----BEGIN CERTIFICATE-----\nMIID2TCCAsGgAwIBAgIEATSJqjA....."
}
```

### 响应

```json
{
    "error": 2401
}
```

## 获取 License 信息

*GET*  /api/v2/license

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2400 license 未找到
* 500
  * 1001 服务器内部错误

### 响应

```json
{
    "valid_until": "2023-03-15 08:11:19",
    "valid_since": "2022-03-15 08:11:19",
    "valid": false,
    "max_nodes": 1,
    "max_node_tags": 1,
    "used_nodes": 12,
    "used_tags": 846,
    "license_type": "retail",
    "error": 0,
    "enabled_plugins": [
        "MODBUS TCP Advance",
        "OPC UA"
    ],
    "hardware_token": "I+kZidSifiyVSbz0/EgcM6AcefnlfR4IU19ZZUnTS18=",
    "object": "emq",
    "email_address": "emq@emqx.io"
}
```

## 修改节点日志等级

*PUT*  /api/v2/log/level

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 404
  * 2003 node 不存在
* 500
  * 1001 内部错误
  * 1010 程序繁忙

### 请求体

```json
{
    // node name
    "node": "modbus-tcp",
    // log level: debug, info, notice, warn, error, fatal
    "level": "debug",
    // whether to switch the core log level
    "core": true
}
```

::: tip
core 字段选填，默认为 true。

node 字段选填，不填此字段时 core 不可以为 false，此时仅切换核心日志等级。
:::

### 响应

```json
{
    "error": 0
}
```

## 修改全局日志等级

*PUT*  /api/v2/log/global

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 500
  * 1001 内部错误
  * 1010 程序繁忙

### 请求体

```json
{
    "level": "debug",
}
```

::: tip
修改所有节点以及核心的日志等级。
:::

### 响应

```json
{
    "error": 0
}
```

## 获取全局日志等级

*GET*  /api/v2/log/global

### 请求头部

**Authorization** Bearer \<token\>

### 响应状态

* 200
* 500
  * 1001 服务器内部错误

### 响应

```json
{
    "level": "info",
}
```

## 批量添加 Drivers

*PUT* /api/v2/global/drivers

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 响应状态

* 200 OK
* 206
    * 2203    tag 属性不支持
    * 2204    tag 类型不支持
    * 2205    tag 地址格式无效
    * 2206    tag 名字太长
    * 2207    tag 地址太长
    * 2208    tag 描述太长
    * 2209    tag 精度无效
* 400
    * 1002    请求 body 无效
    * 2010    node 名称太长
    * 2011    node 不允许删除
    * 2105    group 参数无效
    * 2107    group 名称太长
    * 2304    库打开失败
    * 3013    插件名字太长
    * 3019    插件不支持请求的操作
* 404
    * 2301    库未找到
    * 3014    插件不存在
* 409
    * 2104    group 已存在
    * 2202    tag 名称冲突
    * 2307    插件不允许实例化
* 500
    * 1001    内部错误
    * 1010    程序繁忙

### 请求体

```json
{
    "nodes": [
        {
            "name": "rtu template",
            "plugin": "Modbus RTU",
            "params": {
                "param1": 1,
                "param2": "1.1.1.1",
                "param3": true,
                "param4": 11.22
            },
            "groups": [
                {
                    "name": "group1",
                    "interval": 2000,
                    "tags": [
                        {
                            "name": "tag1",
                            "type": 4,
                            "address": "1!400001",
                            "attribute": 1,
                            "precison": 1,
                            "decimal": 0
                        },
                        {
                            "name": "tag2",
                            "type": 11,
                            "address": "1!400009",
                            "attribute": 3
                        }
                    ]
                }
            ]
        }
    ]
}
```

### 响应

```json
{
    "error": 0
}
```


## 获取 Drivers

*GET* /api/v2/global/drivers

### 请求头部

**Content-Type**  application/json

**Authorization** Bearer \<token\>

### 请求参数

**name** Optional, list of names to filter out driver nodes (separated by ',')
**name** 可选，指定要过滤的南下节点名字列表（以 ',' 分隔）。 未指定 **name** 参数时，则返回所有模板的列表。

### 响应状态

* 200 OK
  * 2005      node 设置未找到
* 400
    * 1003    请求 param 无效
* 404
  * 2003      node 不存在
* 500
    * 1001    内部错误
    * 1010    程序繁忙

### 响应

如果成功，返回所有南向节点。

```json
{
    "nodes": [
        {
            "name": "rtu template",
            "plugin": "Modbus RTU",
            "params": {
                "param1": 1,
                "param2": "1.1.1.1",
                "param3": true,
                "param4": 11.22
            },
            "groups": [
                {
                    "name": "group1",
                    "interval": 2000,
                    "tags": [
                        {
                            "name": "tag1",
                            "type": 4,
                            "address": "1!400001",
                            "attribute": 1,
                            "precison": 1,
                            "decimal": 0
                        },
                        {
                            "name": "tag2",
                            "type": 11,
                            "address": "1!400009",
                            "attribute": 3
                        }
                    ]
                }
            ]
        }
    ]
}
```
否则返回错误玛。

```json
{
    "error": 0
}
```
