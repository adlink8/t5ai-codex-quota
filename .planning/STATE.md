# State: TuYa — Codex Quota Monitor

## Project Reference

See: `.planning/PROJECT.md` (updated 2026-06-07)

**Core value:** Make the T5AI-Board reliably display Codex quota status via MQTT + HTTP fallback, with a repeatable build, flash, and debug workflow.

**Current focus:** Phase 3 complete — MQTT disconnect/reconnect, HTTP fallback, and exponential backoff all verified on hardware.

## Current Status

- Project initialized 2026-06-07.
- **Phase 1 (Build Baseline):** DONE — TuyaOpen SDK build succeeds in WSL Ubuntu-D.
- **Phase 2 (Runtime Connectivity):** DONE — WiFi, HTTP polling, LVGL UI all working on hardware.
- **Phase 3 (MQTT Integration):** DONE — Full MQTT + HTTP dual-channel verified on hardware.
- GitHub repo created: [adlink8/t5ai-codex-quota](https://github.com/adlink8/t5ai-codex-quota)
- Bridge server organized into `bridge_server/` with one-click start/stop scripts.
- Firmware archive created at `firmware_archive/` (14 binaries, categorized).
- Flash tool directory cleaned (only tyutool_cli.exe + tyutool_gui.exe remain).

## Phase 3 Verified Features

| Feature | Status | Notes |
|---------|--------|-------|
| MQTT connect + subscribe | ✓ | `codex/quota` topic, QoS 0, retain |
| Disconnect detection | ✓ | Socket fault → MQTTRecvFailed → on_disconnect |
| Disconnect debounce | ✓ | 2-second window, every-50th silent summary |
| Exponential backoff reconnect | ✓ | 1s → 2s → 4s → ... → 60s cap |
| HTTP fallback auto-switch | ✓ | refresh_task resumes HTTP polling when MQTT down |
| Broker recovery auto-reconnect | ✓ | Board reconnects within ~60s of broker restart |
| SDK socket fd guard | ✓ | `mqtt_client_yield()` checks `sock_fd < 0` before ProcessLoop |

## Build & Flash Reference

| Item | Value |
|------|-------|
| SDK | TuyaOpen @ `~/TuyaOpen` (WSL Ubuntu-D) |
| App path | `~/TuyaOpen/apps/codex_quota` |
| Build command | `python ../../tos.py build` |
| Flash tool | `tyutool_cli.exe write -d t5ai -p COM12 -f <QIO.bin>` |
| Flash port | COM12 (CH342 Port A) @ 921600 baud |
| Debug port | COM11 (CH342 Port B) @ 460800 baud |
| **Correct firmware** | **QIO (~2.6 MB)** — QSPI full image with bootloader |
| Wrong firmware | UA (~2.4 MB) — causes black screen, no bootloader |

## Network Topology

- PC: 10.13.220.28 (WiFi hotspot "一连就爆炸")
- Board: 10.13.220.137 (same subnet)
- Mosquitto: port 1883, anonymous access
- Bridge server: port 5678, HTTP + MQTT publish

## Key Bugs Found & Fixed

| Bug | Root Cause | Fix |
|-----|-----------|-----|
| Board black screen after flash | Flashed UA format instead of QIO | Always use QIO (~2.6MB) for flashing |
| Infinite error loop in SDK wrapper | `MQTT_ProcessLoop` called with invalid socket | Added `sock_fd < 0` guard in `mqtt_client_yield()` |
| Callback storm on disconnect | Multiple rapid `on_disconnected` callbacks | 2-second debounce window with counter |
| Kconfig missing MQTT entries | No MQTT menu in Kconfig | Added MQTT_HOST and MQTT_PORT config entries |

## Open Items

- [ ] 4-hour stability test (board running unattended)
- [ ] SDK `mqtt_client_wrapper.c` socket fd guard not upstreamed to TuyaOpen

## Next Action

Run 4-hour stability test: leave the board running with MQTT broker active, periodically kill/restart broker to verify reconnection, check UI stays responsive and data stays fresh.
