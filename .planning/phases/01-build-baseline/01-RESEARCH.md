# Phase 1: Build Baseline - Research

**Date:** 2026-06-07
**Scope:** Local research from existing project files only.

## Findings

1. This workspace is an app/research workspace, not a full TuyaOpen SDK checkout.
   - Evidence: project root contains `codex_quota_t5`, docs, and flashing tools, but no SDK-level `tos.py`, `export.ps1`, or `apps/` folder.

2. Existing app build flow expects TuyaOpen.
   - `codex_quota_t5/README.md` says to copy the app into `~/TuyaOpen/apps/codex_quota`, then run `tos.py config` and `tos.py build`.

3. Build configuration already declares the intended board and libraries.
   - `app_default.config` enables T5AI-Board, LCD, LVGL v8, TAL WiFi, HTTP, cJSON, and TAL log.

4. `CMakeLists.txt` has one likely portability hazard.
   - It manually adds `/home/li/TuyaOpen/src/tal_wifi/include`, which may be wrong for the actual SDK path or Windows/WSL layout.

5. Source dependencies likely require SDK-side verification.
   - `tuya_main.c` uses Tuya TAL APIs, board APIs, LVGL vendor APIs, and cJSON.
   - `codex_http.c` depends on `http_client_interface.h` and its exact struct names/signatures.
   - These cannot be fully verified without the matching TuyaOpen headers.

6. A secret hygiene issue already exists but is outside Phase 1 unless it blocks build.
   - `Kconfig` has a default WiFi password. Phase 2 should remove or neutralize this.

## Implications For Plan

- The first implementation task must locate and validate the SDK.
- Build failure triage should separate SDK path/setup failures from app compile errors.
- If SDK is absent, Phase 1 can only produce a blocker report and cannot mark BUILD-02/BUILD-04 complete.
- Avoid large source refactors before seeing compiler output.

