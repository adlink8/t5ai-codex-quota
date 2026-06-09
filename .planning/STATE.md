# State: TuYa — Codex Quota Monitor

## Project Reference

See: `.planning/PROJECT.md` (updated 2026-06-08)

**Core value:** Make the T5AI-Board reliably display Codex quota status via MQTT + HTTP fallback, with a repeatable build, flash, and debug workflow.

**Current focus:** Issue #1 resolved — security hardening, config unification, stability test infrastructure, CI all done. Phase 3 hardware-verified features remain the baseline.

## Current Status

- Project initialized 2026-06-07.
- **Phase 1 (Build Baseline):** DONE
- **Phase 2 (Runtime Connectivity):** DONE
- **Phase 3 (MQTT Integration):** DONE — hardware-verified.
- **Issue #1 (Security & Engineering):** DONE (2026-06-08) — 20/21 items completed, 1 deferred.
- GitHub: [adlink8/t5ai-codex-quota](https://github.com/adlink8/t5ai-codex-quota)
- GitHub Actions CI: python-check, secret-scan, markdown-check
- Push via SSH: `ssh://git@ssh.github.com:443/adlink8/t5ai-codex-quota.git` (HTTPS blocked from current network)

## Issue #1 Completion Summary

| Category | Items | Completed |
|----------|-------|-----------|
| P0 High Priority | 7 | 7/7 |
| P1 Medium Priority | 6 | 5/6 (LCD Diagnostics deferred) |
| P2 Enhancement | 4 | Deferred to future |
| Startup Scripts | 2 | 2/2 |
| Docs & Governance | 2 | 2/2 |

### Key Changes

**Security:**
- Bridge server defaults to 127.0.0.1, `--lan-mode` for network access
- Token auth on `/quota` endpoint
- Mosquitto restricted to localhost, auth examples provided
- CORS removed by default, `--enable-cors` to opt in

**Config:**
- CMakeLists.txt no longer hardcodes MQTT_HOST/MQTT_PORT
- Single source of truth: Kconfig + app_default.config
- `docs/configuration.md` created as unified guide

**Firmware:**
- HTTP body truncation returns error -2 with size logging
- JSON parse validates `primary` object + `remaining_percent` number
- MQTT reconnect jitter 0-1000ms on exponential backoff
- Topic changed to `codex/quota` with DEVICE_ID support

**Bridge Server:**
- ThreadingHTTPServer (no more blocking)
- Device heartbeat receiving (`codex/device/+/heartbeat`)
- `/metrics` endpoint (uptime, request counts, API stats)
- `/history` endpoint (last 50 quota responses)
- MQTT credentials support (`--mqtt-user`/`--mqtt-pass`)

**Infrastructure:**
- `.github/workflows/ci.yml` with 3 jobs
- `.gitignore` covers sensitive/runtime/venv files
- `start_bridge.bat` uses `.venv` isolation + PID tracking
- `stop_bridge.bat` kills by PID, not window title

**Stability Tests:**
- `tests/stability/test_plan.md` — 5 scenarios with pass/fail criteria
- `tests/stability/run_full_test.ps1` — master orchestration
- `tests/stability/restart_broker_loop.ps1` — broker restart loop
- `tests/stability/monitor_serial_log.ps1` — serial pattern detection
- `tests/stability/report_template.md` — report template

## Phase 3 Verified Features (Hardware)

| Feature | Status | Notes |
|---------|--------|-------|
| MQTT connect + subscribe | ✓ | `codex/quota` topic (updated) |
| Disconnect detection | ✓ | Socket fault → on_disconnect with debounce |
| Exponential backoff + jitter | ✓ | 1s→60s cap + 0-1000ms random jitter |
| HTTP fallback auto-switch | ✓ | Automatic when MQTT disconnected |
| Broker recovery reconnect | ✓ | ~60s after broker restart |
| SDK socket fd guard | ✓ | Prevents infinite error loop |

## Build & Flash Reference

| Item | Value |
|------|-------|
| SDK | TuyaOpen @ `~/TuyaOpen` (WSL Ubuntu-D) |
| Build | `python ../../tos.py build` |
| Flash | `tyutool_cli.exe write -d t5ai -p COM12 -f <QIO.bin>` |
| Flash port | COM12 @ 921600 baud |
| Debug port | COM11 @ 460800 baud |
| **Correct firmware** | **QIO (~2.6 MB)** |

## Open Items

- [x] 4-hour stability test execution — PASSED, no disconnection (2026-06-09)
- [ ] LCD Diagnostics page (needs hardware button input, Issue #11 deferred)
- [ ] SDK socket fd guard not upstreamed to TuyaOpen
- [ ] P2 enhancements: quota history, alerts, NVS config, OTA

## Next Action

Execute the 4-hour stability test using `tests/stability/run_full_test.ps1` with broker restart every 10 minutes. Fill in the report template and commit results.
