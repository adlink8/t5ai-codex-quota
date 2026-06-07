# State: TuYa

## Project Reference

See: `.planning/PROJECT.md` (updated 2026-06-07)

**Core value:** Make the T5AI-Board reliably display useful local AI/tooling status from a PC bridge with a repeatable build, flash, and debug workflow.
**Current focus:** Phase 2 - Runtime Connectivity config complete; runtime verification pending hardware flash/debug

## Current Status

- Project initialized on 2026-06-07.
- Existing app found at `codex_quota_t5`.
- Existing flash tools found at `烧写工具`.
- Existing research/reference docs found in project root.
- Local GSD CLI did not support the documented `gsd-sdk query` command, so initialization files were created directly in the expected GSD layout.
- Phase 1 plan created at `.planning/phases/01-build-baseline/01-PLAN.md`.
- TuyaOpen SDK confirmed at `/home/li/TuyaOpen` in WSL.
- Phase 1 build succeeded from `/home/li/TuyaOpen/apps/codex_quota`.
- Firmware output recorded at `/home/li/TuyaOpen/apps/codex_quota/dist/codex_quota_1.0.0`.
- Phase 2 config changes build cleanly after deleting SDK app `.build`.
- Project no longer contains the old default WiFi SSID/password.
- Bridge server contract found at `C:\Users\li\Desktop\Myproject\小米手环\codex_bridge_server.py`.

## Open Questions

- What is the current LAN IP of the PC bridge host?
- Which COM port is the T5AI-Board using for flash/debug?

## Next Action

Prepare hardware flash/debug.

Primary issues:

- Set real `WIFI_SSID` and `WIFI_PASSWORD` through TuyaOpen config.
- Confirm current PC LAN IP for `BRIDGE_HOST`.
- Flash `codex_quota_QIO_1.0.0.bin` or use `tos.py flash`.
- Capture serial logs for WiFi, HTTP, JSON parse, and offline fallback behavior.
