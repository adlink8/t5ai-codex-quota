# Stability Test Plan - T5AI-Board Codex Quota Monitor

**Document Version:** 1.0
**Last Updated:** 2026-06-08
**Status:** Draft

---

## 1. Test Objective

Verify the T5AI-Board Codex Quota Monitor firmware can operate stably for **4 continuous hours** under both normal conditions and with periodic fault injection. The test must demonstrate that the device:

- Maintains or recovers MQTT connectivity after broker outages.
- Falls back to HTTP data delivery when MQTT is unavailable.
- Keeps the UI responsive at all times (no freeze, no black screen).
- Does not crash, hang, or trigger a watchdog reset.
- Shows no signs of memory leaks (heap remains stable over the test duration).

---

## 2. Prerequisites

| Item | Requirement |
|------|-------------|
| **Board** | T5AI-Board flashed with the latest QIO firmware (record version before test) |
| **MQTT Broker** | Mosquitto installed and configured with `bridge_server\mosquitto.conf` |
| **Bridge Server** | `codex_bridge_server.py` running on the host PC |
| **Serial Debug** | COM11 connected, baud rate 460800, UTF-8 output |
| **WiFi** | Stable WiFi hotspot providing internet to the board |
| **Power** | Stable USB power supply to the board |
| **Scripts** | All `.ps1` scripts in this directory available |
| **Disk Space** | At least 500 MB free for log files |

### Software Versions (record before test)

- Windows version: ____________________
- Mosquitto version: ____________________
- Python version (bridge server): ____________________
- Firmware version: ____________________
- Board IP address: ____________________

---

## 3. Test Scenarios

### Scenario 1: Normal Operation (Baseline)

- **Duration:** 4 hours continuous
- **Description:** No fault injection. MQTT broker, bridge server, and WiFi remain active throughout.
- **Expected behavior:**
  - MQTT connection stays up for the entire duration.
  - UI updates quota data periodically.
  - Heap memory remains stable (no monotonic decrease).
  - Zero watchdog resets.

### Scenario 2: Broker Restart Every 10 Minutes

- **Duration:** 4 hours
- **Fault:** Stop Mosquitto, wait 5 seconds, restart Mosquitto. Repeat every 10 minutes.
- **Script:** `restart_broker_loop.ps1 -interval_minutes 10 -duration_hours 4`
- **Expected behavior:**
  - MQTT reconnects within **60 seconds** after each broker restart.
  - HTTP fallback activates within **30 seconds** of MQTT disconnect.
  - No crash during reconnection cycles.
  - Approximately 24 broker restart events over 4 hours.

### Scenario 3: Bridge Server Restart Every 30 Minutes

- **Duration:** 4 hours
- **Fault:** Stop the bridge server process, wait 10 seconds, restart it. Repeat every 30 minutes.
- **Expected behavior:**
  - MQTT connection to broker is unaffected (broker stays up).
  - Bridge server re-establishes upstream connections after restart.
  - Board continues sending data; bridge processes it after recovery.
  - Approximately 8 bridge restart events over 4 hours.

### Scenario 4: WiFi Hotspot Temporary Disconnect

- **Duration:** 4 hours
- **Fault:** Disable WiFi hotspot for 30 seconds every 15 minutes.
- **Expected behavior:**
  - Board detects WiFi loss and enters reconnection loop with backoff.
  - WiFi reconnects within 60 seconds of hotspot restoration.
  - MQTT reconnects within 60 seconds after WiFi is back.
  - HTTP fallback activates during WiFi outage (once WiFi recovers).
  - Approximately 16 WiFi outage events over 4 hours.

### Scenario 5: Combined Fault Injection

- **Duration:** 4 hours
- **Fault:** Combine Scenarios 2 + 3 + 4 simultaneously.
- **Expected behavior:**
  - All individual pass criteria still apply.
  - No cascading failures or deadlock conditions.
  - Board recovers gracefully from overlapping faults.
  - UI remains responsive throughout.

---

## 4. Pass / Fail Criteria

| Criterion | Threshold | Measurement Method |
|-----------|-----------|-------------------|
| MQTT reconnect time | <= 60 seconds after broker restart | Serial log timestamps |
| HTTP fallback activation | <= 30 seconds after MQTT disconnect | Serial log "HTTP" pattern |
| UI responsiveness | No freeze or black screen observed | Visual inspection + serial heartbeat |
| Memory stability | Heap floor does not decrease monotonically | Serial heap monitoring logs |
| Crash / watchdog reset | Zero occurrences | Serial log "abort", "panic", "wdt" patterns |
| Uptime | >= 99.5% of test duration | Board uptime counter in serial log |

**Overall PASS:** All criteria met across all executed scenarios.
**Overall FAIL:** Any single criterion fails in any scenario.

---

## 5. Metrics to Collect

The following metrics are recorded by the monitoring scripts:

| Metric | Description | Source |
|--------|-------------|--------|
| `reconnect_count` | Number of MQTT reconnection events | Serial monitor pattern match |
| `disconnect_count` | Number of MQTT disconnection events | Serial monitor pattern match |
| `fallback_activations` | Number of HTTP fallback activations | Serial monitor "HTTP" pattern |
| `http_success` | Successful HTTP fallback requests | Serial log |
| `http_fail` | Failed HTTP fallback requests | Serial log |
| `http_success_ratio` | `http_success / (http_success + http_fail)` | Computed |
| `uptime_seconds` | Total board uptime | Serial log / board counter |
| `heap_min` | Minimum observed free heap | Serial log |
| `heap_avg` | Average free heap | Serial log |
| `wifi_disconnect_count` | Number of WiFi disconnection events | Serial log |
| `backoff_events` | Number of reconnection backoff attempts | Serial monitor "backoff" pattern |
| `broker_restart_count` | Number of broker restart events | Broker restart script log |

---

## 6. Execution Procedure

### Quick Start (Automated)

```powershell
# Run the full automated test (Scenario 2 as default fault injection)
cd tests\stability
.\run_full_test.ps1 -duration_hours 4
```

### Manual Scenario Execution

```powershell
# Terminal 1: Start serial monitor
.\monitor_serial_log.ps1

# Terminal 2: Start broker restart loop (Scenario 2)
.\restart_broker_loop.ps1 -interval_minutes 10 -duration_hours 4

# Terminal 3: Bridge server restart (Scenario 3, manual or scripted)
# Stop and restart codex_bridge_server.py every 30 minutes
```

### Post-Test

1. Stop all scripts.
2. Collect log files from `tests\stability\logs\`.
3. Fill in `report_template.md` with results.
4. File the report as a comment on Issue #7.

---

## 7. Risk and Mitigation

| Risk | Mitigation |
|------|-----------|
| Serial port disconnect during test | Use a USB hub with stable connection; check cable |
| Mosquitto fails to restart | Script includes retry logic and error logging |
| Bridge server crash | Monitor bridge process; manual restart if needed |
| Power loss to board | Use a UPS or stable power source |
| Disk full from logs | Ensure 500 MB+ free space before starting |

---

## 8. References

- Project repository: `C:\Users\li\Desktop\Myproject\TuYa`
- Firmware source: `codex_quota_t5\`
- Bridge server: `bridge_server\codex_bridge_server.py`
- Mosquitto config: `bridge_server\mosquitto.conf`
- GitHub Issue: #7
