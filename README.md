# TuYa

涂鸦 T5AI-Board / TuyaOpen 本地实验项目。

当前主应用是 `codex_quota_t5`：把小米手环项目里的 Codex 额度监控移植到涂鸦 T5AI-Board，通过 WiFi 访问 PC 桥接服务器，并在 480x320 LCD 上显示额度状态。

## 目录

| 路径 | 用途 |
|------|------|
| `codex_quota_t5/` | T5AI-Board 固件应用源码 |
| `烧写工具/` | 本地烧写工具、固件镜像和工具缓存 |
| `.planning/` | GSD 项目上下文、需求、路线图和状态 |
| `涂鸦IoT开发调研报告.md` | 涂鸦平台/SDK/生态调研 |
| `T5AI-Board开发板详细规格.md` | T5AI-Board 硬件规格 |
| `涂鸦芯片模组硬件参数速查.md` | 芯片/模组参数速查 |
| `涂鸦开发社区资源与教程索引.md` | 官方和社区资源索引 |

## 当前架构

```text
T5AI-Board -> WiFi 局域网 -> PC 桥接服务器 -> Codex/ChatGPT 额度来源
```

## 下一步

1. 找到本机 TuyaOpen SDK 目录。
2. 将 `codex_quota_t5` 复制到 TuyaOpen 的 `apps/` 目录。
3. 按 `.planning/ROADMAP.md` 的 Phase 1 验证构建基线。

## 关键文档

- `.planning/PROJECT.md`
- `.planning/REQUIREMENTS.md`
- `.planning/ROADMAP.md`
- `.planning/STATE.md`
- `codex_quota_t5/README.md`

