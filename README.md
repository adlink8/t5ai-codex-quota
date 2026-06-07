# TuYa

TuyaOpen T5AI-Board 本地实验项目。主应用 `codex_quota_t5` 将 Codex 额度监控移植到涂鸦 T5AI-Board，通过 MQTT 推送 + HTTP 回退双通道在 480x320 LCD 上显示额度状态。

**GitHub:** https://github.com/adlink8/t5ai-codex-quota

## 目录

| 路径 | 用途 |
|------|------|
| `codex_quota_t5/` | T5AI-Board 固件应用源码（C/C++） |
| `bridge_server/` | PC 桥接服务器 + Mosquitto 配置 + 一键启动脚本 |
| `firmware_archive/` | 历史固件归档（按阶段分类） |
| `.planning/` | GSD 项目上下文、需求、路线图和状态 |
| `涂鸦IoT开发调研报告.md` | 涂鸦平台/SDK/生态调研 |
| `T5AI-Board开发板详细规格.md` | T5AI-Board 硬件规格 |
| `涂鸦芯片模组硬件参数速查.md` | 芯片/模组参数速查 |
| `涂鸦开发社区资源与教程索引.md` | 官方和社区资源索引 |

## 架构

```
T5AI-Board ←──MQTT subscribe─── Mosquitto Broker ←──MQTT publish─── bridge_server/
  10.13.220.137                 PC:1883                                (同一台 PC)
```

- **主通道 (MQTT)**：板子订阅 `codex/quota`，桥接服务器 retain 推送额度变化
- **回退通道 (HTTP)**：MQTT 断连时自动切回 HTTP GET 轮询
- **断线重连**：指数退避 1s→60s，broker 恢复后自动重连并重新订阅

## 快速开始

### 1. 启动桥接服务器

双击 `bridge_server\start_bridge.bat`，自动启动 Mosquitto + 桥接服务。

### 2. 编译固件

```bash
# WSL Ubuntu-D
export PATH=/home/li/bin:/usr/bin:/bin
cd ~/TuyaOpen/apps/codex_quota
python ../../tos.py build
```

### 3. 烧录（必须用 QIO 固件）

```powershell
echo n | tyutool_cli.exe write -d t5ai -p COM12 -f <path>\codex_quota_QIO_1.0.0.bin
```

| 格式 | 大小 | 用途 |
|------|------|------|
| **QIO** ~2.6MB | QSPI 完整镜像 | **烧录用** |
| UA ~2.4MB | UART 升级格式 | 勿烧录 |
| UG ~1.3MB | OTA 升级 | 勿烧录 |

## 关键文档

- `.planning/PROJECT.md` — 项目需求与决策
- `.planning/STATE.md` — 当前状态与进度
- `.planning/ROADMAP.md` — 路线图
- `codex_quota_t5/README.md` — 固件源码详细说明
- `bridge_server/` — 桥接服务器说明
