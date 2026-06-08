# TuYa

## What This Is

TuYa is a local TuyaOpen/T5AI-Board workspace. Its main app is `codex_quota_t5`, a firmware that displays Codex quota data on the T5AI-Board 480x320 LCD through MQTT push + HTTP fallback dual-channel, with automatic reconnection, exponential backoff with jitter, and multi-device topic support.

The workspace also includes a PC bridge server (`bridge_server/`), stability test infrastructure (`tests/stability/`), Tuya IoT research notes, and hardware references.

**GitHub:** https://github.com/adlink8/t5ai-codex-quota
**CI:** GitHub Actions (python-check, secret-scan, markdown-check)

## Core Value

Make the T5AI-Board reliably display Codex quota status via MQTT + HTTP fallback, with a repeatable build, flash, and debug workflow.

## Requirements

### Validated

- ✓ TuyaOpen SDK build inside WSL — Phase 1
- ✓ WiFi + HTTP polling + LVGL UI on hardware — Phase 2
- ✓ MQTT connect, subscribe, receive updates — Phase 3
- ✓ Disconnect detection with 2-second debounce — Phase 3
- ✓ Exponential backoff reconnection (1s → 60s cap) — Phase 3
- ✓ HTTP fallback auto-switch when MQTT disconnected — Phase 3
- ✓ Broker recovery auto-reconnect within ~60s — Phase 3
- ✓ SDK socket fd guard prevents infinite error loop — Phase 3
- ✓ Config unified: CMakeLists.txt no hardcoded IPs, Kconfig single source — Issue #1
- ✓ Bridge server defaults 127.0.0.1, token auth, CORS opt-in — Issue #1
- ✓ Mosquitto restricted to localhost, auth support — Issue #1
- ✓ HTTP body truncation returns explicit error — Issue #1
- ✓ JSON parse validates required fields — Issue #1
- ✓ ThreadingHTTPServer prevents blocking — Issue #1
- ✓ MQTT topic `codex/quota/global` + DEVICE_ID multi-device — Issue #1
- ✓ Device heartbeat receiving on bridge status page — Issue #1
- ✓ MQTT reconnect jitter 0-1000ms — Issue #1
- ✓ /metrics and /history endpoints — Issue #1
- ✓ venv isolation in start_bridge.bat — Issue #1
- ✓ PID-based stop_bridge.bat — Issue #1
- ✓ GitHub Actions CI — Issue #1
- ✓ .gitignore covers sensitive/runtime files — Issue #1
- ✓ Stability test scripts ready — Issue #1

### Active

- [ ] 4-hour stability test execution (scripts at `tests/stability/`)

### Deferred

- [ ] LCD Diagnostics page (needs hardware button input)
- [ ] Quota history trends and UI arrows
- [ ] Threshold alerting (warning/critical/recovery)
- [ ] NVS/serial config (change WiFi without recompile)
- [ ] OTA capability (version check + UG upgrade)

### Out of Scope

- Full Tuya cloud product lifecycle
- Mobile app or MiniApp development
- OTA deployment and mass production tooling

## Architecture

```
T5AI-Board ←──MQTT subscribe─── Mosquitto (127.0.0.1:1883) ←──MQTT publish─── Bridge Server
codex/quota/global                                             token auth         (127.0.0.1:5678)
device heartbeat ←─── codex/device/+/heartbeat                                     ↕ Codex API
```

- **Primary (MQTT):** `codex/quota/global` topic, retain, token-gated
- **Fallback (HTTP):** Auto-switch on MQTT disconnect, exponential backoff + jitter
- **Security:** Bridge defaults localhost, LAN mode + token opt-in, CORS opt-in
- **Multi-device:** DEVICE_ID in topic path, heartbeat monitoring

## Context

- **Hardware:** Tuya T5AI-Board, BK7258 (ARM Cortex-M33F @480MHz), 16MB PSRAM, 8MB Flash, 480x320 RGB LCD
- **SDK:** TuyaOpen (C/C++, CMake, Ninja, LVGL)
- **Build env:** WSL Ubuntu-D at `/home/li/TuyaOpen`
- **Flash:** tyutool_cli.exe, COM12 @ 921600 baud, debug COM11 @ 460800 baud
- **Bridge:** Python ThreadingHTTPServer (port 5678) + Mosquitto MQTT (port 1883)
- **Config:** Kconfig + app_default.config (single source), see `docs/configuration.md`

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| QIO format for flashing | UA/UG lack bootloader, cause black screen | ✓ Verified |
| Socket fd guard in SDK | Prevents infinite error loop | ✓ Verified |
| Disconnect debounce (2s) | Suppresses callback storm | ✓ Verified |
| Bridge defaults 127.0.0.1 | Prevents LAN data leakage | ✓ Issue #1 |
| Token auth on /quota | Prevents unauthorized quota access | ✓ Issue #1 |
| Config via Kconfig only | Single source, no CMake hardcoding | ✓ Issue #1 |
| ThreadingHTTPServer | Slow API calls don't block health/status | ✓ Issue #1 |
| Topic `codex/quota/global` | Supports multi-device on same broker | ✓ Issue #1 |
| Reconnect jitter | Prevents thundering herd on recovery | ✓ Issue #1 |
| venv in start_bridge.bat | Isolates Python deps from system | ✓ Issue #1 |
| PID-based process mgmt | Precise stop, no accidental kills | ✓ Issue #1 |
| SSH push via port 443 | HTTPS blocked from current network | ✓ Workaround |

## Constraints

- **Hardware required:** Runtime behavior needs T5AI-Board
- **SDK dependency:** Full TuyaOpen at `~/TuyaOpen`
- **Network:** Device and PC on same LAN
- **SDK patch:** `mqtt_client_wrapper.c` socket fd guard not upstreamed
- **Git push:** HTTPS port 443 intermittently blocked, use SSH fallback

---
*Last updated: 2026-06-08 after Issue #1 completion (20/21 items, commit b806cfc)*
