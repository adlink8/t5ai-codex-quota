# TuYa

## What This Is

TuYa is a local TuyaOpen/T5AI-Board workspace. Its main app is `codex_quota_t5`, a firmware that displays Codex quota data on the T5AI-Board 480x320 LCD through MQTT push + HTTP fallback dual-channel, with automatic reconnection and exponential backoff.

The workspace also includes a PC bridge server (`bridge_server/`), Tuya IoT research notes, hardware references, and a firmware archive.

**GitHub:** https://github.com/adlink8/t5ai-codex-quota

## Core Value

Make the T5AI-Board reliably display Codex quota status via MQTT + HTTP fallback, with a repeatable build, flash, and debug workflow.

## Requirements

### Validated

- ✓ `codex_quota_t5` builds inside WSL TuyaOpen SDK — Phase 1
- ✓ WiFi + HTTP polling + LVGL UI working on hardware — Phase 2
- ✓ MQTT connect, subscribe, receive quota updates — Phase 3
- ✓ Disconnect detection with 2-second debounce — Phase 3
- ✓ Exponential backoff reconnection (1s → 60s cap) — Phase 3
- ✓ HTTP fallback auto-switch when MQTT disconnected — Phase 3
- ✓ Broker recovery auto-reconnect within ~60s — Phase 3
- ✓ SDK socket fd guard prevents infinite error loop — Phase 3

### Active

- [ ] 4-hour stability test: board running unattended with periodic broker restart

### Out of Scope

- Full Tuya cloud product lifecycle — local firmware experiments only
- Mobile app or MiniApp development
- OTA deployment and mass production tooling
- Rewriting the PC bridge server from scratch

## Architecture

```
T5AI-Board ←──MQTT subscribe─── Mosquitto (PC:1883) ←──MQTT publish─── Bridge Server
  10.13.220.137                                              ↕ HTTP API
                                                        Codex/ChatGPT quota source
```

- **Primary channel (MQTT):** Board subscribes to `codex/quota`, bridge publishes retain messages
- **Fallback channel (HTTP):** Auto-switches to HTTP GET polling when MQTT disconnected
- **Reconnection:** Exponential backoff 1s→60s, full client destroy/recreate cycle
- **Debounce:** 2-second window suppresses repeated disconnect callbacks

## Context

- **Hardware:** Tuya T5AI-Board, BK7258 (ARM Cortex-M33F @480MHz), 16MB PSRAM, 8MB Flash, 480x320 RGB LCD
- **SDK:** TuyaOpen (C/C++, CMake, Ninja, LVGL)
- **Build env:** WSL Ubuntu-D at `/home/li/TuyaOpen`
- **Flash:** tyutool_cli.exe, COM12 @ 921600 baud, debug COM11 @ 460800 baud
- **Bridge:** Python HTTP server (port 5678) + Mosquitto MQTT (port 1883)
- **Network:** PC WiFi hotspot, board on same subnet

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Use QIO format for flashing | UA/UG lack bootloader, cause black screen | ✓ Verified |
| Socket fd guard in SDK wrapper | Prevents infinite error loop on broken socket | ✓ Verified |
| Disconnect debounce (2s window) | Suppresses callback storm from rapid TCP teardown | ✓ Verified |
| Full client destroy on reconnect | Avoids stale state in TuyaOpen MQTT wrapper | ✓ Verified |
| Bridge server in `bridge_server/` | Self-contained with start/stop scripts and mosquitto.conf | ✓ Done |
| Firmware archive categorized | Historical builds preserved by phase | ✓ Done |
| WiFi credentials out of defaults | Prevents accidental secret commits | ✓ Good |
| Use WSL SDK for builds | Windows SDK not available, WSL path proven | ✓ Good |

## Constraints

- **Hardware required:** Runtime behavior cannot be validated without T5AI-Board
- **SDK dependency:** Build requires full TuyaOpen checkout at `~/TuyaOpen`
- **Network:** Device and PC must be on same LAN
- **SDK modification:** `mqtt_client_wrapper.c` has local patch (socket fd guard) not upstreamed

---
*Last updated: 2026-06-07 after Phase 3 completion, firmware archive, and GitHub push*
