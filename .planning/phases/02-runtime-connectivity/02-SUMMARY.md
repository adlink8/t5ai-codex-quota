# Phase 2 Summary: Runtime Connectivity

**Updated:** 2026-06-07
**Result:** Configuration implementation complete; hardware runtime verification pending

## Completed

- Removed default WiFi SSID/password from `codex_quota_t5/Kconfig`.
- Added configurable `BRIDGE_PATH` to `Kconfig`.
- Removed hard-coded `BRIDGE_HOST`, `BRIDGE_PORT`, and `BRIDGE_PATH` overrides from `tuya_main.c`.
- Added fallback macros and runtime config guards in `tuya_main.c`.
- Added bridge request logging before HTTP polling.
- Deleted generated `codex_quota_t5/tuya_kconfig.h` because it contained stale credentials and is auto-generated.
- Added `tuya_kconfig.h` to `.gitignore`.
- Updated `codex_quota_t5/README.md` to configure WiFi/bridge through TuyaOpen config instead of source edits.
- Restored clean-build LCD/LVGL/TAL/HTTP/cJSON config in `app_default.config`.

## Verified

Clean build command:

```powershell
wsl -e bash -lc 'cd /home/li/TuyaOpen && . ./export.sh && cd apps/codex_quota && tos.py build'
```

Before the final build, `/home/li/TuyaOpen/apps/codex_quota/.build` was deleted so the test did not rely on stale config cache.

Build result:

```text
BUILD SUCCESS
Target    : codex_quota_QIO_1.0.0.bin
Output    : /home/li/TuyaOpen/apps/codex_quota/dist/codex_quota_1.0.0
Platform  : T5AI
Board     : TUYA_T5AI_BOARD
```

Generated config now contains:

```c
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define BRIDGE_HOST "192.168.1.109"
#define BRIDGE_PORT 5678
#define BRIDGE_PATH "/quota"
```

Project-wide search found no remaining `Password@549` or `TP-LINK_5G_7239`.

## Bridge Contract

Bridge server found at:

```text
C:\Users\li\Desktop\Myproject\小米手环\codex_bridge_server.py
```

The firmware parser matches the bridge `/quota` JSON fields:

- `plan_type`
- `primary.label`
- `primary.used_percent`
- `primary.remaining_percent`
- `primary.resets_in`
- optional `secondary`
- `updated_at`

## Pending Hardware Runtime Verification

These still require flash/debug on the actual T5AI-Board:

- WiFi connects and logs assigned IP.
- HTTP fetch reaches PC bridge over LAN.
- Offline fallback is visible after bridge/network failure.
- Parsed data updates the LCD UI.

