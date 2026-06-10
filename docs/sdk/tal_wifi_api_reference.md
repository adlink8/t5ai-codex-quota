# TuyaOpen WiFi API 参考文档 (tal_wifi.h)

> **源文件**: `src/tal_wifi/include/tal_wifi.h`
> **版权**: Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
> **依赖头文件**: `tuya_cloud_types.h`, `tkl_wifi.h`

本文档提供 TuyaOpen WiFi 抽象层的完整 API 参考，涵盖所有函数签名、参数说明、返回值、枚举类型、结构体定义及使用示例。

---

## 目录

1. [宏定义](#1-宏定义)
2. [枚举类型](#2-枚举类型)
3. [结构体定义](#3-结构体定义)
4. [函数 API](#4-函数-api)
   - [4.1 初始化与事件回调](#41-初始化与事件回调)
   - [4.2 AP 扫描](#42-ap-扫描)
   - [4.3 信道管理](#43-信道管理)
   - [4.4 Sniffer 模式](#44-sniffer-模式)
   - [4.5 IP 地址管理](#45-ip-地址管理)
   - [4.6 MAC 地址管理](#46-mac-地址管理)
   - [4.7 工作模式管理](#47-工作模式管理)
   - [4.8 SoftAP 管理](#48-softap-管理)
   - [4.9 Station 连接管理](#49-station-连接管理)
   - [4.10 连接状态查询](#410-连接状态查询)
   - [4.11 国家码设置](#411-国家码设置)
   - [4.12 管理帧收发](#412-管理帧收发)
   - [4.13 低功耗管理](#413-低功耗管理)
   - [4.14 RF 校准](#414-rf-校准)
   - [4.15 通用控制](#415-通用控制)
5. [使用示例](#5-使用示例)

---

## 1. 宏定义

### 帧标签与协议常量

| 宏名 | 值 | 说明 |
|---|---|---|
| `TAG_SSID_NUMBER` | `0` | SSID 标签编号 |
| `TAG_PAYLOAD_NUMBER` | `221` | Payload 标签编号 (Vendor Specific) |
| `PROBE_REQUEST_TYPE_SUBTYPE` | `0x0040` | Probe Request 帧类型子类型 |
| `PROBE_REQSPONSE_TYPE_SUBTYPE` | `0x0050` | Probe Response 帧类型子类型 |
| `PROBE_REQUEST_DURATION_ID` | `0x0` | Probe Request Duration ID |
| `PROBE_RESPONSET_DURATION_ID` | `0x0` | Probe Response Duration ID |
| `PROBE_REQUEST_PAYLOAD_LEN_MAX` | `255` | Probe Request 负载最大长度 |
| `BROADCAST_MAC_ADDR` | `0xFFFFFFFF` | 广播 MAC 地址 |

### To/From DS 标志

| 宏名 | 值 | 说明 |
|---|---|---|
| `TO_FROM_DS_MASK` | `0x03` | To/From DS 掩码 |
| `TFD_IBSS` | `0x00` | IBSS 模式 (da+sa+bssid) |
| `TFD_TO_AP` | `0x01` | 发送到 AP (bssid+sa+da) |
| `TFD_FROM_AP` | `0x02` | 来自 AP (dst+bssid+sa) |
| `TFD_WDS` | `0x03` | WDS 模式 (ra+ta+da) |

### 广播信道

| 宏名 | 值 | 说明 |
|---|---|---|
| `BC_TO_AP` | `0` | 到 AP 的广播信道 |
| `BC_FROM_AP` | `1` | 来自 AP 的广播信道 |
| `BC_CHAN_NUM` | `2` | 广播信道总数 |

---

## 2. 枚举类型

### 2.1 MIMO_TYPE_E — MIMO 类型

WiFi 芯片检测到的本地 AP 信息中的 MIMO 类型。

```c
typedef enum {
    MIMO_TYPE_NORMAL = 0,  // 普通模式
    MIMO_TYPE_HT40,        // HT40 模式 (40MHz 带宽)
    MIMO_TYPE_2X2,         // 2x2 MIMO 模式
    MIMO_TYPE_LDPC,        // LDPC 编码模式
    MIMO_TYPE_NUM,         // MIMO 类型总数 (不可用作有效类型)
} MIMO_TYPE_E;
```

### 2.2 WLAN_FRM_TP_E — WLAN 帧类型

802.11 帧类型标识，用于 sniffer 模式下的帧分类。

```c
typedef enum {
    WFT_PROBE_REQ   = 0x40,  // Probe Request 帧
    WFT_PROBE_RSP   = 0x50,  // Probe Response 帧
    WFT_AUTH        = 0xb0,  // Authentication 帧
    WFT_BEACON      = 0x80,  // Beacon 帧
    WFT_DATA        = 0x08,  // 数据帧
    WFT_QOS_DATA    = 0x88,  // QoS 数据帧
    WFT_MIMO_DATA   = 0xff,  // MIMO 数据帧
} WLAN_FRM_TP_E;
```

### 2.3 BC_DA_CHAN_T — 广播信道类型

```c
typedef unsigned char BC_DA_CHAN_T;
// BC_TO_AP   = 0  → 到 AP 的广播信道
// BC_FROM_AP = 1  → 来自 AP 的广播信道
// BC_CHAN_NUM = 2  → 广播信道总数
```

### 2.4 外部依赖枚举（来自 tkl_wifi.h）

以下枚举类型在 `tkl_wifi.h` 中定义，被 `tal_wifi.h` API 使用：

- **`WF_IF_E`** — WiFi 接口类型（Station / AP）
- **`WF_WK_MD_E`** — WiFi 工作模式（关闭 / Station / AP / Sniffer / Station+AP）
- **`WF_STATION_STAT_E`** — Station 连接状态（未连接 / 连接中 / 连接成功 / 密码错误 / 找不到 AP 等）
- **`WF_IOCTL_CMD_E`** — ioctl 控制命令

---

## 3. 结构体定义

### 3.1 MIMO_IF_S — MIMO 信息结构体

WiFi 芯片检测到的本地 AP 信息。

```c
typedef struct {
    signed char   rssi;      // 信号强度 (dBm)
    MIMO_TYPE_E   type;      // MIMO 类型
    unsigned short len;      // 数据包长度
    unsigned char  channel;  // 信道号
    unsigned char  mcs;      // MCS 索引 (调制编码方案)
} MIMO_IF_S;
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `rssi` | `signed char` | 接收信号强度，单位 dBm |
| `type` | `MIMO_TYPE_E` | MIMO 类型 |
| `len` | `unsigned short` | 数据包长度 |
| `channel` | `unsigned char` | 工作信道 |
| `mcs` | `unsigned char` | MCS (Modulation and Coding Scheme) 索引 |

### 3.2 WLAN_MANAGEMENT_S — 802.11 管理帧标签

```c
#pragma pack(1)
typedef struct {
    unsigned char id;    // 标签 ID
    unsigned char len;   // 标签数据长度
    char data[0];        // 标签数据 (柔性数组)
} WLAN_MANAGEMENT_S;
#pragma pack()
```

### 3.3 WLAN_PROBE_REQ_IF_S — Probe Request 帧信息

```c
#pragma pack(1)
typedef struct {
    unsigned char  frame_type;       // 帧类型
    unsigned char  frame_ctrl_flags; // 帧控制标志
    unsigned short duration;         // 持续时间
    unsigned char  dest[6];          // 目标 MAC 地址
    unsigned char  src[6];           // 源 MAC 地址
    unsigned char  bssid[6];         // BSSID MAC 地址
    unsigned short seq_frag_num;     // 序列号和分片号
} WLAN_PROBE_REQ_IF_S;
#pragma pack()
```

### 3.4 WLAN_BEACON_IF_S — Beacon 帧信息

```c
#pragma pack(1)
typedef struct {
    unsigned char  frame_ctrl_flags; // 帧控制标志
    unsigned short duration;         // 持续时间
    unsigned char  dest[6];          // 目标 MAC 地址
    unsigned char  src[6];           // 源 MAC 地址
    unsigned char  bssid[6];         // BSSID MAC 地址
    unsigned short seq_frag_num;     // 序列号和分片号
    unsigned char  timestamp[8];     // 时间戳
    unsigned short beacon_interval;  // Beacon 间隔 (TU)
    unsigned short cap_info;         // 能力信息
    unsigned char  ssid_element_id;  // SSID 元素 ID
    unsigned char  ssid_len;         // SSID 长度
    char           ssid[0];          // SSID 字符串 (柔性数组)
} WLAN_BEACON_IF_S;
#pragma pack()
```

### 3.5 WLAN_DATA_IF_S — 数据帧信息

```c
#pragma pack(1)
typedef struct {
    unsigned char  frame_ctrl_flags; // 帧控制标志
    unsigned short duration;         // 持续时间
    WLAN_ADDR_U    addr;             // 地址 (联合体)
    unsigned short seq_frag_num;     // 序列号和分片号
    unsigned short qos_ctrl;         // QoS 控制位
} WLAN_DATA_IF_S;
#pragma pack()
```

### 3.6 地址结构体

```c
// 通用地址结构
typedef struct {
    unsigned char addr1[6];  // 地址1
    unsigned char addr2[6];  // 地址2
    unsigned char addr3[6];  // 地址3
} WLAN_COM_ADDR_S;

// To-AP 地址结构 (bssid+sa+da)
typedef struct {
    unsigned char bssid[6];  // BSSID
    unsigned char src[6];    // 源地址
    unsigned char dst[6];    // 目标地址
} WLAN_TO_AP_ADDR_S;

// From-AP 地址结构 (dst+bssid+sa)
typedef struct {
    unsigned char dst[6];    // 目标地址
    unsigned char bssid[6];  // BSSID
    unsigned char src[6];    // 源地址
} WLAN_FROM_AP_ADDR_S;

// 地址联合体
typedef union {
    WLAN_COM_ADDR_S     com;      // 通用地址
    WLAN_TO_AP_ADDR_S   to_ap;    // 到 AP 地址
    WLAN_FROM_AP_ADDR_S from_ap;  // 来自 AP 地址
} WLAN_ADDR_U;
```

### 3.7 WLAN_FRAME_S — WLAN 帧信息（聚合）

```c
typedef struct {
    unsigned char frame_type;  // WLAN 帧类型 (WLAN_FRM_TP_E)
    union {
        WLAN_BEACON_IF_S beacon_info;  // Beacon 帧信息
        WLAN_DATA_IF_S   data_info;    // 数据帧信息
        MIMO_IF_S        mimo_info;    // MIMO 信息
    } frame_data;
} WLAN_FRAME_S, *P_WLAN_FRAME_S;
```

### 3.8 Probe 帧相关结构体

```c
#pragma pack(1)
// Probe Request 包头
typedef struct {
    unsigned short type_and_subtype;  // 类型和子类型
    unsigned short duration_id;       // 持续时间 ID
    unsigned char  addr1[6];          // 接收端地址
    unsigned char  addr2[6];          // 发送端地址
    unsigned char  addr3[6];          // BSSID
    unsigned short seq_ctrl;          // 序列控制
} PROBE_REQUEST_PACKAGE_HEAD_S;

// Probe Response 包头
typedef struct {
    uint16_t type_and_subtype;  // 类型和子类型
    uint16_t duration_id;       // 持续时间 ID
    uint8_t  addr1[6];          // 接收端地址
    uint8_t  addr2[6];          // 发送端地址
    uint8_t  addr3[6];          // BSSID
    uint16_t seq_ctrl;          // 序列控制
    uint8_t  timestamp[8];      // 时间戳
    uint16_t beacon_interval;   // Beacon 间隔
    uint16_t cap_info;          // 能力信息
} PROBE_RESPONSE_PACKAGE_HEAD_S;

// Beacon 标签数据单元
typedef struct {
    unsigned char index;  // 标签索引
    unsigned char len;    // 标签数据长度
    unsigned char ptr[0]; // 标签数据 (柔性数组)
} BEACON_TAG_DATA_UNIT_S;
#pragma pack()

// Probe Request 固定结构
typedef struct {
    PROBE_REQUEST_PACKAGE_HEAD_S pack_head;  // 包头
    BEACON_TAG_DATA_UNIT_S       tag_ssid;   // SSID 标签
} PROBE_REQUEST_FIX_S;
```

---

## 4. 函数 API

### 4.1 初始化与事件回调

#### `tal_wifi_init`

初始化 WiFi 模块并注册 Station 事件回调。

```c
OPERATE_RET tal_wifi_init(WIFI_EVENT_CB cb);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `cb` | in | `WIFI_EVENT_CB` | WiFi Station 事件变化回调函数 |

**返回值**: `OPRT_OK` 成功；其他值失败，参见 `tuya_error_code.h`

---

### 4.2 AP 扫描

#### `tal_wifi_all_ap_scan`

扫描当前环境中所有 AP 信息。

```c
OPERATE_RET tal_wifi_all_ap_scan(AP_IF_S **ap_ary, uint32_t *num);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `ap_ary` | out | `AP_IF_S **` | AP 信息数组指针（函数内部分配内存） |
| `num` | out | `uint32_t *` | AP 数量 |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_assign_ap_scan`

扫描指定 SSID 的 AP 信息。

```c
OPERATE_RET tal_wifi_assign_ap_scan(int8_t *ssid, AP_IF_S **ap);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `ssid` | in | `int8_t *` | 要扫描的目标 SSID |
| `ap` | out | `AP_IF_S **` | 找到的 AP 信息（函数内部分配内存） |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_release_ap`

释放 AP 扫描函数分配的内存。

```c
OPERATE_RET tal_wifi_release_ap(AP_IF_S *ap);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `ap` | in | `AP_IF_S *` | 要释放的 AP 信息指针 |

**返回值**: `OPRT_OK` 成功；其他值失败

> **注意**: 调用 `tal_wifi_all_ap_scan` 或 `tal_wifi_assign_ap_scan` 后，当 AP 信息不再需要时，必须调用此函数释放内存。

---

### 4.3 信道管理

#### `tal_wifi_set_cur_channel`

设置 WiFi 工作信道。

```c
OPERATE_RET tal_wifi_set_cur_channel(uint8_t chan);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `chan` | in | `uint8_t` | 要设置的信道号 (1-13) |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_get_cur_channel`

获取当前 WiFi 工作信道。

```c
OPERATE_RET tal_wifi_get_cur_channel(uint8_t *chan);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `chan` | out | `uint8_t *` | 当前工作信道号 |

**返回值**: `OPRT_OK` 成功；其他值失败

---

### 4.4 Sniffer 模式

#### `tal_wifi_sniffer_set`

启用或禁用 WiFi Sniffer 模式。启用后，WiFi 从空中接收数据包，用户需通过回调 `cb` 将这些数据包发送给 Tuya SDK。

```c
OPERATE_RET tal_wifi_sniffer_set(BOOL_T en, SNIFFER_CALLBACK cb);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `en` | in | `BOOL_T` | `TRUE` 启用 Sniffer，`FALSE` 禁用 |
| `cb` | in | `SNIFFER_CALLBACK` | 收到数据包时的通知回调 |

**返回值**: `OPRT_OK` 成功；其他值失败

---

### 4.5 IP 地址管理

#### `tal_wifi_get_ip`

获取 WiFi 接口 IP 地址信息。当 WiFi 工作在 AP+Station 模式时，有两个 IP 地址。

```c
OPERATE_RET tal_wifi_get_ip(WF_IF_E wf, NW_IP_S *ip);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `wf` | in | `WF_IF_E` | WiFi 接口类型（Station 或 AP） |
| `ip` | out | `NW_IP_S *` | IP 地址信息（包含 IP、子网掩码、网关） |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_set_ip`

设置 WiFi 接口 IP 地址信息。

```c
OPERATE_RET tal_wifi_set_ip(WF_IF_E wf, NW_IP_S *ip);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `wf` | in | `WF_IF_E` | WiFi 接口类型（Station 或 AP） |
| `ip` | in | `NW_IP_S *` | 要设置的 IP 地址信息 |

**返回值**: `OPRT_OK` 成功；其他值失败

---

### 4.6 MAC 地址管理

#### `tal_wifi_set_mac`

设置 WiFi 接口 MAC 地址。当 WiFi 工作在 AP+Station 模式时，有两个 MAC 地址。

```c
OPERATE_RET tal_wifi_set_mac(WF_IF_E wf, NW_MAC_S *mac);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `wf` | in | `WF_IF_E` | WiFi 接口类型（Station 或 AP） |
| `mac` | in | `NW_MAC_S *` | 要设置的 MAC 地址 |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_get_mac`

获取 WiFi 接口 MAC 地址。

```c
OPERATE_RET tal_wifi_get_mac(WF_IF_E wf, NW_MAC_S *mac);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `wf` | in | `WF_IF_E` | WiFi 接口类型（Station 或 AP） |
| `mac` | out | `NW_MAC_S *` | 当前 MAC 地址 |

**返回值**: `OPRT_OK` 成功；其他值失败

---

### 4.7 工作模式管理

#### `tal_wifi_set_work_mode`

设置 WiFi 工作模式。

```c
OPERATE_RET tal_wifi_set_work_mode(WF_WK_MD_E mode);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `mode` | in | `WF_WK_MD_E` | WiFi 工作模式 |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_get_work_mode`

获取当前 WiFi 工作模式。

```c
OPERATE_RET tal_wifi_get_work_mode(WF_WK_MD_E *mode);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `mode` | out | `WF_WK_MD_E *` | 当前 WiFi 工作模式 |

**返回值**: `OPRT_OK` 成功；其他值失败

---

### 4.8 SoftAP 管理

#### `tal_wifi_ap_start`

启动 SoftAP 热点。

```c
OPERATE_RET tal_wifi_ap_start(WF_AP_CFG_IF_S *cfg);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `cfg` | in | `WF_AP_CFG_IF_S *` | SoftAP 配置信息（SSID、密码、信道等） |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_ap_stop`

停止 SoftAP 热点。

```c
OPERATE_RET tal_wifi_ap_stop(void);
```

**参数**: 无

**返回值**: `OPRT_OK` 成功；其他值失败

---

### 4.9 Station 连接管理

#### `tal_wifi_station_connect`

使用 SSID 和密码连接 WiFi。

```c
OPERATE_RET tal_wifi_station_connect(int8_t *ssid, int8_t *passwd);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `ssid` | in | `int8_t *` | 目标 AP 的 SSID |
| `passwd` | in | `int8_t *` | 目标 AP 的密码 |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_station_disconnect`

断开当前 WiFi 连接。

```c
OPERATE_RET tal_wifi_station_disconnect(void);
```

**参数**: 无

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_get_connected_ap_info`

获取当前已连接 AP 的信息，用于快速重连。

```c
OPERATE_RET tal_wifi_get_connected_ap_info(FAST_WF_CONNECTED_AP_INFO_T **fast_ap_info);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `fast_ap_info` | out | `FAST_WF_CONNECTED_AP_INFO_T **` | 快速连接 AP 信息 |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_fast_station_connect`

使用快速连接信息连接 WiFi。

```c
OPERATE_RET tal_fast_station_connect(FAST_WF_CONNECTED_AP_INFO_T *fast_ap_info);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `fast_ap_info` | in | `FAST_WF_CONNECTED_AP_INFO_T *` | 快速连接 AP 信息（从 `tal_wifi_get_connected_ap_info` 获取） |

**返回值**: `OPRT_OK` 成功；其他值失败

---

### 4.10 连接状态查询

#### `tal_wifi_station_get_conn_ap_rssi`

获取当前连接 AP 的信号强度。

```c
OPERATE_RET tal_wifi_station_get_conn_ap_rssi(int8_t *rssi);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `rssi` | out | `int8_t *` | RSSI 值 (dBm) |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_get_bssid`

获取当前连接 AP 的 BSSID。

```c
OPERATE_RET tal_wifi_get_bssid(uint8_t *mac);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `mac` | out | `uint8_t *` | BSSID MAC 地址 (6 字节) |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_station_get_status`

获取 WiFi Station 工作状态。

```c
OPERATE_RET tal_wifi_station_get_status(WF_STATION_STAT_E *stat);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `stat` | out | `WF_STATION_STAT_E *` | Station 当前状态 |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_station_get_err_stat`

获取 WiFi Station 错误状态（连接失败时的详细错误码）。

```c
OPERATE_RET tal_wifi_station_get_err_stat(WF_STATION_STAT_E *stat);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `stat` | out | `WF_STATION_STAT_E *` | Station 错误状态 |

**返回值**: `OPRT_OK` 成功；其他值失败

---

### 4.11 国家码设置

#### `tal_wifi_set_country_code`

设置 WiFi 国家码，影响可用信道范围和发射功率。

```c
OPERATE_RET tal_wifi_set_country_code(char *country_code);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `country_code` | in | `char *` | 国家码字符串，如 `"CN"`, `"US"`, `"JP"` |

**返回值**: `OPRT_OK` 成功；其他值失败

---

### 4.12 管理帧收发

#### `tal_wifi_send_mgnt`

发送 WiFi 管理帧。

```c
OPERATE_RET tal_wifi_send_mgnt(uint8_t *buf, uint32_t len);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `buf` | in | `uint8_t *` | 管理帧数据缓冲区 |
| `len` | in | `uint32_t` | 缓冲区长度 |

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_register_recv_mgnt_callback`

注册接收 WiFi 管理帧的回调。

```c
OPERATE_RET tal_wifi_register_recv_mgnt_callback(BOOL_T enable, WIFI_REV_MGNT_CB recv_cb);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `enable` | in | `BOOL_T` | `TRUE` 启用接收，`FALSE` 禁用 |
| `recv_cb` | in | `WIFI_REV_MGNT_CB` | 收到管理帧时的回调函数 |

**返回值**: `OPRT_OK` 成功；其他值失败

---

### 4.13 低功耗管理

#### `tal_wifi_lp_enable`

启用 WiFi 低功耗模式。

```c
OPERATE_RET tal_wifi_lp_enable(void);
```

**参数**: 无

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_lp_disable`

禁用 WiFi 低功耗模式。

```c
OPERATE_RET tal_wifi_lp_disable(void);
```

**参数**: 无

**返回值**: `OPRT_OK` 成功；其他值失败

#### `tal_wifi_set_lps_dtim`

设置 WiFi 低功耗模式的 DTIM 间隔。需在进入低功耗模式前调用。

```c
void tal_wifi_set_lps_dtim(uint32_t dtim);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `dtim` | in | `uint32_t` | DTIM 间隔值 |

**返回值**: 无 (void)

> **注意**: 此函数无返回值，应在进入低功耗模式前调用。

---

### 4.14 RF 校准

#### `tal_wifi_rf_calibrated`

执行 WiFi RF 校准。通常在 WiFi 测试时调用。

```c
BOOL_T tal_wifi_rf_calibrated(void);
```

**参数**: 无

**返回值**: `TRUE` 校准成功；`FALSE` 校准失败

---

### 4.15 通用控制

#### `tal_wifi_ioctl`

WiFi 通用控制接口，用于执行各种扩展命令。

```c
OPERATE_RET tal_wifi_ioctl(WF_IOCTL_CMD_E cmd, void *args);
```

| 参数 | 方向 | 类型 | 说明 |
|---|---|---|---|
| `cmd` | in | `WF_IOCTL_CMD_E` | 控制命令 |
| `args` | in | `void *` | 与命令关联的参数 |

**返回值**: `OPRT_OK` 成功；其他值失败

---

## 5. 使用示例

### 5.1 WiFi 初始化与连接

```c
#include "tal_wifi.h"

// WiFi 事件回调
static void wifi_event_callback(WF_EVENT_E event)
{
    switch (event) {
    case WFE_CONNECTED:
        PR_DEBUG("WiFi connected");
        break;
    case WFE_DISCONNECTED:
        PR_DEBUG("WiFi disconnected");
        break;
    default:
        break;
    }
}

void wifi_init_and_connect(void)
{
    OPERATE_RET ret;

    // 1. 初始化 WiFi，注册事件回调
    ret = tal_wifi_init(wifi_event_callback);
    if (ret != OPRT_OK) {
        PR_ERR("WiFi init failed: %d", ret);
        return;
    }

    // 2. 设置工作模式为 Station
    ret = tal_wifi_set_work_mode(WWM_STATION);
    if (ret != OPRT_OK) {
        PR_ERR("Set work mode failed: %d", ret);
        return;
    }

    // 3. 连接到 AP
    int8_t ssid[] = "MyWiFi";
    int8_t passwd[] = "password123";
    ret = tal_wifi_station_connect(ssid, passwd);
    if (ret != OPRT_OK) {
        PR_ERR("Connect failed: %d", ret);
        return;
    }

    PR_DEBUG("WiFi connecting...");
}
```

### 5.2 AP 扫描与信息获取

```c
void scan_all_aps(void)
{
    AP_IF_S *ap_ary = NULL;
    uint32_t num = 0;

    OPERATE_RET ret = tal_wifi_all_ap_scan(&ap_ary, &num);
    if (ret != OPRT_OK) {
        PR_ERR("Scan failed: %d", ret);
        return;
    }

    PR_INFO("Found %u APs:", num);
    for (uint32_t i = 0; i < num; i++) {
        PR_INFO("  [%u] SSID: %s, RSSI: %d, Channel: %d",
                i, ap_ary[i].ssid, ap_ary[i].rssi, ap_ary[i].channel);
    }

    // 释放扫描结果内存
    tal_wifi_release_ap(ap_ary);
}

void scan_specific_ap(void)
{
    int8_t target_ssid[] = "TargetAP";
    AP_IF_S *ap = NULL;

    OPERATE_RET ret = tal_wifi_assign_ap_scan(target_ssid, &ap);
    if (ret != OPRT_OK) {
        PR_ERR("AP not found: %d", ret);
        return;
    }

    PR_INFO("Found: SSID=%s, RSSI=%d", ap->ssid, ap->rssi);

    tal_wifi_release_ap(ap);
}
```

### 5.3 启动 SoftAP 热点

```c
void start_softap(void)
{
    // 先设置工作模式为 AP
    tal_wifi_set_work_mode(WWM_SOFTAP);

    // 配置 AP 参数 (WF_AP_CFG_IF_S 结构体来自 tkl_wifi.h)
    WF_AP_CFG_IF_S ap_cfg = {0};
    // ... 填充 AP 配置（SSID、密码、信道等）

    OPERATE_RET ret = tal_wifi_ap_start(&ap_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("Start SoftAP failed: %d", ret);
        return;
    }

    PR_INFO("SoftAP started");
}
```

### 5.4 Sniffer 模式抓包

```c
static void sniffer_callback(unsigned char *buf,
                              unsigned short len,
                              signed char rssi)
{
    // 解析 802.11 帧
    if (len < sizeof(WLAN_PROBE_REQ_IF_S)) {
        return;
    }

    WLAN_FRAME_S *frame = (WLAN_FRAME_S *)buf;
    switch (frame->frame_type) {
    case WFT_PROBE_REQ:
        PR_INFO("Probe Request, RSSI: %d", rssi);
        break;
    case WFT_BEACON:
        PR_INFO("Beacon frame");
        break;
    case WFT_DATA:
    case WFT_QOS_DATA:
        PR_INFO("Data frame");
        break;
    default:
        break;
    }
}

void start_sniffer(void)
{
    OPERATE_RET ret = tal_wifi_sniffer_set(TRUE, sniffer_callback);
    if (ret != OPRT_OK) {
        PR_ERR("Enable sniffer failed: %d", ret);
    }
}

void stop_sniffer(void)
{
    tal_wifi_sniffer_set(FALSE, NULL);
}
```

### 5.5 获取连接状态与信号强度

```c
void check_wifi_status(void)
{
    WF_STATION_STAT_E stat;
    OPERATE_RET ret;

    ret = tal_wifi_station_get_status(&stat);
    if (ret == OPRT_OK) {
        PR_INFO("Station status: %d", stat);
    }

    // 获取信号强度
    int8_t rssi;
    ret = tal_wifi_station_get_conn_ap_rssi(&rssi);
    if (ret == OPRT_OK) {
        PR_INFO("RSSI: %d dBm", rssi);
    }

    // 获取 BSSID
    uint8_t bssid[6];
    ret = tal_wifi_get_bssid(bssid);
    if (ret == OPRT_OK) {
        PR_INFO("BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
                bssid[0], bssid[1], bssid[2],
                bssid[3], bssid[4], bssid[5]);
    }

    // 获取 IP 地址
    NW_IP_S ip;
    ret = tal_wifi_get_ip(WF_STATION, &ip);
    if (ret == OPRT_OK) {
        PR_INFO("IP: %s, Mask: %s, GW: %s",
                ip.ip, ip.mask, ip.gw);
    }

    // 获取 MAC 地址
    NW_MAC_S mac;
    ret = tal_wifi_get_mac(WF_STATION, &mac);
    if (ret == OPRT_OK) {
        PR_INFO("MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                mac.mac[0], mac.mac[1], mac.mac[2],
                mac.mac[3], mac.mac[4], mac.mac[5]);
    }
}
```

### 5.6 低功耗模式

```c
void enable_low_power(void)
{
    // 设置 DTIM 间隔
    tal_wifi_set_lps_dtim(3);

    // 启用低功耗
    OPERATE_RET ret = tal_wifi_lp_enable();
    if (ret != OPRT_OK) {
        PR_ERR("Enable low power failed: %d", ret);
    }
}

void disable_low_power(void)
{
    OPERATE_RET ret = tal_wifi_lp_disable();
    if (ret != OPRT_OK) {
        PR_ERR("Disable low power failed: %d", ret);
    }
}
```

### 5.7 管理帧收发

```c
static void mgnt_recv_callback(unsigned char *buf, int len)
{
    PR_INFO("Received management frame, len=%d", len);
    // 处理收到的管理帧
}

void setup_management_frame(void)
{
    // 注册管理帧接收回调
    OPERATE_RET ret = tal_wifi_register_recv_mgnt_callback(TRUE, mgnt_recv_callback);
    if (ret != OPRT_OK) {
        PR_ERR("Register mgnt callback failed: %d", ret);
    }
}

void send_custom_frame(uint8_t *frame_data, uint32_t len)
{
    OPERATE_RET ret = tal_wifi_send_mgnt(frame_data, len);
    if (ret != OPRT_OK) {
        PR_ERR("Send mgnt failed: %d", ret);
    }
}
```

### 5.8 快速重连

```c
void fast_reconnect(void)
{
    FAST_WF_CONNECTED_AP_INFO_T *fast_info = NULL;

    // 获取当前连接信息
    OPERATE_RET ret = tal_wifi_get_connected_ap_info(&fast_info);
    if (ret != OPRT_OK || fast_info == NULL) {
        PR_ERR("Get connected AP info failed");
        return;
    }

    // 保存 fast_info 用于后续快速重连...
    // 当需要重连时：
    ret = tal_fast_station_connect(fast_info);
    if (ret != OPRT_OK) {
        PR_ERR("Fast connect failed: %d", ret);
    }
}
```

---

## 错误码参考

所有函数返回 `OPERATE_RET` 类型，成功时返回 `OPRT_OK`。错误码定义在 `tuya_error_code.h` 中，常见错误码包括：

| 错误码 | 说明 |
|---|---|
| `OPRT_OK` | 操作成功 |
| `OPRT_COM_ERROR` | 通用错误 |
| `OPRT_INVALID_PARM` | 无效参数 |
| `OPRT_MALLOC_FAILED` | 内存分配失败 |
| `OPRT_NOT_SUPPORTED` | 不支持的操作 |
| `OPRT_TIMEOUT` | 操作超时 |

---

## 回调函数类型

以下回调类型在 `tkl_wifi.h` 或 `tuya_cloud_types.h` 中定义：

| 类型 | 说明 |
|---|---|
| `WIFI_EVENT_CB` | WiFi Station 事件回调 |
| `SNIFFER_CALLBACK` | Sniffer 模式数据包回调 |
| `WIFI_REV_MGNT_CB` | 管理帧接收回调 |
