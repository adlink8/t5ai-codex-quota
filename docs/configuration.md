# Configuration Guide

This document explains how to configure the Codex Quota Monitor project for different environments.

## Single Source of Truth: Kconfig + app_default.config

All runtime configuration (WiFi credentials, bridge server address, MQTT broker settings) is managed through the **Kconfig** system provided by the TuyaOpen SDK.

| File | Role |
|---|---|
| `codex_quota_t5/Kconfig` | Defines all configurable options with their types and defaults |
| `codex_quota_t5/app_default.config` | Stores the compiled-in default values for a clean checkout |

When you run `tos.py config menu`, the SDK reads `Kconfig` for the option definitions and writes your selections into the build system. The `app_default.config` file provides safe, non-sensitive defaults so the project builds out of the box.

## CMakeLists.txt Should NOT Contain Hardcoded IPs

The `CMakeLists.txt` file is for **build configuration only**: source file lists, include paths, and compile-time feature flags (such as LVGL font selections).

**Do not** add hardcoded IP addresses, WiFi passwords, or server addresses to `CMakeLists.txt`. These values belong in Kconfig because:

1. Kconfig values are stored per-build and are not committed to version control with sensitive data.
2. The `MQTT_HOST` and `MQTT_PORT` compile definitions in `CMakeLists.txt` are a legacy pattern. The firmware now reads these from the Kconfig-generated `tuya_kconfig.h` header at compile time.
3. Hardcoded IPs force every developer to edit `CMakeLists.txt`, which creates merge conflicts and accidental credential commits.

If you see a hardcoded IP like `MQTT_HOST=\"10.13.220.28\"` in `CMakeLists.txt`, it should be removed in favor of the Kconfig value.

## Using tos.py config menu

The TuyaOpen SDK provides an interactive configuration menu:

```bash
# From the TuyaOpen SDK root or app directory
tos.py config menu
```

This launches a text-based menu (similar to Linux `menuconfig`) where you can set:

### WiFi Configuration

| Option | Kconfig Key | Default | Description |
|---|---|---|---|
| WiFi SSID | `WIFI_SSID` | `""` (empty) | Your WiFi network name |
| WiFi Password | `WIFI_PASSWORD` | `""` (empty) | Your WiFi password |

### Bridge Server

| Option | Kconfig Key | Default | Description |
|---|---|---|---|
| Bridge Host | `BRIDGE_HOST` | `192.168.1.109` | IP address of the PC running the bridge server |
| Bridge Port | `BRIDGE_PORT` | `5678` | HTTP port of the bridge server |
| Bridge Path | `BRIDGE_PATH` | `/quota` | HTTP endpoint path for quota data |

### MQTT Configuration

| Option | Kconfig Key | Default | Description |
|---|---|---|---|
| MQTT Host | `MQTT_HOST` | `""` (empty) | IP address of the MQTT broker (usually same as Bridge Host) |
| MQTT Port | `MQTT_PORT` | `1883` | MQTT broker port |

## Example Configurations

### Home Network (typical)

```
WIFI_SSID = "MyHomeWiFi"
WIFI_PASSWORD = "********"
BRIDGE_HOST = "192.168.1.109"
BRIDGE_PORT = 5678
BRIDGE_PATH = "/quota"
MQTT_HOST = "192.168.1.109"
MQTT_PORT = 1883
```

### Office / Lab Network

```
WIFI_SSID = "OfficeNet"
WIFI_PASSWORD = "********"
BRIDGE_HOST = "10.13.220.28"
BRIDGE_PORT = 5678
BRIDGE_PATH = "/quota"
MQTT_HOST = "10.13.220.28"
MQTT_PORT = 1883
```

### Development (MQTT disabled, HTTP only)

```
WIFI_SSID = "DevWiFi"
WIFI_PASSWORD = "********"
BRIDGE_HOST = "192.168.0.50"
BRIDGE_PORT = 5678
BRIDGE_PATH = "/quota"
MQTT_HOST = ""
MQTT_PORT = 1883
```

When `MQTT_HOST` is empty, the firmware skips MQTT entirely and relies on HTTP polling with exponential backoff.

## Security Note

**Never commit real WiFi passwords or sensitive credentials to version control.**

- The `app_default.config` file in the repository should always contain empty strings for `WIFI_SSID` and `WIFI_PASSWORD`.
- Use `tos.py config menu` locally to set your real credentials. These are written to the build directory, not to the source tree.
- The `.gitignore` file excludes build outputs and sensitive files (`.env`, `*.secret`, `auth.json`).
- Before committing, verify that no real passwords or IP addresses appear in the diff:

```bash
git diff --staged
```

- If you accidentally commit a password, change it immediately and consider the old credential compromised.
