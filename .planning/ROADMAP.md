# Roadmap: TuYa

**Created:** 2026-06-07
**Granularity:** Coarse

## Phase 1: Build Baseline

**Goal:** Confirm the current `codex_quota_t5` app can build inside TuyaOpen for T5AI-Board.

**Scope:**
- Locate or document the expected TuyaOpen SDK path.
- Copy or link `codex_quota_t5` into the SDK app directory.
- Run configuration and build commands.
- Fix only build-blocking include/config/source issues.
- Record successful command sequence and firmware output path.

**Requirements:** BUILD-01, BUILD-02, BUILD-03, BUILD-04

**Done When:**
- Build command succeeds for T5AI-Board.
- Build output path is documented.
- Any source/config changes are captured in the project.

## Phase 2: Runtime Connectivity

**Goal:** Make WiFi and bridge polling reliable enough for hardware validation.

**Scope:**
- Verify WiFi configuration path through Kconfig/build config.
- Make bridge host/port/path easy to set for the user's LAN.
- Confirm HTTP request shape matches the PC bridge `/quota` response.
- Verify JSON parsing handles primary-only and primary+secondary payloads.
- Preserve offline fallback and bounded retry behavior.

**Requirements:** CONF-01, CONF-02, CONF-03, FW-01, FW-02, FW-03, FW-04, FW-05

**Done When:**
- Firmware can be configured for the current LAN without accidental secret commits.
- Logs prove WiFi connection, HTTP fetch, parse success, and offline fallback behavior.

## Phase 3: LCD UI Verification

**Goal:** Validate that the LCD UI is readable and accurately reflects quota state.

**Scope:**
- Test LVGL initialization order on real hardware.
- Verify ring rendering, labels, colors, and offline state on 480x320 screen.
- Adjust layout only where hardware screenshots/logs show a concrete issue.
- Document UI behavior and any known limitations.

**Requirements:** UI-01, UI-02, UI-03, UI-04

**Done When:**
- Hardware screen shows expected primary/secondary quota state.
- Offline and refresh status are visible.
- UI issues found during validation are either fixed or documented.

## Phase 4: Flash And Debug Workflow

**Goal:** Make future build/flash/debug runs reproducible from one place.

**Scope:**
- Verify `tos.py flash` or bundled `tyutool` path.
- Document serial port assumptions and log capture commands.
- Capture common failure modes and first diagnostic checks.
- Update README with the final workflow.

**Requirements:** FLASH-01, FLASH-02, FLASH-03

**Done When:**
- A user can build, flash, run, and inspect logs from documented commands.
- Project README points to the canonical workflow.
- `.planning/STATE.md` reflects completed phase status.

## Next Command

Start with:

```powershell
# GSD-style next step
$gsd-plan-phase 1
```

If running manually, begin Phase 1 by locating the TuyaOpen SDK checkout and running the app build.

