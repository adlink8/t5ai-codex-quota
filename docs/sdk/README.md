# TuyaOpen SDK API 参考文档

本目录包含 TuyaOpen SDK 的详细 API 参考文档，供 AI 助手和开发者使用。

## 文档列表

| 文件 | 内容 | 大小 |
|------|------|------|
| [tal_wifi_api_reference.md](tal_wifi_api_reference.md) | WiFi API - 扫描、连接、状态查询、管理帧等 | 27KB |
| [TuyaOpen_libmqtt_API_Reference.md](TuyaOpen_libmqtt_API_Reference.md) | MQTT API - 连接、订阅、发布、回调、重连 | 28KB |
| [TuyaOpen_System_API_Reference_CN.md](TuyaOpen_System_API_Reference_CN.md) | 系统 API - 日志、内存、线程、定时器、休眠 | 31KB |

## 快速参考

### WiFi API (tal_wifi.h)
```c
#include "tal_wifi.h"

// 初始化
tal_wifi_init(callback);

// 扫描
tal_wifi_all_ap_scan(&aps, &num);
tal_wifi_assign_ap_scan(ssid, &ap);

// 连接
tal_wifi_station_connect(ssid, password);

// 状态
tal_wifi_station_get_status(&stat);
tal_wifi_get_ip(WF_STATION, &ip);
tal_wifi_station_get_conn_ap_rssi(&rssi);
```

### MQTT API (mqtt_client_interface.h)
```c
#include "mqtt_client_interface.h"

// 创建客户端
void *client = mqtt_client_new();

// 配置
mqtt_client_config_t config = {
    .host = "broker.example.com",
    .port = 1883,
    .clientid = "my_device",
    .on_connected = my_connected_cb,
    .on_message = my_message_cb,
};
mqtt_client_init(client, &config);

// 连接
mqtt_client_connect(client);

// 订阅
mqtt_client_subscribe(client, "topic", 0);

// 主循环
while (1) {
    mqtt_client_yield(client);
}
```

### 系统 API
```c
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_thread.h"
#include "tal_system.h"

// 日志
PR_NOTICE("value=%d", 42);
PR_ERR("error occurred");

// 内存
void *p = tal_malloc(1024);
tal_free(p);

// 线程
THREAD_HANDLE th;
THREAD_CFG_T cfg = {.stackDepth = 4096, .priority = 5};
tal_thread_create_and_start(&th, NULL, NULL, my_task, NULL, &cfg);

// 休眠
tal_system_sleep(1000);  // 1秒

// 随机数
uint32_t rand = tkl_system_get_random(100);  // 0-99
```

## 项目使用示例

参考项目源码：
- `codex_quota_t5/src/codex_mqtt.c` - MQTT 客户端实现
- `codex_quota_t5/src/codex_http.c` - HTTP 客户端实现
- `codex_quota_t5/src/codex_ui.c` - LVGL UI 实现
- `codex_quota_t5/src/tuya_main.c` - 主程序框架

## 构建环境

- SDK: TuyaOpen @ `~/TuyaOpen` (WSL Ubuntu-D)
- 平台: T5AI (BK7258, ARM Cortex-M33F @480MHz)
- 工具链: gcc-arm-none-eabi-10.3-2021.10
- 构建: `python3 ../../tos.py build`

---
*生成日期: 2026-06-10*
