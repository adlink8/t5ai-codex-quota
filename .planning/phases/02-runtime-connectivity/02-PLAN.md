# Phase 2: Runtime Connectivity - Plan

**Status:** Partially executed
**Created:** 2026-06-07
**Requirements:** CONF-01, CONF-02, CONF-03, FW-01, FW-02, FW-03, FW-04, FW-05

## Goal

Make WiFi and bridge polling configurable and safe enough for hardware validation.

## Execution Steps

1. Remove committed/default WiFi credentials from project config.
2. Keep WiFi credentials supplied through TuyaOpen Kconfig/app config.
3. Move bridge host, port, and path to Kconfig-generated macros.
4. Add runtime guards for missing WiFi SSID and missing bridge config.
5. Compare firmware JSON parser against the PC bridge server contract.
6. Rebuild from a clean SDK app `.build` cache.
7. Defer real WiFi/HTTP/LCD log validation until hardware flash/debug.

## Verification

- Clean SDK app build must pass after deleting `/home/li/TuyaOpen/apps/codex_quota/.build`.
- Generated `tuya_kconfig.h` must not contain old WiFi SSID/password.
- Generated `tuya_kconfig.h` must expose `BRIDGE_HOST`, `BRIDGE_PORT`, and `BRIDGE_PATH`.

