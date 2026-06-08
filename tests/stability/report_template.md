# Stability Test Report - T5AI-Board Codex Quota Monitor

---

## Test Information

| Field | Value |
|-------|-------|
| **Date** | YYYY-MM-DD |
| **Tester** | (name) |
| **Firmware Version** | (e.g., v1.2.3-qio) |
| **Board Serial** | (serial number) |

---

## Test Environment

| Component | Details |
|-----------|---------|
| **PC Model** | (e.g., Lenovo ThinkPad X1 Carbon) |
| **OS** | Windows 11 Pro 23H2 |
| **CPU** | (e.g., Intel i7-1365U) |
| **RAM** | (e.g., 16 GB) |
| **WiFi Router/AP** | (e.g., TP-Link Archer AX50) |
| **WiFi Band** | (2.4 GHz / 5 GHz) |
| **Board IP** | (e.g., 192.168.1.42) |
| **Mosquitto Version** | (e.g., 2.0.18) |
| **Python Version** | (e.g., 3.12.1) |
| **Bridge Server Commit** | (git hash) |

---

## Test Duration

| Field | Value |
|-------|-------|
| **Planned Duration** | 4 hours |
| **Actual Start** | YYYY-MM-DD HH:MM:SS |
| **Actual End** | YYYY-MM-DD HH:MM:SS |
| **Actual Duration** | Xh Ym Zs |
| **Board Uptime** | (from serial log) |

---

## Results by Scenario

### Scenario 1: Normal Operation (Baseline)

| Criterion | Threshold | Actual | Result |
|-----------|-----------|--------|--------|
| MQTT connection stability | Continuous 4h | (e.g., 4h 0m, 0 disconnects) | PASS / FAIL |
| UI responsiveness | No freeze/black screen | (observation) | PASS / FAIL |
| Heap memory trend | Stable | (min: X, avg: Y, trend: stable) | PASS / FAIL |
| Crash / watchdog reset | Zero | (count) | PASS / FAIL |

**Notes:**

(Any observations or anomalies)

---

### Scenario 2: Broker Restart Every 10 Minutes

| Criterion | Threshold | Actual | Result |
|-----------|-----------|--------|--------|
| Broker restart events | ~24 | (count) | -- |
| MQTT reconnect time | <= 60s | (avg: Xs, max: Ys) | PASS / FAIL |
| HTTP fallback activation | <= 30s | (avg: Xs, max: Ys) | PASS / FAIL |
| HTTP success ratio | > 90% | (X/Y = Z%) | PASS / FAIL |
| Crash / watchdog reset | Zero | (count) | PASS / FAIL |

**Reconnect times (seconds):**

| Event # | Reconnect Time (s) | Notes |
|---------|-------------------|-------|
| 1 | | |
| 2 | | |
| ... | | |

---

### Scenario 3: Bridge Server Restart Every 30 Minutes

| Criterion | Threshold | Actual | Result |
|-----------|-----------|--------|--------|
| Bridge restart events | ~8 | (count) | -- |
| Data delivery continuity | Resumes after restart | (observation) | PASS / FAIL |
| MQTT connection impact | No disconnect | (observation) | PASS / FAIL |
| Crash / watchdog reset | Zero | (count) | PASS / FAIL |

**Notes:**

(Any observations or anomalies)

---

### Scenario 4: WiFi Hotspot Temporary Disconnect

| Criterion | Threshold | Actual | Result |
|-----------|-----------|--------|--------|
| WiFi outage events | ~16 | (count) | -- |
| WiFi reconnect time | <= 60s after restore | (avg: Xs, max: Ys) | PASS / FAIL |
| MQTT reconnect time | <= 60s after WiFi back | (avg: Xs, max: Ys) | PASS / FAIL |
| HTTP fallback activation | Activates during outage | (count) | PASS / FAIL |
| Crash / watchdog reset | Zero | (count) | PASS / FAIL |

**Notes:**

(Any observations or anomalies)

---

### Scenario 5: Combined Fault Injection

| Criterion | Threshold | Actual | Result |
|-----------|-----------|--------|--------|
| MQTT reconnect time | <= 60s | (avg: Xs, max: Ys) | PASS / FAIL |
| HTTP fallback activation | <= 30s | (avg: Xs, max: Ys) | PASS / FAIL |
| UI responsiveness | No freeze/black screen | (observation) | PASS / FAIL |
| Cascading failures | None | (observation) | PASS / FAIL |
| Crash / watchdog reset | Zero | (count) | PASS / FAIL |

**Notes:**

(Any observations or anomalies)

---

## Metrics Summary

| Metric | Scenario 1 | Scenario 2 | Scenario 3 | Scenario 4 | Scenario 5 |
|--------|-----------|-----------|-----------|-----------|-----------|
| MQTT disconnect count | | | | | |
| MQTT reconnect count | | | | | |
| Fallback activations | | | | | |
| HTTP success | | | | | |
| HTTP fail | | | | | |
| HTTP success ratio | | | | | |
| WiFi disconnect count | | | | | |
| Backoff events | | | | | |
| Heap min (bytes) | | | | | |
| Heap avg (bytes) | | | | | |
| Uptime (seconds) | | | | | |

---

## Log Excerpts for Failures

> Paste relevant serial log excerpts for any failed criteria or anomalies here.

### Failure 1: (description)

```
(timestamp) log line 1
(timestamp) log line 2
...
```

### Failure 2: (description)

```
(timestamp) log line 1
(timestamp) log line 2
...
```

(If no failures occurred, state: "No failures observed during testing.")

---

## Conclusion

**Overall Result:** PASS / FAIL

**Rationale:**

(Explain why the test passed or failed. Reference specific criteria that were not met, if any. Note any patterns or trends observed.)

**Recommendations:**

(List any recommended follow-up actions, bug fixes, or additional tests.)

---

## Attachments

- [ ] Serial monitor log file
- [ ] Broker restart CSV log
- [ ] Bridge server log (if applicable)
- [ ] Screenshots of any UI issues
- [ ] Photos of board setup (optional)
