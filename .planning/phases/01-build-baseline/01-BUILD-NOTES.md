# Phase 1 Build Notes

**Date:** 2026-06-07
**Result:** Build succeeded

## SDK

- SDK path: `/home/li/TuyaOpen`
- Host access: WSL
- TuyaOpen version reported by build: `dcb6d784-dirty`
- App path in SDK: `/home/li/TuyaOpen/apps/codex_quota`

## Verified Commands

Run from Windows PowerShell:

```powershell
wsl -e bash -lc 'cd /home/li/TuyaOpen && . ./export.sh && cd apps/codex_quota && tos.py build'
```

Equivalent inside WSL:

```bash
cd /home/li/TuyaOpen
. ./export.sh
cd apps/codex_quota
tos.py build
```

## Build Output

Build command exited with code 0.

Output directory:

```text
/home/li/TuyaOpen/apps/codex_quota/dist/codex_quota_1.0.0
```

Firmware artifacts:

```text
/home/li/TuyaOpen/apps/codex_quota/dist/codex_quota_1.0.0/codex_quota_QIO_1.0.0.bin
/home/li/TuyaOpen/apps/codex_quota/dist/codex_quota_1.0.0/codex_quota_UA_1.0.0.bin
/home/li/TuyaOpen/apps/codex_quota/dist/codex_quota_1.0.0/codex_quota_UG_1.0.0.bin
```

Build summary:

```text
BUILD SUCCESS
Target    : codex_quota_QIO_1.0.0.bin
Platform  : T5AI
Chip      : T5AI
Board     : TUYA_T5AI_BOARD
Framework : base
```

## App Config Changes Synced Back To Project

The SDK copy had already been adjusted to build successfully. These build-relevant changes were synced back into the project copy:

- Removed hard-coded `/home/li/TuyaOpen/src/tal_wifi/include` from `codex_quota_t5/CMakeLists.txt`.
- Replaced obsolete board/library defaults in `codex_quota_t5/app_default.config` with SDK-recognized T5AI defaults:
  - `CONFIG_BOARD_CHOICE_T5AI=y`
  - `CONFIG_TUYA_T5AI_BOARD_EX_MODULE_NONE=y`
  - `CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=4096`

## Warnings

Build produced SDK/platform warnings from TuyaOpen/T5AI internals, including CMake deprecation warnings and adapter warnings. They did not stop the build and were not app-side blockers.

## Follow-Up For Phase 2

- `codex_quota_t5/Kconfig` still contains default WiFi credentials. Remove or neutralize committed secrets in the runtime connectivity phase.
- Bridge host remains environment-specific and should be configurable for the current LAN.

