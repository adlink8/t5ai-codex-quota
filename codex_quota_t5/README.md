# Codex Quota Monitor — T5AI-Board 移植版

将小米手环 8 Pro 的 Codex 额度监控应用移植到涂鸦 T5AI-Board。
通过 WiFi 直连 PC 桥接服务器，在 480x320 LCD 屏幕上显示额度环形进度条。
支持 MQTT 推送 + HTTP 回退双通道，断线自动重连，支持触摸屏操作。

## 架构

```
T5AI-Board ←──MQTT subscribe─── Mosquitto Broker ←──MQTT publish─── PC 桥接服务器
  (WiFi IP)                     <BRIDGE_HOST>:1883                   (同一台 PC)
```

> 注：`<BRIDGE_HOST>` 通过 `tos.py config menu` 中的 `BRIDGE_HOST` / `MQTT_HOST` 配置，不再硬编码在源码中。

- **主通道 (MQTT)**：板子订阅 `codex/quota` 主题，桥接服务器推送额度变化
- **回退通道 (HTTP)**：MQTT 断连时自动切换 HTTP GET 轮询（指数退避 60s→300s）
- **断线重连**：broker 恢复后板子自动重连（指数退避 1s→60s cap），重新订阅

## 文件结构

```
codex_quota_t5/
├── src/
│   ├── tuya_main.c           # 主程序：WiFi + MQTT 连接 + 指数退避重连 + 任务调度
│   ├── codex_http.c          # HTTP 请求 + cJSON 解析 (MQTT 回退通道)
│   ├── codex_http.h          # 数据结构定义
│   ├── codex_mqtt.c          # MQTT 客户端：连接/订阅/回调/防抖/重连
│   ├── codex_mqtt.h          # MQTT 接口头文件
│   ├── codex_serial.c        # 串口通信
│   ├── codex_serial.h        # 串口接口
│   ├── codex_ui.c            # LVGL UI：环形进度条 + 颜色分级 + 触摸交互
│   ├── codex_ui.h            # UI 接口
│   ├── drv_tp.c              # 触摸驱动 (Beken SDK，直接编译进 app)
│   ├── tp_driver.c           # 触摸底层驱动
│   ├── bk_queue.c            # Beken 队列实现
│   └── sim_i2c_driver_v2.c   # 模拟 I2C 驱动 v2（TP 驱动依赖）
├── CMakeLists.txt            # 构建配置 (源文件列表 + TP 驱动 include + 编译选项)
├── app_default.config        # 编译选项默认值（WiFi、Bridge、MQTT、TP，安全空值）
├── Kconfig                   # 可配置项（WiFi SSID、服务器地址、MQTT 配置）— 唯一配置源
└── README.md                 # 本文件
```

## SDK 修改说明

本项目对 TuyaOpen SDK 有两处修改，随项目一同提交：

### 1. LVGL 字体配置 (`src/liblvgl/v8/conf/lv_conf.h`)

启用了项目 UI 所需的 Montserrat 字体：14、16、20、24、32、34 号。添加了 `LV_DRAW_BUF_PARTS=10` 以支持多缓冲区渲染。

### 2. MQTT 客户端修复 (`src/libmqtt/src/mqtt_client_wrapper.c`)

- **空指针防护**：`mqtt_client_connect` 中对 `clientid`、`username`、`password` 增加 NULL 检查，避免 NULL 指针调用 `strlen` 导致崩溃。
- **Yield 超时优化**：`mqtt_client_yield` 中将 `MQTT_ProcessLoop` 超时从 `config.timeout_ms`（默认 5000ms）改为 50ms，防止长时间阻塞主循环、LVGL 任务和看门狗。
- **Socket 状态检查**：yield 前检查传输层 socket fd，若已关闭则直接返回超时，避免在已断开的连接上无限循环 `MQTTRecvFailed`/`MQTTSendFailed` 错误。

## 触摸屏支持

通过 `app_default.config` 启用 `CONFIG_ENABLE_LVGL_TP=y`，激活 LVGL v8 输入设备端口层（`lv_port_indev.c`）中的触摸代码路径。

触摸驱动初始化流程：

1. `lv_vendor_init()` → `lv_port_indev_init()` → 注册 touchpad 输入设备
2. `drv_tp_open()` 在 `tuya_main.c` 中初始化触摸屏硬件（I2C + 中断）
3. `touchpad_read()` 回调通过 TDL API `tdl_tp_dev_read()` 读取触摸坐标

> 注：`drv_tp.c`、`tp_driver.c`、`bk_queue.c`、`sim_i2c_driver_v2.c` 从 Beken SDK 复制到 `src/` 目录直接编译，
> 因为 SDK 的 `sdkconfig.cmake` 不会自动启用 TP 驱动编译。CMakeLists.txt 中已添加必要的 include 路径和 warning 抑制。

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

**烧录时必须选择 QIO 固件：**

| 文件 | 格式 | 大小 | 用途 |
|------|------|------|------|
| `codex_quota_QIO_1.0.0.bin` | QSPI 完整镜像 | ~2.6 MB | **唯一正确的烧录文件**，含 bootloader + 分区表 + 应用 |
| `codex_quota_UA_1.0.0.bin` | UART 升级格式 | ~2.4 MB | 烧录后板子黑屏无法启动 |
| `codex_quota_UG_1.0.0.bin` | OTA 升级格式 | ~1.3 MB | 仅用于在线 OTA 升级 |

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

**重要：MQTT_HOST 和 MQTT_PORT 仅通过 Kconfig 配置**，不再在 `CMakeLists.txt` 中硬编码。
`CMakeLists.txt` 仅包含构建结构（源文件列表、LVGL 字体等），所有网络地址均由 Kconfig 管理。

`app_default.config` 只保留空 WiFi 默认值，避免把密码提交进项目。

**安全改进：**
- 桥接服务器的 MQTT broker 配置位于 `bridge_server/mosquitto.conf`，建议生产环境限制监听地址
- 桥接服务器使用 token 认证（`~/.codex/auth.json`），自动刷新过期 token
- `.gitignore` 已排除 `.env`、`auth.json`、`*.secret` 等敏感文件
- 完整配置指南参见 [docs/configuration.md](../docs/configuration.md)

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
- 支持触摸屏交互
- HTTP 失败退避：60s → 120s → 240s → 上限 300s
- MQTT 断连重连退避：1s → 2s → ... → 上限 60s

## 已知问题与修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| MQTT 数据到达后白屏崩溃 | `MQTT_ProcessLoop` 超时 5000ms 阻塞主循环，LVGL 和看门狗饿死 | Yield 超时改为 50ms |
| 触摸屏无反应 | `CONFIG_ENABLE_LVGL_TP` 未启用，触摸代码被 `#ifdef` 编译掉 | `app_default.config` 中启用 `CONFIG_ENABLE_LVGL_TP=y` |
| MQTT 连接空指针崩溃 | `clientid`/`username`/`password` 为 NULL 时调用 `strlen` | 增加 NULL 检查 |
| TP 驱动链接失败 | Beken SDK 的 `sdkconfig.cmake` 未启用 `CONFIG_TP`，`libdriver.a` 不含 TP 符号 | 将 TP 驱动源文件复制到 `src/` 直接编译 |
