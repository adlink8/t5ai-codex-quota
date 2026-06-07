# Phase 1: Build Baseline - Context

**Gathered:** 2026-06-07
**Status:** Ready for planning

<domain>
## Phase Boundary

This phase only establishes a reproducible TuyaOpen build baseline for `codex_quota_t5`.

It does not validate WiFi runtime, bridge polling, LCD appearance, flashing, or hardware logs except where those facts are needed to confirm build configuration.
</domain>

<decisions>
## Implementation Decisions

### Build Target

- Treat `codex_quota_t5` as a TuyaOpen app that must be copied or linked into a TuyaOpen SDK `apps/` directory.
- Use T5AI-Board as the target board.
- Start from existing `codex_quota_t5/app_default.config`, `Kconfig`, and `CMakeLists.txt`.

### Change Scope

- Fix only build-blocking issues in Phase 1.
- Avoid runtime behavior refactors unless the compiler forces a source change.
- Do not edit parent workspace files outside `C:\Users\li\Desktop\Myproject\TuYa`.

### the agent's Discretion

- Choose the least invasive SDK integration method after locating the SDK: copy if symlink/junction behavior is uncertain, link if the SDK tooling handles it cleanly.
- Add small build notes to README when a command is verified.
- If SDK is missing, stop at a documented blocker instead of fabricating build success.
</decisions>

<canonical_refs>
## Canonical References

Downstream agents MUST read these before implementing.

### Project Planning

- `.planning/PROJECT.md` - project purpose, constraints, and active requirements.
- `.planning/REQUIREMENTS.md` - BUILD-01 through BUILD-04 acceptance criteria.
- `.planning/ROADMAP.md` - Phase 1 boundary and done conditions.

### Firmware App

- `codex_quota_t5/README.md` - current app-level build instructions.
- `codex_quota_t5/CMakeLists.txt` - current source list, include paths, and LVGL compile definitions.
- `codex_quota_t5/Kconfig` - app config surface.
- `codex_quota_t5/app_default.config` - target board and library defaults.
- `codex_quota_t5/src/tuya_main.c` - Tuya app entrypoint and API usage.
- `codex_quota_t5/src/codex_http.c` - HTTP client and JSON dependency surface.
- `codex_quota_t5/src/codex_ui.c` - LVGL dependency surface.
</canonical_refs>

<specifics>
## Specific Ideas

- First command should locate the SDK, for example:

```powershell
Get-ChildItem C:\Users\li -Recurse -Directory -Filter TuyaOpen -ErrorAction SilentlyContinue
```

- If the SDK is found, verify whether `tos.py`, `export.ps1`, and `apps/` exist before modifying anything.
- The current `CMakeLists.txt` contains a hard-coded WSL include path: `/home/li/TuyaOpen/src/tal_wifi/include`. Treat this as a likely build portability issue.
- The current `Kconfig` contains default WiFi credentials. Do not add more secrets during this phase; schedule cleanup in Phase 2 if not fixed now.
</specifics>

<deferred>
## Deferred Ideas

- Move bridge host/port/path fully into config.
- Remove committed WiFi password defaults.
- Flash device and inspect serial logs.
- Validate LCD rendering on real hardware.
</deferred>

---
*Phase: 01-build-baseline*
*Context gathered: 2026-06-07*

