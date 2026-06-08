<#
.SYNOPSIS
    Monitor serial output from T5AI-Board during stability testing.

.DESCRIPTION
    Opens COM11 at 460800 baud, reads serial output continuously, logs all
    data to a timestamped file, and watches for key patterns related to
    MQTT connectivity, HTTP fallback, and reconnection backoff.

.PARAMETER port
    Serial port to monitor. Default: COM11.

.PARAMETER baud_rate
    Baud rate. Default: 460800.

.PARAMETER log_dir
    Directory for log output. Defaults to tests\stability\logs\.

.PARAMETER stop_flag_file
    Path to a file whose existence signals this script to stop.
    Used by run_full_test.ps1 for coordinated shutdown.

.EXAMPLE
    .\monitor_serial_log.ps1
    .\monitor_serial_log.ps1 -port COM3 -baud_rate 115200
#>

[CmdletBinding()]
param(
    [string]$port            = "COM11",
    [int]$baud_rate          = 460800,
    [string]$log_dir         = "",
    [string]$stop_flag_file  = ""
)

# ---------------------------------------------------------------------------
# Encoding setup
# ---------------------------------------------------------------------------
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

# Resolve paths
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not $log_dir) {
    $log_dir = Join-Path $scriptDir "logs"
}
if (-not $stop_flag_file) {
    $stop_flag_file = Join-Path $scriptDir ".stop_monitor"
}

# Ensure log directory exists
if (-not (Test-Path $log_dir)) {
    New-Item -ItemType Directory -Path $log_dir -Force | Out-Null
}

# Remove stale stop flag
if (Test-Path $stop_flag_file) {
    Remove-Item $stop_flag_file -Force
}

# ---------------------------------------------------------------------------
# Log file setup
# ---------------------------------------------------------------------------
$timestamp  = Get-Date -Format "yyyyMMdd_HHmmss"
$logFile    = Join-Path $log_dir "serial_monitor_${timestamp}.log"
$summaryFile = Join-Path $log_dir "serial_summary_${timestamp}.txt"

# ---------------------------------------------------------------------------
# Pattern definitions for monitoring
# ---------------------------------------------------------------------------
$patterns = @{
    "MQTT_disconnect"    = "MQTT.*断连|MQTT.*disconnect|MQTT.*DISCONNECT"
    "MQTT_reconnect"     = "MQTT.*重连|MQTT.*reconnect|MQTT.*RECONNECT"
    "HTTP_fallback"      = "HTTP"
    "backoff"            = "backoff|back.?off"
    "watchdog"           = "wdt|watchdog|WDT"
    "panic_crash"        = "panic|abort|Guru Meditation|assertion failed"
    "heap_info"          = "heap|free.*mem|mem.*free"
    "wifi_disconnect"    = "WiFi.*disconnect|wifi.*断|WiFi.*lost"
    "wifi_reconnect"     = "WiFi.*connect|wifi.*连|WiFi.*join"
}

$counters = @{}
foreach ($key in $patterns.Keys) {
    $counters[$key] = 0
}

$totalLines   = 0
$matchedLines = 0
$startTime    = Get-Date

# ---------------------------------------------------------------------------
# Open serial port
# ---------------------------------------------------------------------------
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " T5AI-Board Serial Monitor" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "Port         : $port"
Write-Host "Baud rate    : $baud_rate"
Write-Host "Log file     : $logFile"
Write-Host "Summary file : $summaryFile"
Write-Host "Stop flag    : $stop_flag_file"
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

try {
    $serialPort = New-Object System.IO.Ports.SerialPort $port, $baud_rate, "None", 8, "One"
    $serialPort.ReadTimeout = 1000
    $serialPort.Encoding = [System.Text.Encoding]::UTF8
    $serialPort.NewLine = "`n"
    $serialPort.Open()
    Write-Host "[OK] Serial port $port opened at $baud_rate baud." -ForegroundColor Green
} catch {
    Write-Host "[ERROR] Failed to open serial port ${port}: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "        Ensure the port exists and is not in use by another application." -ForegroundColor Yellow
    exit 1
}

# ---------------------------------------------------------------------------
# Main read loop
# ---------------------------------------------------------------------------
Write-Host "Monitoring serial output... Press Ctrl+C or create stop flag to end." -ForegroundColor Gray
Write-Host ""

$lineBuffer = ""

try {
    while ($true) {
        # Check stop flag
        if (Test-Path $stop_flag_file) {
            Write-Host ""
            Write-Host "[STOP] Stop flag detected. Shutting down monitor." -ForegroundColor Yellow
            break
        }

        # Try to read data
        try {
            $bytesToRead = $serialPort.BytesToRead
            if ($bytesToRead -gt 0) {
                $chunk = $serialPort.ReadExisting()
                $lineBuffer += $chunk

                # Process complete lines
                while ($lineBuffer -match "^(.*?)[\r\n]+(.*)$") {
                    $line = $Matches[1]
                    $lineBuffer = $Matches[2]

                    if ($line.Trim().Length -eq 0) { continue }

                    $totalLines++
                    $logTimestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
                    $logLine = "[$logTimestamp] $line"

                    # Write to log file
                    $logLine | Out-File -FilePath $logFile -Encoding UTF8 -Append

                    # Check patterns
                    $lineMatched = $false
                    foreach ($patternName in $patterns.Keys) {
                        if ($line -match $patterns[$patternName]) {
                            $counters[$patternName]++
                            $lineMatched = $true

                            # Color-code console output
                            $color = switch ($patternName) {
                                "MQTT_disconnect"  { "Red" }
                                "MQTT_reconnect"   { "Green" }
                                "HTTP_fallback"     { "Yellow" }
                                "backoff"           { "Magenta" }
                                "watchdog"          { "Red" }
                                "panic_crash"       { "Red" }
                                "heap_info"         { "DarkCyan" }
                                "wifi_disconnect"   { "Red" }
                                "wifi_reconnect"    { "Green" }
                                default             { "White" }
                            }

                            Write-Host "[$patternName] " -ForegroundColor $color -NoNewline
                            Write-Host $line -ForegroundColor White
                        }
                    }

                    if (-not $lineMatched) {
                        # Print non-matching lines in gray (truncate long lines)
                        $displayLine = if ($line.Length -gt 120) { $line.Substring(0, 120) + "..." } else { $line }
                        Write-Host "  $displayLine" -ForegroundColor DarkGray
                    }
                }
            } else {
                Start-Sleep -Milliseconds 100
            }
        } catch [System.TimeoutException] {
            # ReadTimeout is normal when no data arrives
            Start-Sleep -Milliseconds 100
        } catch {
            Write-Host "[WARN] Serial read error: $($_.Exception.Message)" -ForegroundColor Yellow
            Start-Sleep -Milliseconds 500
        }
    }
} catch {
    Write-Host "[ERROR] Unexpected error in main loop: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    # Clean up serial port
    if ($serialPort -and $serialPort.IsOpen) {
        $serialPort.Close()
        $serialPort.Dispose()
        Write-Host "[OK] Serial port closed." -ForegroundColor Green
    }

    # Remove stop flag
    if (Test-Path $stop_flag_file) {
        Remove-Item $stop_flag_file -Force
    }
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
$endTime  = Get-Date
$duration = $endTime - $startTime
$durationStr = "{0:D2}:{1:D2}:{2:D2}" -f $duration.Hours, $duration.Minutes, $duration.Seconds

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " Serial Monitor - SUMMARY" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "Duration             : $durationStr"
Write-Host "Total lines received : $totalLines"
Write-Host "Pattern matches      : $matchedLines"
Write-Host ""

Write-Host "Pattern Breakdown:" -ForegroundColor White
foreach ($patternName in ($patterns.Keys | Sort-Object)) {
    $count = $counters[$patternName]
    $color = if ($count -gt 0) { "Yellow" } else { "Gray" }
    Write-Host "  $($patternName.PadRight(20)) : $count" -ForegroundColor $color
}

Write-Host ""
Write-Host "Log file     : $logFile"
Write-Host "Summary file : $summaryFile"
Write-Host "==============================================" -ForegroundColor Cyan

# Write summary to file
$summaryContent = @"
T5AI-Board Serial Monitor Summary
==================================
Start Time       : $($startTime.ToString("yyyy-MM-dd HH:mm:ss"))
End Time         : $($endTime.ToString("yyyy-MM-dd HH:mm:ss"))
Duration         : $durationStr
Total Lines      : $totalLines

Pattern Counts:
"@

foreach ($patternName in ($patterns.Keys | Sort-Object)) {
    $summaryContent += "`n  $($patternName.PadRight(20)) : $($counters[$patternName])"
}

$summaryContent += @"

Log File: $logFile
==================================
"@

$summaryContent | Out-File -FilePath $summaryFile -Encoding UTF8
