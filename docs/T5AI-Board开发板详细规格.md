# T5AI-Board 开发板详细规格

> 更新日期：2026-06-07
> 设备：涂鸦 T5AI-Board（旗舰版）

---

## 一、核心芯片：T5-E1-IPEX 模组

| 参数 | 规格 |
|------|------|
| 芯片型号 | T5QN88 |
| CPU | ARMv8-M Cortex-M33F @ 480MHz（FPU + DSP） |
| PSRAM | 16MB |
| Flash | 8MB |
| WiFi | Wi-Fi 6 (802.11ax) 2.4GHz |
| 蓝牙 | Bluetooth LE 5.4 |
| GPIO | 56 个 |
| 工作温度 | -40 ~ 85°C |
| 模组尺寸 | 18 × 19.7 × 2.8mm |
| 天线 | IPEX 外接天线座 |
| 认证 | FCC / CE 预认证 |

---

## 二、外设接口

| 接口 | 数量 | 用途 |
|------|------|------|
| UART | 3 路 | 调试串口、外部 MCU 通信 |
| SPI | 2 路 | 高速外设（屏幕、存储） |
| I2C | 2 路 | 传感器（温湿度、环境光等） |
| I2S | 3 路 | 音频（麦克风输入、扬声器输出、回声消除） |
| PWM | 5 路 | LED 调光、电机控制 |
| ADC | 11 通道 | 模拟信号采集 |
| USB 2.0 | 1 | 数据传输、摄像头 |
| SDIO | 1 | TF 卡扩展存储 |
| DVP | 1 | 并口摄像头接口 |
| RGB 屏接口 | 1 | 直连 LCD 显示屏 |

---

## 三、板载硬件

- 双麦克风（远场拾音 + AEC 回声消除）
- 板载扬声器 + 功放
- CH343 双路 USB 转串口（烧录 + 调试）
- TF 卡槽（扩展存储）
- DVP / USB 摄像头接口
- RGB LCD 显示接口
- 3 个用户按键
- LED 状态指示灯
- Type-C 供电 + 数据

---

## 四、开发环境搭建

### 4.1 安装要求

- **操作系统：** Windows 10+ (需 WSL2 + Ubuntu 20.04) / Linux / macOS
- **IDE：** VS Code + Tuya Wind IDE 插件
- **串口驱动：** CH343（Windows 需手动安装）
- **Python 3：** tos.py 脚本运行需要

### 4.2 开发流程

```
1. 安装 VS Code + Tuya Wind IDE 插件
2. 配置 WSL2 环境（Windows 用户）
3. 克隆 TuyaOpen SDK：git clone https://github.com/tuya/TuyaOpen
4. 创建产品（platform.tuya.com）获取 PID 和密钥
5. 选择示例项目或新建项目
6. 编译：tos.py build
7. 烧录：tos.py flash（自动检测串口）
8. 手机 App 配网（BLE 配网，<15秒）
```

### 4.3 SDK 与框架

| 框架 | 地址 | 说明 |
|------|------|------|
| TuyaOpen | https://github.com/tuya/TuyaOpen | 开源 AI+IoT 框架（C/C++ + CMake） |
| arduino-TuyaOpen | https://github.com/tuya/arduino-TuyaOpen | Arduino 核心库 |
| Tuya Wind IDE | VS Code 插件市场搜索 | 官方 IDE |

### 4.4 图形库

- **LVGL** — 开源 GUI 库，T5AI-Board 的 LCD 屏幕用 LVGL 绘制 UI
- **GUI Guider** — NXP 出品的可视化 GUI 设计工具，可生成 LVGL 代码

---

## 五、AI 能力

| 能力 | 说明 | 运行位置 |
|------|------|---------|
| KWS 唤醒词 | "Hey Tuya" 或自定义 | 端侧 |
| ASR 语音识别 | 语音转文字 | 端侧 / 云端 |
| TTS 语音合成 | 文字转语音 | 云端 |
| LLM 大模型 | ChatGPT / Gemini / 豆包 | 涂鸦云 |
| MCP 协议 | 控制涂鸦生态 IoT 设备 | 涂鸦云 |
| 多模态 AI | 摄像头 + 语音 + 文字 | 混合 |

---

## 六、社区项目参考

| 项目 | 技术栈 | 难度 |
|------|--------|------|
| AI 时钟 | LCD + 天气 API + 语音对话 | 中等 |
| AI 像素屏 | 1024 LED + 传感器 | 入门 |
| AI 语音机器人 | 3D 打印 + LLM 对话 | 中等 |
| 游戏机（2048/PVZ） | LVGL 图形库 | 较高 |
| AI 音乐陪练 | 音频分析 + AI 反馈 | 较高 |
| 语音控制机械臂 | 语音 + 电机控制 | 较高 |
| 桌面聊天助手 | LCD + ChatGPT | 中等 |

---

## 七、官方文档

| 资源 | 地址 |
|------|------|
| T5AI-Board 开发板文档 | https://developer.tuya.com/cn/docs/iot-device-dev/T5-E1-IPEX-development-board |
| T5-E1-IPEX 模组规格书 | https://developer.tuya.com/cn/docs/iot/T5-E1-IPEX-Module-Datasheet |
| T5-E1-IPEX 硬件设计指南 | https://developer.tuya.com/cn/docs/iot/T5-E1-IPEX-hardware-design-guide |
| TuyaOpen 快速入门 | https://developer.tuya.com/cn/docs/developer/opensdk-quickstart |
| T5 模组规格书 | https://developer.tuya.com/cn/docs/developer/tuya-open |
| AI Coding 教程 | https://developer.tuya.com/cn/docs/developer/aicoding-tuyaopen |
