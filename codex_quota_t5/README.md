# Codex Quota Monitor — T5AI-Board 移植版

将小米手环 8 Pro 的 Codex 额度监控应用移植到涂鸦 T5AI-Board。
通过 WiFi 直连 PC 桥接服务器，在 480x320 LCD 屏幕上显示额度环形进度条。
支持 MQTT 推送 + HTTP 回退双通道，断线自动重连。

## 架构

```
T5AI-Board ←──MQTT subscribe─── Mosquitto Broker ←──MQTT publish─── PC 桥接服务器
  10.13.220.137                 10.13.220.28:1883                    (同一台 PC)
```

- **主通道 (MQTT)**：板子订阅 `codex/quota` 主题，桥接服务器推送额度变化
- **回退通道 (HTTP)**：MQTT 断连时自动切换 HTTP GET 轮询（指数退避 60s→300s）
- **断线重连**：broker 恢复后板子自动重连（指数退避 1s→60s cap），重新订阅

## 文件结构

```
codex_quota_t5/
├── src/
│   ├── tuya_main.c      # 主程序：WiFi + MQTT 连接 + 指数退避重连 + 任务调度
│   ├── codex_http.c     # HTTP 请求 + cJSON 解析 (MQTT 回退通道)
│   ├── codex_http.h     # 数据结构定义
│   ├── codex_mqtt.c     # MQTT 客户端：连接/订阅/回调/防抖/重连
│   ├── codex_mqtt.h     # MQTT 接口头文件
│   ├── codex_ui.c       # LVGL UI：环形进度条 + 颜色分级
│   └── codex_ui.h       # UI 接口
├── CMakeLists.txt       # 构建配置 (含 MQTT_HOST/PORT + 源文件列表)
├── app_default.config   # 编译选项（WiFi、Bridge、MQTT）
├── Kconfig              # 可配置项（WiFi SSID、服务器地址、MQTT 配置）
└── README.md            # 本文件
```

## 编译烧录步骤

### 已验证构建环境

- TuyaOpen SDK：WSL Ubuntu-D `/home/li/TuyaOpen`
- SDK 内应用路径：`/home/li/TuyaOpen/apps/codex_quota`
- 构建命令：

```bash
# WSL Ubuntu-D 中执行（PATH 必须用硬编码路径，不能用 $HOME）
export PATH=/home/li/bin:/usr/bin:/bin
cd ~/TuyaOpen/apps/codex_quota
rm -rf .build dist
python ../../tos.py build
```

构建成功后输出目录：

```text
/home/li/TuyaOpen/apps/codex_quota/dist/codex_quota_1.0.0/
```

**⚠️ 烧录时必须选择 QIO 固件：**

| 文件 | 格式 | 大小 | 用途 |
|------|------|------|------|
| `codex_quota_QIO_1.0.0.bin` | QSPI 完整镜像 | ~2.6 MB | **✅ 唯一正确的烧录文件**，含 bootloader + 分区表 + 应用 |
| `codex_quota_UA_1.0.0.bin` | UART 升级格式 | ~2.4 MB | ❌ 烧录后板子黑屏无法启动 |
| `codex_quota_UG_1.0.0.bin` | OTA 升级格式 | ~1.3 MB | ❌ 仅用于在线 OTA 升级 |

### 1. 把项目复制到 TuyaOpen 目录

```bash
# 假设 TuyaOpen 在 ~/TuyaOpen
cp -r codex_quota_t5 ~/TuyaOpen/apps/codex_quota
```

### 2. 修改 WiFi、桥接服务器与 MQTT 配置

不要把 WiFi 密码写进源码。通过 TuyaOpen 配置写入：

```bash
tos.py config menu
```

需要设置：

- `WIFI_SSID`：你的 WiFi 名称
- `WIFI_PASSWORD`：你的 WiFi 密码
- `BRIDGE_HOST`：PC 的局域网 IP，例如 `192.168.1.109`
- `BRIDGE_PORT`：默认 `5678`
- `BRIDGE_PATH`：默认 `/quota`
- `MQTT_HOST`：MQTT broker 的 IP（与 BRIDGE_HOST 相同即可）
- `MQTT_PORT`：默认 `1883`

`app_default.config` 只保留空 WiFi 默认值，避免把密码提交进项目。

### 3. 编译

```bash
cd ~/TuyaOpen

# Windows PowerShell
.\export.ps1

# 配置（选择 T5AI-Board）
tos.py config

# 编译
tos.py build
```

### 4. 烧录

```powershell
# Windows PowerShell（必须用 QIO 固件）
echo n | tyutool_cli.exe write -d t5ai -p COM12 -f <path>\codex_quota_QIO_1.0.0.bin
```

调试串口：COM11 @ 460800 baud

### 5. PC 端启动 Mosquitto + 桥接服务器

双击 `bridge_server\start_bridge.bat` 一键启动（自动启动 Mosquitto + 桥接服务），或手动启动：

```powershell
# 启动 Mosquitto broker
mosquitto.exe -c bridge_server\mosquitto.conf -d

# 启动桥接服务器
python bridge_server\codex_bridge_server.py --port 5678
```

停止服务：双击 `bridge_server\stop_bridge.bat`。

## MQTT 功能说明

- **连接**：板子启动后连接 WiFi，然后连接 MQTT broker 并订阅 `codex/quota`
- **推送模式**：桥接服务器在额度变化时发布 retain 消息，板子即时收到更新
- **断连检测**：broker 停止后板子检测到 socket fault，触发 on_disconnect 回调
- **防抖**：2 秒窗口内重复回调静默处理，避免回调风暴
- **指数退避重连**：1s → 2s → 4s → ... → 60s (上限)，每次完整销毁旧客户端并重建
- **HTTP 自动回退**：MQTT 断连期间 refresh_task 自动恢复 HTTP 轮询获取数据
- **自动恢复**：broker 恢复后板子在下一个退避周期自动重连并重新订阅

## UI 说明

- 480x320 LCD，纯黑背景（省电 + 高对比度）
- 左侧大环：主窗口（5小时额度）剩余百分比
- 右侧环：副窗口（周额度）剩余百分比
- 颜色分级：绿色 >50%、黄色 >20%、红色 ≤20%
- 底部状态栏：上次更新时间 / 离线提示 / MQTT 连接状态
- HTTP 失败退避：60s → 120s → 240s → 上限 300s
- MQTT 断连重连退避：1s → 2s → ... → 上限 60s
