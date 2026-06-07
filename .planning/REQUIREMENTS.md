# Requirements: TuYa

**Defined:** 2026-06-07
**Core Value:** Make the T5AI-Board reliably display useful local AI/tooling status from a PC bridge with a repeatable build, flash, and debug workflow.

## v1 Requirements

### Build

- [x] **BUILD-01**: App can be copied into a TuyaOpen SDK `apps/` directory and selected for T5AI-Board build.
- [x] **BUILD-02**: Build completes without missing Tuya/LVGL/cJSON include errors.
- [x] **BUILD-03**: Build configuration documents required fonts, display name, WiFi, HTTP, and board dependencies.
- [x] **BUILD-04**: Generated firmware image path is recorded after successful build.

### Configuration

- [x] **CONF-01**: WiFi SSID and password are supplied through Kconfig or documented build config.
- [x] **CONF-02**: Bridge host, port, and path can be updated predictably for the user's LAN.
- [x] **CONF-03**: No secret WiFi password is committed into project docs or source by default.

### Firmware Runtime

- [ ] **FW-01**: Firmware initializes board hardware and LVGL display before creating UI objects.
- [ ] **FW-02**: Firmware connects to WiFi and logs assigned IP or failure state.
- [ ] **FW-03**: Firmware fetches quota JSON from the PC bridge over HTTP.
- [ ] **FW-04**: Firmware parses primary and optional secondary quota windows.
- [ ] **FW-05**: Firmware uses retry/backoff and shows offline UI state when fetch fails.

### UI

- [ ] **UI-01**: LCD shows primary quota remaining percentage.
- [ ] **UI-02**: LCD shows secondary quota remaining percentage when present.
- [ ] **UI-03**: LCD color thresholds communicate healthy/warning/critical quota levels.
- [ ] **UI-04**: LCD shows last-update or offline status clearly.

### Flash And Debug

- [ ] **FLASH-01**: Project documents a verified flash path using `tos.py flash` or bundled `tyutool`.
- [ ] **FLASH-02**: Serial/log output can be captured during boot and refresh cycles.
- [ ] **FLASH-03**: A failed build/flash/run can be diagnosed from documented commands and logs.

## v2 Requirements

### Productization

- **PROD-01**: Replace hard-coded bridge network settings with device-side configuration or provisioning flow.
- **PROD-02**: Add BLE/AP configuration mode for WiFi and bridge settings.
- **PROD-03**: Add multiple screen layouts or themes.
- **PROD-04**: Integrate additional TuyaOpen AI examples such as voice or camera.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Tuya cloud product launch | Prototype currently validates local firmware and LCD behavior first. |
| Mobile app panel | Not required for local quota monitor behavior. |
| OTA or factory provisioning | Serial flashing is sufficient for current hardware experiments. |
| Rebuilding PC bridge server | Existing companion bridge is assumed as an external dependency. |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| BUILD-01 | Phase 1 | Complete |
| BUILD-02 | Phase 1 | Complete |
| BUILD-03 | Phase 1 | Complete |
| BUILD-04 | Phase 1 | Complete |
| CONF-01 | Phase 2 | Complete |
| CONF-02 | Phase 2 | Complete |
| CONF-03 | Phase 2 | Complete |
| FW-01 | Phase 2 | Pending |
| FW-02 | Phase 2 | Pending |
| FW-03 | Phase 2 | Pending |
| FW-04 | Phase 2 | Pending |
| FW-05 | Phase 2 | Pending |
| UI-01 | Phase 3 | Pending |
| UI-02 | Phase 3 | Pending |
| UI-03 | Phase 3 | Pending |
| UI-04 | Phase 3 | Pending |
| FLASH-01 | Phase 4 | Pending |
| FLASH-02 | Phase 4 | Pending |
| FLASH-03 | Phase 4 | Pending |

**Coverage:**
- v1 requirements: 19 total
- Mapped to phases: 19
- Unmapped: 0

---
*Requirements defined: 2026-06-07*
*Last updated: 2026-06-07 after Phase 2 configuration verification*
