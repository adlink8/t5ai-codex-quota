# Phase 1: Build Baseline - Plan

**Status:** Ready for execution
**Created:** 2026-06-07
**Requirements:** BUILD-01, BUILD-02, BUILD-03, BUILD-04

## Goal

Confirm the current `codex_quota_t5` app can build inside a TuyaOpen SDK checkout for T5AI-Board, and record the exact command path and firmware output.

## Non-Goals

- Do not flash hardware.
- Do not validate runtime WiFi/HTTP behavior.
- Do not redesign the LVGL UI.
- Do not solve WiFi secret hygiene unless the build requires touching config.

## Execution Steps

### 1. Locate TuyaOpen SDK

Run targeted searches first:

```powershell
Get-ChildItem C:\Users\li -Directory -Filter TuyaOpen -Recurse -ErrorAction SilentlyContinue
Get-ChildItem C:\Users\li\Desktop,C:\Users\li\Documents,D:\ -Directory -Filter TuyaOpen -Recurse -ErrorAction SilentlyContinue
```

Validate candidate SDK directory:

```powershell
Test-Path .\tos.py
Test-Path .\export.ps1
Test-Path .\apps
```

Deliverable:

- Record selected SDK path in `.planning/phases/01-build-baseline/01-BUILD-NOTES.md`.

### 2. Stage App Into SDK

Preferred path:

```powershell
Copy-Item -Recurse -Force C:\Users\li\Desktop\Myproject\TuYa\codex_quota_t5 <TuyaOpen>\apps\codex_quota
```

If the destination already exists:

- Inspect it first.
- If it is an older copy of the same app, update it.
- If unrelated, choose `apps\codex_quota_t5` and record the name used.

Deliverable:

- App is present in SDK `apps/`.
- No parent workspace files are modified.

### 3. Configure T5AI-Board Build

From the SDK root, run the SDK environment/bootstrap command appropriate for the platform.

Expected Windows/PowerShell path from existing README:

```powershell
.\export.ps1
python .\tos.py config
```

If `python .\tos.py` fails but `tos.py` is executable through another launcher, record the working invocation.

Configuration choices:

- Board: T5AI-Board.
- App: copied `codex_quota` or `codex_quota_t5`.
- Defaults: start from `codex_quota_t5/app_default.config`.

Deliverable:

- Capture selected board/app/config state in build notes.

### 4. Build And Capture Output

Run:

```powershell
python .\tos.py build
```

If build fails:

1. Classify failure:
   - SDK/toolchain missing.
   - App not discoverable.
   - Missing include/library.
   - C API mismatch.
   - Config/Kconfig issue.
2. Fix the smallest app-side issue.
3. Re-copy app into SDK if source changed.
4. Re-run build.

Deliverable:

- Successful build output path, or a precise blocker with failing command and first actionable error.

### 5. Update Project Docs

If build succeeds:

- Update `codex_quota_t5/README.md` with verified commands and output path.
- Add `.planning/phases/01-build-baseline/01-BUILD-NOTES.md`.
- Update `.planning/STATE.md` current status.
- Mark BUILD-01 through BUILD-04 complete in `.planning/REQUIREMENTS.md` only if each is actually satisfied.

If build cannot run because SDK is missing:

- Add `01-BUILD-NOTES.md` with the SDK blocker.
- Leave requirements pending.
- Update `.planning/STATE.md` with the blocker and next action.

## Verification

Minimum verification for completion:

```powershell
# From SDK root
python .\tos.py build
```

Completion requires:

- Exit code 0.
- Firmware/build artifact path identified.
- App path inside SDK recorded.
- Any app-side source/config changes reflected in the project copy.

## Risk Controls

- Do not delete SDK folders.
- Do not overwrite an unrelated app under SDK `apps/`.
- Do not commit or add new WiFi secrets.
- Keep changes scoped to `C:\Users\li\Desktop\Myproject\TuYa` unless copying into the SDK is required for build.

## Expected Outputs

- `.planning/phases/01-build-baseline/01-BUILD-NOTES.md`
- Updated `codex_quota_t5/README.md` if commands are verified.
- Updated `.planning/STATE.md`
- Optionally updated app source/config files if compiler output requires fixes.

