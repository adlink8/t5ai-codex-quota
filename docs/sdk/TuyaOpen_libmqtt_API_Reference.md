# TuyaOpen libmqtt API 参考手册

> **基于**: TuyaOpen `mqtt_client_interface.h`（Amazon coreMQTT v1.0.1 封装）  
> **头文件**: `#include "mqtt_client_interface.h"`  
> **依赖库**: TuyaOpen libmqtt  
> **文档版本**: 2026-06-10

---

## 目录

1. [概述](#1-概述)
2. [枚举类型](#2-枚举类型)
3. [结构体](#3-结构体)
4. [回调函数类型](#4-回调函数类型)
5. [API 函数](#5-api-函数)
   - [5.1 mqtt_client_new](#51-mqtt_client_new)
   - [5.2 mqtt_client_init](#52-mqtt_client_init)
   - [5.3 mqtt_client_connect](#53-mqtt_client_connect)
   - [5.4 mqtt_client_yield](#54-mqtt_client_yield)
   - [5.5 mqtt_client_subscribe](#55-mqtt_client_subscribe)
   - [5.6 mqtt_client_unsubscribe](#56-mqtt_client_unsubscribe)
   - [5.7 mqtt_client_publish](#57-mqtt_client_publish)
   - [5.8 mqtt_client_disconnect](#58-mqtt_client_disconnect)
   - [5.9 mqtt_client_deinit](#59-mqtt_client_deinit)
   - [5.10 mqtt_client_free](#510-mqtt_client_free)
   - [5.11 mqtt_client_is_connected](#511-mqtt_client_is_connected)
6. [典型使用流程](#6-典型使用流程)
7. [完整示例代码](#7-完整示例代码)
8. [注意事项与最佳实践](#8-注意事项与最佳实践)

---

## 1. 概述

TuyaOpen `libmqtt` 是对 Amazon coreMQTT v1.0.1 的轻量封装，提供基于事件回调的 MQTT 客户端接口。该库支持：

- MQTT 3.1.1 协议
- QoS 0 / QoS 1 消息
- TLS/SSL 加密连接（可选）
- 异步事件回调（连接、断连、消息接收、订阅确认等）
- 非阻塞网络事件处理（yield 模式）

**客户端生命周期**:

```
mqtt_client_new()  →  mqtt_client_init()  →  mqtt_client_connect()
                                                     ↓
                                              mqtt_client_yield()  ←── 主循环中反复调用
                                                     ↓
                                              mqtt_client_disconnect()
                                                     ↓
                                              mqtt_client_deinit()  →  mqtt_client_free()
```

---

## 2. 枚举类型

### 2.1 `mqtt_client_status_t`

MQTT 客户端操作的返回状态码。

```c
typedef enum {
    MQTT_STATUS_SUCCESS          = 0,   // 操作成功
    MQTT_STATUS_FAILURE          = -1,  // 通用失败
    MQTT_STATUS_INVALID_PARAM    = -2,  // 参数无效
    MQTT_STATUS_NO_MEMORY        = -3,  // 内存分配失败
    MQTT_STATUS_NETWORK_ERROR    = -4,  // 网络错误
    MQTT_STATUS_PROTOCOL_ERROR   = -5,  // 协议错误
    MQTT_STATUS_NOT_CONNECTED    = -6,  // 未连接
    MQTT_STATUS_TIMEOUT          = -7,  // 操作超时
} mqtt_client_status_t;
```

| 枚举值 | 数值 | 说明 |
|--------|------|------|
| `MQTT_STATUS_SUCCESS` | 0 | 操作成功完成 |
| `MQTT_STATUS_FAILURE` | -1 | 通用/未知失败 |
| `MQTT_STATUS_INVALID_PARAM` | -2 | 传入参数不合法 |
| `MQTT_STATUS_NO_MEMORY` | -3 | 内存分配失败 |
| `MQTT_STATUS_NETWORK_ERROR` | -4 | TCP 连接或数据传输错误 |
| `MQTT_STATUS_PROTOCOL_ERROR` | -5 | MQTT 协议层错误 |
| `MQTT_STATUS_NOT_CONNECTED` | -6 | 客户端未处于已连接状态 |
| `MQTT_STATUS_TIMEOUT` | -7 | 连接或操作超时 |

---

## 3. 结构体

### 3.1 `mqtt_client_config_t`

MQTT 客户端配置结构体，在调用 `mqtt_client_init()` 时传入。

```c
typedef struct {
    const char              *cacert;          // CA 证书（TLS 使用），非 TLS 时设为 NULL
    uint32_t                cacert_len;       // CA 证书长度（字节），非 TLS 时设为 0
    const char              *host;            // MQTT Broker 地址（IP 或域名）
    uint16_t                port;             // MQTT Broker 端口（通常 1883 或 8883）
    uint16_t                keepalive;        // 心跳保活间隔（秒），推荐 60
    uint32_t                timeout_ms;       // 连接超时时间（毫秒），推荐 5000
    const char              *clientid;        // MQTT 客户端 ID（必须唯一）
    const char              *username;         // 用户名（可选，无认证时设为 NULL）
    const char              *password;         // 密码（可选，无认证时设为 NULL）
    void                    *userdata;         // 用户自定义数据指针，会透传给所有回调
    mqtt_connected_cb_t     on_connected;      // 连接成功回调
    mqtt_disconnected_cb_t  on_disconnected;   // 断开连接回调
    mqtt_message_cb_t       on_message;        // 收到消息回调
    mqtt_published_cb_t     on_published;      // 消息发布成功回调
    mqtt_subscribed_cb_t    on_subscribed;     // 订阅成功回调
    mqtt_unsubscribed_cb_t  on_unsubscribed;   // 取消订阅成功回调
} mqtt_client_config_t;
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `cacert` | `const char*` | 否 | PEM 格式 CA 证书，用于 TLS 连接。非 TLS 设为 `NULL` |
| `cacert_len` | `uint32_t` | 否 | CA 证书字节长度。非 TLS 设为 `0` |
| `host` | `const char*` | **是** | Broker 地址，如 `"192.168.1.100"` 或 `"mqtt.example.com"` |
| `port` | `uint16_t` | **是** | Broker 端口，明文 `1883`，TLS `8883` |
| `keepalive` | `uint16_t` | 否 | 心跳间隔（秒），推荐 `60`。设为 `0` 禁用心跳 |
| `timeout_ms` | `uint32_t` | 否 | 连接超时（毫秒），推荐 `5000` |
| `clientid` | `const char*` | **是** | 客户端标识符，同一 Broker 上必须唯一 |
| `username` | `const char*` | 否 | 认证用户名，匿名时设为 `NULL` |
| `password` | `const char*` | 否 | 认证密码，匿名时设为 `NULL` |
| `userdata` | `void*` | 否 | 自定义上下文指针，透传到所有回调函数 |
| `on_connected` | 回调 | 否 | 连接建立成功时调用 |
| `on_disconnected` | 回调 | 否 | 连接断开时调用 |
| `on_message` | 回调 | 否 | 收到 PUBLISH 消息时调用 |
| `on_published` | 回调 | 否 | 消息发布完成时调用 |
| `on_subscribed` | 回调 | 否 | 订阅确认（SUBACK）时调用 |
| `on_unsubscribed` | 回调 | 否 | 取消订阅确认时调用 |

---

### 3.2 `mqtt_client_message_t`

接收到的 MQTT 消息结构体，作为 `on_message` 回调的参数传入。

```c
typedef struct {
    const char    *topic;       // 消息主题（null-terminated 字符串）
    const void    *payload;     // 消息负载（原始字节，不保证 null-terminated）
    uint32_t      length;       // 负载长度（字节）
} mqtt_client_message_t;
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `topic` | `const char*` | 消息主题名，null-terminated 字符串 |
| `payload` | `const void*` | 消息负载数据指针，**不保证以 `\0` 结尾** |
| `length` | `uint32_t` | 负载数据的字节长度 |

> ⚠️ **重要**: `payload` 不一定以 null 结尾。如需作为 C 字符串处理，必须手动复制并添加 `\0`。

---

## 4. 回调函数类型

所有回调的第一个参数 `client` 是 MQTT 客户端句柄，最后一个参数 `userdata` 是 `mqtt_client_config_t` 中的 `userdata` 透传值。

### 4.1 `mqtt_connected_cb_t` — 连接成功回调

```c
typedef void (*mqtt_connected_cb_t)(void *client, void *userdata);
```

| 参数 | 说明 |
|------|------|
| `client` | MQTT 客户端句柄 |
| `userdata` | 用户自定义数据 |

**触发时机**: TCP 连接建立 + MQTT CONNACK 收到后。通常在此回调中调用 `mqtt_client_subscribe()` 订阅主题。

---

### 4.2 `mqtt_disconnected_cb_t` — 断开连接回调

```c
typedef void (*mqtt_disconnected_cb_t)(void *client, void *userdata);
```

| 参数 | 说明 |
|------|------|
| `client` | MQTT 客户端句柄 |
| `userdata` | 用户自定义数据 |

**触发时机**: 连接异常断开（网络中断、服务端关闭、心跳超时等）。

> ⚠️ **注意**: 此回调可能在 `mqtt_client_yield()` 内部被调用。**不要在回调中销毁客户端**（调用 `mqtt_client_deinit` / `mqtt_client_free`），否则会导致 use-after-free。应设置标志位，由外部主循环负责重建连接。

---

### 4.3 `mqtt_message_cb_t` — 消息接收回调

```c
typedef void (*mqtt_message_cb_t)(void *client, uint16_t msgid,
                                   const mqtt_client_message_t *msg,
                                   void *userdata);
```

| 参数 | 说明 |
|------|------|
| `client` | MQTT 客户端句柄 |
| `msgid` | 消息 ID（QoS 1 时有效） |
| `msg` | 消息结构体指针，包含 topic、payload、length |
| `userdata` | 用户自定义数据 |

**触发时机**: 收到 PUBLISH 消息时。在回调中处理消息或将数据复制到缓冲区。

---

### 4.4 `mqtt_published_cb_t` — 发布成功回调

```c
typedef void (*mqtt_published_cb_t)(void *client, uint16_t msgid, void *userdata);
```

| 参数 | 说明 |
|------|------|
| `client` | MQTT 客户端句柄 |
| `msgid` | 发布消息的 ID |
| `userdata` | 用户自定义数据 |

**触发时机**: QoS 1 消息收到 PUBACK 时。

---

### 4.5 `mqtt_subscribed_cb_t` — 订阅成功回调

```c
typedef void (*mqtt_subscribed_cb_t)(void *client, uint16_t msgid, void *userdata);
```

| 参数 | 说明 |
|------|------|
| `client` | MQTT 客户端句柄 |
| `msgid` | 订阅请求的 ID |
| `userdata` | 用户自定义数据 |

**触发时机**: 收到 SUBACK 确认订阅成功时。

---

### 4.6 `mqtt_unsubscribed_cb_t` — 取消订阅回调

```c
typedef void (*mqtt_unsubscribed_cb_t)(void *client, uint16_t msgid, void *userdata);
```

| 参数 | 说明 |
|------|------|
| `client` | MQTT 客户端句柄 |
| `msgid` | 取消订阅请求的 ID |
| `userdata` | 用户自定义数据 |

**触发时机**: 收到 UNSUBACK 确认取消订阅时。

---

## 5. API 函数

### 5.1 `mqtt_client_new`

创建一个新的 MQTT 客户端实例。

```c
void* mqtt_client_new(void);
```

| | 说明 |
|------|------|
| **参数** | 无 |
| **返回值** | 客户端句柄指针，失败返回 `NULL` |
| **说明** | 仅分配内存和初始化内部状态，不建立网络连接 |

---

### 5.2 `mqtt_client_init`

初始化 MQTT 客户端，设置 Broker 地址、认证信息和回调函数。

```c
mqtt_client_status_t mqtt_client_init(void *client, mqtt_client_config_t *config);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `client` | `void*` | `mqtt_client_new()` 返回的句柄 |
| `config` | `mqtt_client_config_t*` | 配置结构体指针 |

| 返回值 | 说明 |
|--------|------|
| `MQTT_STATUS_SUCCESS` | 初始化成功 |
| `MQTT_STATUS_INVALID_PARAM` | 参数为 NULL 或配置不合法 |
| `MQTT_STATUS_NO_MEMORY` | 内存分配失败 |

---

### 5.3 `mqtt_client_connect`

发起 TCP 连接并发送 MQTT CONNECT 报文。

```c
mqtt_client_status_t mqtt_client_connect(void *client);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `client` | `void*` | 已初始化的客户端句柄 |

| 返回值 | 说明 |
|--------|------|
| `MQTT_STATUS_SUCCESS` | 连接请求已发出（不一定已建立） |
| `MQTT_STATUS_NETWORK_ERROR` | TCP 连接失败 |
| `MQTT_STATUS_TIMEOUT` | 连接超时 |
| `MQTT_STATUS_PROTOCOL_ERROR` | MQTT 协议握手失败 |

> **注意**: 此函数返回 `SUCCESS` 后，需要调用 `mqtt_client_yield()` 来处理 CONNACK 响应，此时 `on_connected` 回调才会被触发。

---

### 5.4 `mqtt_client_yield`

处理 MQTT 网络事件（非阻塞）。必须在主循环中周期性调用。

```c
void mqtt_client_yield(void *client);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `client` | `void*` | 客户端句柄 |

| 返回值 | 说明 |
|--------|------|
| `void` | 无返回值 |

**功能**:
- 发送/接收 MQTT 报文
- 处理 PINGREQ/PINGRESP 心跳
- 触发 `on_message`、`on_disconnected` 等回调
- 维持连接活跃

**调用频率**: 推荐每 100~200ms 调用一次。

> ⚠️ **注意**: `on_disconnected` 回调可能在此函数内部触发。不要在断连回调中销毁客户端。

---

### 5.5 `mqtt_client_subscribe`

订阅一个 MQTT 主题。

```c
uint16_t mqtt_client_subscribe(void *client, const char *topic, int qos);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `client` | `void*` | 客户端句柄（已连接状态） |
| `topic` | `const char*` | 主题字符串，支持通配符 `+`（单级）和 `#`（多级） |
| `qos` | `int` | 服务质量等级：`0`（至多一次）或 `1`（至少一次） |

| 返回值 | 说明 |
|--------|------|
| `> 0` | 订阅请求的消息 ID（成功发出） |
| `0` | 订阅失败 |

> **建议**: 通常在 `on_connected` 回调中调用。

---

### 5.6 `mqtt_client_unsubscribe`

取消订阅一个 MQTT 主题。

```c
uint16_t mqtt_client_unsubscribe(void *client, const char *topic);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `client` | `void*` | 客户端句柄（已连接状态） |
| `topic` | `const char*` | 要取消订阅的主题 |

| 返回值 | 说明 |
|--------|------|
| `> 0` | 取消订阅请求的消息 ID |
| `0` | 请求失败 |

---

### 5.7 `mqtt_client_publish`

发布一条 MQTT 消息。

```c
uint16_t mqtt_client_publish(void *client, const char *topic,
                              const void *payload, uint32_t length,
                              int qos, int retain);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `client` | `void*` | 客户端句柄（已连接状态） |
| `topic` | `const char*` | 目标主题 |
| `payload` | `const void*` | 消息负载数据 |
| `length` | `uint32_t` | 负载长度（字节） |
| `qos` | `int` | QoS 等级：`0` 或 `1` |
| `retain` | `int` | 是否保留消息：`0` 不保留，`1` 保留 |

| 返回值 | 说明 |
|--------|------|
| `> 0` | 发布消息的 ID（QoS 1 时可用于跟踪 PUBACK） |
| `0` | 发布失败 |

---

### 5.8 `mqtt_client_disconnect`

发送 MQTT DISCONNECT 报文并关闭网络连接。

```c
mqtt_client_status_t mqtt_client_disconnect(void *client);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `client` | `void*` | 客户端句柄 |

| 返回值 | 说明 |
|--------|------|
| `MQTT_STATUS_SUCCESS` | 断开成功 |
| 其他 | 操作失败（可能连接已断开） |

---

### 5.9 `mqtt_client_deinit`

反初始化 MQTT 客户端，释放内部资源（网络缓冲区、会话状态等）。

```c
void mqtt_client_deinit(void *client);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `client` | `void*` | 客户端句柄 |

| 返回值 | 说明 |
|--------|------|
| `void` | 无返回值 |

> **调用顺序**: 应在 `mqtt_client_disconnect()` 之后、`mqtt_client_free()` 之前调用。

---

### 5.10 `mqtt_client_free`

释放客户端句柄占用的内存。

```c
void mqtt_client_free(void *client);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `client` | `void*` | 客户端句柄 |

| 返回值 | 说明 |
|--------|------|
| `void` | 无返回值 |

> **注意**: 调用后 `client` 指针失效，不可再使用。建议调用后置为 `NULL`。

---

### 5.11 `mqtt_client_is_connected`

查询客户端当前是否处于已连接状态。

```c
int mqtt_client_is_connected(void *client);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `client` | `void*` | 客户端句柄 |

| 返回值 | 说明 |
|--------|------|
| `1` | 已连接 |
| `0` | 未连接 |

---

## 6. 典型使用流程

### 6.1 初始化与连接

```
┌─────────────────┐
│  mqtt_client_new │  ← 分配客户端
└────────┬────────┘
         ↓
┌─────────────────┐
│  mqtt_client_init│  ← 设置 Broker、回调
└────────┬────────┘
         ↓
┌────────────────────┐
│ mqtt_client_connect │  ← 发起 TCP + MQTT 握手
└────────┬───────────┘
         ↓
┌─────────────────┐
│  mqtt_client_yield│  ← 处理 CONNACK，触发 on_connected
└────────┬────────┘
         ↓
    on_connected()  →  mqtt_client_subscribe()  ← 订阅主题
```

### 6.2 主循环事件处理

```
while (运行中) {
    mqtt_client_yield(client);    ← 非阻塞，处理收发
    // 其他业务逻辑
    sleep(100ms);
}
```

### 6.3 断开与销毁

```
┌────────────────────┐
│ mqtt_client_disconnect│  ← 发送 DISCONNECT
└────────┬───────────┘
         ↓
┌─────────────────┐
│ mqtt_client_deinit │  ← 释放内部资源
└────────┬────────┘
         ↓
┌───────────────┐
│ mqtt_client_free │  ← 释放内存
└───────────────┘
```

---

## 7. 完整示例代码

### 7.1 基础连接与订阅示例

```c
#include "mqtt_client_interface.h"
#include <string.h>

#define MQTT_HOST       "192.168.1.100"
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "my_device_001"
#define MQTT_TOPIC      "device/status"
#define MQTT_QOS        1
#define MQTT_KEEPALIVE  60
#define MQTT_TIMEOUT_MS 5000

static void *g_client = NULL;

/* ── 回调函数 ─────────────────────────────── */

static void on_connected(void *client, void *userdata)
{
    PR_NOTICE("[mqtt] 已连接，订阅主题: %s", MQTT_TOPIC);
    uint16_t msgid = mqtt_client_subscribe(client, MQTT_TOPIC, MQTT_QOS);
    if (msgid > 0) {
        PR_NOTICE("[mqtt] 订阅请求已发出, msgid=%u", msgid);
    }
}

static void on_disconnected(void *client, void *userdata)
{
    PR_ERR("[mqtt] 连接断开");
    /* 注意: 不要在此处销毁客户端 */
}

static void on_message(void *client, uint16_t msgid,
                        const mqtt_client_message_t *msg, void *userdata)
{
    PR_NOTICE("[mqtt] 收到消息: topic=%s len=%u",
              msg->topic, (unsigned)msg->length);

    /* 安全地将 payload 转为字符串 */
    char buf[1024];
    size_t copy_len = msg->length < sizeof(buf) - 1
                      ? msg->length : sizeof(buf) - 1;
    memcpy(buf, msg->payload, copy_len);
    buf[copy_len] = '\0';

    PR_NOTICE("[mqtt] payload: %s", buf);
}

static void on_subscribed(void *client, uint16_t msgid, void *userdata)
{
    PR_NOTICE("[mqtt] 订阅确认, msgid=%u", msgid);
}

/* ── 初始化与连接 ─────────────────────────── */

int mqtt_app_init(const char *host, uint16_t port)
{
    /* 1. 创建客户端 */
    g_client = mqtt_client_new();
    if (g_client == NULL) {
        PR_ERR("[mqtt] 创建客户端失败");
        return -1;
    }

    /* 2. 配置 */
    mqtt_client_config_t config = {
        .cacert         = NULL,
        .cacert_len     = 0,
        .host           = host,
        .port           = port,
        .keepalive      = MQTT_KEEPALIVE,
        .timeout_ms     = MQTT_TIMEOUT_MS,
        .clientid       = MQTT_CLIENT_ID,
        .username       = NULL,
        .password       = NULL,
        .userdata       = NULL,
        .on_connected   = on_connected,
        .on_disconnected= on_disconnected,
        .on_message     = on_message,
        .on_published   = NULL,
        .on_subscribed  = on_subscribed,
        .on_unsubscribed= NULL,
    };

    /* 3. 初始化 */
    mqtt_client_status_t status = mqtt_client_init(g_client, &config);
    if (status != MQTT_STATUS_SUCCESS) {
        PR_ERR("[mqtt] 初始化失败: %d", status);
        mqtt_client_free(g_client);
        g_client = NULL;
        return -1;
    }

    /* 4. 连接 */
    status = mqtt_client_connect(g_client);
    if (status != MQTT_STATUS_SUCCESS) {
        PR_ERR("[mqtt] 连接失败: %d", status);
        mqtt_client_deinit(g_client);
        mqtt_client_free(g_client);
        g_client = NULL;
        return -1;
    }

    /* 5. yield 处理 CONNACK */
    for (int i = 0; i < 10; i++) {
        mqtt_client_yield(g_client);
        /* 检查 on_connected 是否已触发（通过全局标志） */
        sleep_ms(200);
    }

    return 0;
}

/* ── 发布消息 ─────────────────────────────── */

int mqtt_app_publish(const char *topic, const char *json)
{
    if (g_client == NULL) return -1;

    uint16_t msgid = mqtt_client_publish(
        g_client,
        topic,
        json,
        strlen(json),
        1,    /* QoS 1 */
        0     /* 不保留 */
    );

    return (msgid > 0) ? 0 : -1;
}

/* ── 主循环 ───────────────────────────────── */

void mqtt_app_loop(void)
{
    if (g_client != NULL) {
        mqtt_client_yield(g_client);
    }
}

/* ── 清理 ─────────────────────────────────── */

void mqtt_app_cleanup(void)
{
    if (g_client != NULL) {
        mqtt_client_disconnect(g_client);
        mqtt_client_deinit(g_client);
        mqtt_client_free(g_client);
        g_client = NULL;
    }
}
```

### 7.2 TLS 安全连接示例

```c
/* PEM 格式 CA 证书（示例） */
static const char ca_cert[] = "-----BEGIN CERTIFICATE-----\n"
    "MIIBkTCB+wIJAL...\n"
    "-----END CERTIFICATE-----";

int mqtt_tls_connect(const char *host, uint16_t port)
{
    g_client = mqtt_client_new();
    if (g_client == NULL) return -1;

    mqtt_client_config_t config = {
        .cacert         = ca_cert,           // TLS CA 证书
        .cacert_len     = sizeof(ca_cert) - 1,
        .host           = host,
        .port           = port,              // 通常 8883
        .keepalive      = 60,
        .timeout_ms     = 10000,             // TLS 握手较慢，增加超时
        .clientid       = "tls_device_001",
        .username       = "device_user",
        .password       = "device_pass",
        .userdata       = NULL,
        .on_connected   = on_connected,
        .on_disconnected= on_disconnected,
        .on_message     = on_message,
        .on_published   = NULL,
        .on_subscribed  = on_subscribed,
        .on_unsubscribed= NULL,
    };

    mqtt_client_status_t status = mqtt_client_init(g_client, &config);
    if (status != MQTT_STATUS_SUCCESS) {
        mqtt_client_free(g_client);
        g_client = NULL;
        return -1;
    }

    status = mqtt_client_connect(g_client);
    if (status != MQTT_STATUS_SUCCESS) {
        mqtt_client_deinit(g_client);
        mqtt_client_free(g_client);
        g_client = NULL;
        return -1;
    }

    return 0;
}
```

### 7.3 断线重连示例（指数退避）

```c
#include "tkl_system.h"  /* tkl_system_sleep, tkl_system_get_millisecond */

#define RECONNECT_INITIAL_MS   1000
#define RECONNECT_MAX_MS       60000

static uint32_t g_backoff_ms = RECONNECT_INITIAL_MS;
static int      g_connected  = 0;

/* on_disconnected 回调中仅设置标志 */
static void on_disconnected_reconnect(void *client, void *userdata)
{
    g_connected = 0;
    PR_NOTICE("[mqtt] 断连，等待主循环重连");
}

void mqtt_reconnect_loop(const char *host, uint16_t port)
{
    while (1) {
        if (g_client != NULL && g_connected) {
            mqtt_client_yield(g_client);
            tkl_system_sleep(100);
            continue;
        }

        /* 执行重连 */
        PR_NOTICE("[mqtt] 尝试重连 (backoff=%u ms)", (unsigned)g_backoff_ms);

        if (g_client != NULL) {
            mqtt_client_disconnect(g_client);
            mqtt_client_deinit(g_client);
            mqtt_client_free(g_client);
            g_client = NULL;
        }

        if (mqtt_app_init(host, port) == 0) {
            PR_NOTICE("[mqtt] 重连成功");
            g_connected = 1;
            g_backoff_ms = RECONNECT_INITIAL_MS;  /* 重置退避 */
        } else {
            PR_ERR("[mqtt] 重连失败，%u ms 后重试", (unsigned)g_backoff_ms);
            tkl_system_sleep(g_backoff_ms);

            /* 指数退避 */
            g_backoff_ms *= 2;
            if (g_backoff_ms > RECONNECT_MAX_MS) {
                g_backoff_ms = RECONNECT_MAX_MS;
            }
        }
    }
}
```

### 7.4 主题通配符订阅示例

```c
static void on_connected_wildcard(void *client, void *userdata)
{
    /* 订阅设备所有状态主题 */
    mqtt_client_subscribe(client, "device/+/status", 0);

    /* 订阅所有以 device/ 开头的主题 */
    mqtt_client_subscribe(client, "device/#", 1);
}
```

---

## 8. 注意事项与最佳实践

### 8.1 生命周期管理

| 规则 | 说明 |
|------|------|
| 创建顺序 | `new` → `init` → `connect` → `yield` |
| 销毁顺序 | `disconnect` → `deinit` → `free` |
| 不可重复初始化 | 已 init 的客户端需要先 `deinit` 再重新 `init` |
| 置 NULL | `free` 后将指针置为 `NULL`，防止野指针 |

### 8.2 回调安全

| 规则 | 说明 |
|------|------|
| 不要在回调中销毁客户端 | `on_disconnected` 可能在 `yield()` 内部触发，销毁会导致 use-after-free |
| 回调中避免阻塞 | 回调在 `yield()` 上下文中执行，阻塞会卡住整个消息循环 |
| payload 不以 `\0` 结尾 | `on_message` 中的 `msg->payload` 需手动复制并添加终止符 |
| LVGL 线程安全 | 如需在回调中操作 UI，应设置标志位，在主循环中加锁操作 |

### 8.3 网络与性能

| 建议 | 说明 |
|------|------|
| yield 调用频率 | 每 100~200ms 调用一次，确保心跳和消息及时处理 |
| QoS 选择 | 实时数据用 QoS 0，重要指令用 QoS 1 |
| keepalive 设置 | 推荐 60 秒，网络不稳定时可适当增大 |
| 连接超时 | 局域网 5000ms，公网/TLS 可设 10000~30000ms |
| 重连退避 | 使用指数退避 + 随机抖动，避免重连风暴 |

### 8.4 内存与资源

| 建议 | 说明 |
|------|------|
| 嵌入式设备 | 注意栈深度，`yield()` 和回调共用调用栈 |
| 消息缓冲区 | `on_message` 回调中的缓冲区应在回调返回前完成处理 |
| PSRAM 使用 | 大 JSON 解析建议使用 PSRAM 分配（如 T5 平台的 `tal_psram_malloc`） |

---

## 附录: API 速查表

| 函数 | 签名 | 返回值 |
|------|------|--------|
| `mqtt_client_new` | `void* mqtt_client_new(void)` | 句柄 / `NULL` |
| `mqtt_client_init` | `mqtt_client_status_t mqtt_client_init(void*, mqtt_client_config_t*)` | 状态码 |
| `mqtt_client_connect` | `mqtt_client_status_t mqtt_client_connect(void*)` | 状态码 |
| `mqtt_client_yield` | `void mqtt_client_yield(void*)` | void |
| `mqtt_client_subscribe` | `uint16_t mqtt_client_subscribe(void*, const char*, int)` | msgid / 0 |
| `mqtt_client_unsubscribe` | `uint16_t mqtt_client_unsubscribe(void*, const char*)` | msgid / 0 |
| `mqtt_client_publish` | `uint16_t mqtt_client_publish(void*, const char*, const void*, uint32_t, int, int)` | msgid / 0 |
| `mqtt_client_disconnect` | `mqtt_client_status_t mqtt_client_disconnect(void*)` | 状态码 |
| `mqtt_client_deinit` | `void mqtt_client_deinit(void*)` | void |
| `mqtt_client_free` | `void mqtt_client_free(void*)` | void |
| `mqtt_client_is_connected` | `int mqtt_client_is_connected(void*)` | 1/0 |

---

*本文档基于 TuyaOpen libmqtt 源码分析生成，覆盖 `mqtt_client_interface.h` 中的全部公开 API。*
