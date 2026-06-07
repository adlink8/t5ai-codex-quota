# TuYa

## What This Is

TuYa is a local TuyaOpen/T5AI-Board experiment workspace. Its current concrete app is `codex_quota_t5`, a firmware port that shows Codex quota data on the T5AI-Board LCD through WiFi and a PC bridge server.

The workspace also keeps Tuya IoT research notes, T5AI-Board hardware references, and local flashing binaries so future firmware experiments can start from known context instead of rediscovery.

## Core Value

Make the T5AI-Board reliably display useful local AI/tooling status from a PC bridge with a repeatable build, flash, and debug workflow.

## Requirements

### Validated

- ✓ `codex_quota_t5` builds successfully inside WSL TuyaOpen SDK at `/home/li/TuyaOpen/apps/codex_quota` — Phase 1
- ✓ WiFi credentials and bridge endpoint are Kconfig/app-config driven without committed default WiFi secrets — Phase 2

### Active

- [ ] Flash a generated firmware image to T5AI-Board using `tos.py flash` or the bundled `tyutool` tools.
- [ ] Verify the LCD UI renders quota state, offline state, and refresh behavior on real hardware.
- [ ] Document one-line setup/build/flash/debug commands for future runs.

### Out of Scope

- Full Tuya cloud product lifecycle - current workspace focuses on local firmware experiments, not production cloud onboarding.
- Mobile app or MiniApp panel development - LCD firmware and local bridge integration are the current priority.
- OTA deployment and mass production tooling - serial flashing is enough for the prototype stage.
- Rewriting the PC bridge server - this app expects to reuse the existing `codex_bridge_server.py` from the companion project.

## Context

- Target hardware: Tuya T5AI-Board using the T5-E1-IPEX module.
- App stack: TuyaOpen, C/C++, CMake, LVGL, Tuya TAL WiFi/thread/system APIs, cJSON, HTTP polling.
- Current app path: `codex_quota_t5`.
- Current binary/tools path: `烧写工具`.
- Current architecture: T5AI-Board -> WiFi LAN -> PC bridge server -> ChatGPT/Codex quota source.
- Current UI intent: 480x320 LCD, black background, ring indicators for primary/secondary quota windows, color thresholding, offline status.
- Known hard-coded value: `BRIDGE_HOST` in `codex_quota_t5/src/tuya_main.c` is currently `192.168.1.109`.

## Constraints

- **Hardware**: Requires T5AI-Board or compatible T5 target - runtime behavior cannot be fully validated on the Windows host alone.
- **SDK**: Build requires TuyaOpen and `tos.py`; this folder is an app workspace, not a full SDK checkout.
- **Network**: Device and PC bridge must be on the same LAN; bridge IP/port/path must match firmware config.
- **Display**: UI depends on LVGL and board display initialization order.
- **Safety**: Do not run destructive cleanup or overwrite parent workspace files without explicit confirmation.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Keep `codex_quota_t5` as a TuyaOpen app folder | Matches TuyaOpen app-copy workflow and current README | Pending |
| Track planning docs in `.planning` | Keeps future GSD tasks grounded in local project state | ✓ Good |
| Treat bridge server as external dependency | Avoids duplicating companion project logic | Pending |
| Use coarse roadmap phases | Firmware work has a small number of high-value validation gates | ✓ Good |
| Use WSL SDK at `/home/li/TuyaOpen` | Actual local SDK path provided by user and verified by build | ✓ Good |
| Keep WiFi credentials out of defaults | Prevents accidental secret commits and forces environment-specific config | ✓ Good |

## Evolution

After each phase:

1. Move verified requirements to Validated.
2. Update Active requirements when hardware findings change scope.
3. Record build, flash, and runtime decisions in Key Decisions.
4. Keep paths and commands accurate for the current local machine.

---
*Last updated: 2026-06-07 after Phase 2 configuration verification*
