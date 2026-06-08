# 涂鸦 IoT 开发调研报告

> 调研日期：2026-06-07
> 基于 iot-project-research 五步调研法

---

## 一、硬件能力摸底

### 1.1 涂鸦芯片平台总览

涂鸦提供两代 IoT 芯片，覆盖从基础智能插座到 AI 多媒体处理的全场景。

#### 经典 BK7231 系列（与博通集成联合开发）

| 芯片 | CPU | RAM | Flash | WiFi | 蓝牙 | 典型模组 | 备注 |
|------|-----|-----|-------|------|------|---------|------|
| BK7231N | ARM9EJ-S 120MHz | 256KB | 2MB | Wi-Fi 4 (802.11n) | BLE 5.1/5.2 | CB2S | 最主流型号，性价比之王 |
| BK7231T | 32-bit 120MHz | 256KB | 2MB | Wi-Fi 4 (802.11n) | BLE 4.2 | CBU | 蓝牙版本较旧 |
| BK7231U | 32-bit | — | — | Wi-Fi 4 | BLE 4.2 | — | 支持摄像头+音频（多媒体） |

#### 新一代 T 系列（涂鸦自研）

| 芯片 | CPU | RAM | Flash | WiFi | 蓝牙 | 特色 |
|------|-----|-----|-------|------|------|------|
| T2 | 32-bit RISC 120MHz | 256KB | 2MB | Wi-Fi 4 | BLE 5.1 | 入门级，18 GPIO |
| T3 | 32-bit 320MHz | 640KB | 4MB | Wi-Fi 6 (802.11ax) | BLE 5.4 | 工业级温度 -40~105°C |
| T5 | ARM Cortex-M33F 480MHz | 16.5MB (512KB+16MB PSRAM) | 12MB | Wi-Fi 6 | BLE 5.4 | AI 加速、摄像头、H.264、USB 2.0、CAN |

**注意：** T 系列中没有 T4，从 T3 直接跳到 T5。

### 1.2 开发板

| 开发板 | 芯片 | 音频 | 摄像头 | 显示屏 | AI | 适用场景 |
|--------|------|------|--------|--------|-----|---------|
| T2-U Board | T2 | 无 | 无 | 无 | 无 | 基础 IoT 原型 |
| T5AI-Board（旗舰） | T5 | 双麦克风+扬声器 | 支持 | LCD/RGB | 是 | AI 视觉+语音全功能开发 |
| T5AI-Core DevKit | T5 | 单麦克风+扬声器 | 无 | 无 | 是 | 语音助手/音频 AI 项目 |

T5AI-Board 是旗舰开发平台，使用 T5-E1-IPEX 模组，预装 TuyaOpen 框架支持。可通过淘宝涂鸦官方店购买。

### 1.3 硬件设计要点

- **RF 设计：** 50 欧姆阻抗 RF 走线，12mil 边缘间距
- **天线：** PCB 上需要独立天线净空区
- **供电：** 最低 3.3V / 220mA
- **GPIO：** P28 引脚必须下拉用于开机自校准
- **PCB：** CoB (Chip-on-Board) 设计，需遵循官方布局指南

---

## 二、平台与 SDK 调研

### 2.1 TuyaOS（官方 IoT 操作系统）

TuyaOS 是涂鸦自研的分布式跨平台 IoT 操作系统，5 层架构（Kernel → Abstraction → Libraries → Services → Application），可运行在 RTOS、Linux 或裸机之上。

**两种开发模式：**
- **TuyaOS SDK：** 全源码开发，灵活度最高
- **TuyaOS EasyGo：** 向导式套件开发，上手最快

**开发工具：**
- **Tuya Wind IDE** — VS Code 插件，集成框架管理、编译、调试、烧录。支持 Windows (WSL2)、Linux、macOS
- **tos.py** — CLI 工具，SDK 管理、编译、烧录
- **Tuya MiniApp IDE** — 开发智能设备控制面板（小程序）

### 2.2 TuyaOpen（开源 AI+IoT 框架）

- **GitHub：** https://github.com/tuya/TuyaOpen （1.6k+ Stars，Apache 2.0）
- **官网：** https://tuyaopen.ai
- **支持芯片：** T2/T3/T5、ESP32-C3/S3/C6、LN882H、BK7231N、树莓派、瑞芯微
- **AI 能力：** 端侧 ASR（语音识别）、KWS（唤醒词）、TTS、STT、LLM 集成（ChatGPT、Gemini）
- **构建系统：** C/C++ + CMake
- **理念：** "一次构建，跨芯片部署"
- **Arduino 支持：** https://github.com/tuya/arduino-TuyaOpen （358 Stars）

### 2.3 MCU SDK 开发（外部 MCU + 涂鸦通信模组）

适用场景：MCU 负责设备逻辑，涂鸦模组负责联网和云通信。

**开发流程：**
1. 登录 https://platform.tuya.com/ 创建产品
2. 配置 DP（数据点）— 定义设备能力
3. 选择云模组（WiFi/Zigbee/BLE）
4. 下载自动生成的 MCU SDK（支持 STM32、8051、Arduino）
5. 通过 UART 串口集成到 MCU
6. 测试 → 量产

**关键技术：**
- 通信方式：UART 串口（波特率 9600 或 115200）
- `wifi_protocol_init()` 必须在串口初始化之前调用
- Arduino 库：https://github.com/tuya/tuya-wifi-mcu-sdk-arduino-library

### 2.4 App SDK（移动端 App 开发）

- **智能生活 App SDK：** iOS / Android / HarmonyOS 原生开发
- **面板/小程序：** MiniApp IDE 自定义控制面板
- **注意：** React Native SDK 已停止维护

### 2.5 云开发 API

- **协议：** MQTT 3.1.1 / TLS 1.2 / JSON (TuyaLink)
- **认证：** 一机一密（每台设备独立证书）
- **QoS：** 0（发后即忘）和 1（至少一次）
- **API 文档：** https://developer.tuya.com/cn/docs/iot/api-reference
- **认证方式：** HMAC-SHA256 签名（Access ID + Access Secret）

---

## 三、社区工具与替代方案

### 3.1 本地控制工具（绕过涂鸦云）

| 工具 | 语言 | 功能 | 地址 |
|------|------|------|------|
| **tinytuya** | Python | LAN 本地控制涂鸦设备 | https://github.com/jasonacox/tinytuya |
| **tuya-local** | Python | Home Assistant 本地集成（1000+ 设备） | https://github.com/make-all/tuya-local |
| **TuyAPI** | Node.js | Node.js 本地控制库 | https://github.com/codetheweb/tuyapi |

**tinytuya 用法：**
```python
import tinytuya
d = tinytuya.Device('设备ID', 'IP', 'LocalKey', version=3.3)
data = d.status()
d.turn_on()
```

### 3.2 固件刷写工具

| 工具 | 方式 | 适用 | 地址 |
|------|------|------|------|
| **Tuya-Convert** | OTA 无线刷写 | 旧版固件设备 | https://github.com/ct-Open-Source/tuya-convert |
| **Tuya-Cloudcutter** | OTA 断云刷写 | 新版固件设备（Cloudcutter 方案） | https://github.com/tuya-cloudcutter |
| **OpenBeken** | 开源固件 | BK7231T/N 60+ 芯片 | 搜索 OpenBK7231T_App |
| **串口直刷** | TTL-USB | 开发原型 | `tos.py flash` 或 `bk_writer` |

**重要提醒：** 新版涂鸦设备已修补 Tuya-Convert 利用的 OTA 漏洞，推荐优先使用 Tuya-Cloudcutter。

### 3.3 涂鸦 GitHub 开源生态

涂鸦 GitHub 组织（https://github.com/tuya）有 191+ 仓库，重点包括：

| 仓库 | Stars | 用途 |
|------|-------|------|
| tuya/TuyaOpen | 1.6k | AI+IoT 开源框架 |
| tuya/tuya-smart-life | 465 | Home Assistant 官方集成 |
| tuya/tuya-homebridge | 394 | Apple HomeKit 桥接 |
| tuya/arduino-TuyaOpen | 358 | Arduino 核心库 |
| tuya/tyutool | — | Rust 编写的 UART 下载工具 |
| tuya/tuya-iot-core-sdk | — | 嵌入式 C 云连接 SDK |
| tuya/tuyaos-link-sdk-python | — | Python 云连接 SDK |

---

## 四、部署方案论证

### 4.1 固件烧录方式

| 方式 | 场景 | 工具 |
|------|------|------|
| 串口直刷 | 开发原型 | `tos.py flash` / `bk_writer` / TTL-USB |
| OTA 无线刷写 | 改刷自定义固件 | Tuya-Convert / Tuya-Cloudcutter |
| 量产烧录 | 工厂批量 | 涂鸦云模组烧录授权平台 |

### 4.2 WiFi 配网方式

| 方式 | 原理 | 优点 | 缺点 | 适用 |
|------|------|------|------|------|
| SmartConfig | UDP 广播编码 WiFi 密码 | 不需蓝牙，BOM 最低 | 部分路由器不兼容 | 低成本 WiFi 设备 |
| AP 模式 | 设备自建热点 | 最可靠 | 用户需手动切 WiFi | 工业/B2B 设备 |
| BLE 配网 | 蓝牙传递 WiFi 凭证 | 体验最好，<15秒 | 需要蓝牙硬件 | 消费级产品（推荐） |

**推荐方案：** BLE + SmartConfig 双保险，涂鸦 WBR3 双模模组原生支持全部三种方式。

### 4.3 量产部署流程

1. 使用涂鸦预认证模组（免去完整 RF 重认证）
2. 固件开发（TuyaOS / TuyaOpen）
3. 原型测试 + 云集成验证
4. 认证提交（FCC/CE/Matter/Zigbee，通过涂鸦认证服务）
5. 工厂设置：云模组烧录授权平台注入凭证 → 自动化 PCBA 测试 → MAC 地址分配
6. 量产：MES 集成编程设备

---

## 五、网络连通性与云服务

### 5.1 设备到云通信

- **协议：** MQTT 3.1.1 over TLS 1.2
- **数据格式：** JSON（TuyaLink 规范）
- **Topic 分类：** 物模型/数据点、OTA 固件更新、配置管理、状态上报
- **区域端点：** 中国、美国、欧洲、印度各有独立端点

### 5.2 多协议支持

- WiFi 2.4GHz（消费设备主力）
- Zigbee 3.0（低功耗 Mesh 网络）
- Bluetooth/BLE（配网和短距控制）
- Matter（新标准，涂鸦是 CSA-IOT 董事会成员）

### 5.3 最新 AI 产品（2025-2026）

| 产品 | 发布时间 | 说明 |
|------|---------|------|
| **Hey Tuya** | CES 2026 | 多 Agent AI 生活助手，PAE 引擎（长期记忆+实时通信+动态编排） |
| **DuckyClaw** | 2026.03 | 硬件原生 AI Agent 框架，支持 T5-AI Board/树莓派/瑞芯微 |
| **Tuya Cobuilder** | 2026.05 | 端到端物理 AI 生成系统，自然语言→完整 AI 硬件设备 |

---

## 六、开发入口与官方资源

| 资源 | 地址 |
|------|------|
| 开发者平台首页 | https://developer.tuya.com/cn/overview |
| IoT 设备开发文档 | https://developer.tuya.com/cn/docs/iot/ |
| TuyaOS 快速入门 | https://developer.tuya.com/cn/docs/iot-device-dev/TuyaOS-course |
| TuyaOpen 快速入门 | https://developer.tuya.com/cn/docs/developer/opensdk-quickstart |
| 云 API 参考 | https://developer.tuya.com/cn/docs/iot/api-reference |
| MCU 接入指南 | https://developer.tuya.com/cn/docs/iot/mcu-access-guide |
| 硬件选型 | https://solution.tuya.com/cn/hardwareSelection |
| 认证服务 | https://developer.tuya.com/cn/certification |
| 下载专区 | https://developer.tuya.com/cn/docs/iot/download |
| App SDK | https://developer.tuya.com/cn/docs/app-development/app-sdk-smart-home |

---

*本报告基于公开资料、官方文档和社区实践整理。涂鸦生态更新迅速，建议以官方文档为准。*
