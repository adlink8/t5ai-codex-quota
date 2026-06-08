<#
.SYNOPSIS
    Master script to run the full T5AI-Board stability test.

.DESCRIPTION
    Orchestrates the stability test by launching the serial monitor and
    broker restart loop in background, waiting for the specified duration,
    then signaling both scripts to stop and collecting results.

.PARAMETER duration_hours
    Total test duration in hours. Default: 4.

.PARAMETER broker_interval_minutes
    Interval between broker restarts in minutes. Default: 10.

.PARAMETER port
    Serial port for monitoring. Default: COM11.

.PARAMETER baud_rate
    Serial baud rate. Default: 460800.

.PARAMETER skip_broker_restart
    If set, skip the broker restart loop (for Scenario 1 baseline test).

.PARAMETER skip_serial_monitor
    If set, skip the serial monitor (if running externally).

.EXAMPLE
    .\run_full_test.ps1
    .\run_full_test.ps1 -duration_hours 2 -broker_interval_minutes 5
    .\run_full_test.ps1 -skip_broker_restart   # Baseline test (Scenario 1)
#>

[CmdletBinding()]
param(
    [double]$duration_hours          = 4,
    [double]$broker_interval_minutes = 10,
    [string]$port                    = "COM11",
    [int]$baud_rate                  = 460800,
    [switch]$skip_broker_restart,
    [switch]$skip_serial_monitor
)

# ---------------------------------------------------------------------------
# Encoding setup
# ---------------------------------------------------------------------------
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

# ---------------------------------------------------------------------------
# Path setup
# ---------------------------------------------------------------------------
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$logDir    = Join-Path $scriptDir "logs"

# Ensure log directory exists
if (-not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir -Force | Out-Null
}

# Stop flag files for coordinating child scripts
$monitorStopFlag = Join-Path $scriptDir ".stop_monitor"
$brokerStopFlag  = Join-Path $scriptDir ".stop_broker"

# Clean up any stale stop flags
foreach ($flag in @($monitorStopFlag, $brokerStopFlag)) {
    if (Test-Path $flag) {
        Remove-Item $flag -Force
    }
}

# ---------------------------------------------------------------------------
# Banner
# ---------------------------------------------------------------------------
$startTime = Get-Date
$endTime   = $startTime.AddHours($duration_hours)

Write-Host ""
Write-Host "######################################################" -ForegroundColor Cyan
Write-Host "#                                                    #" -ForegroundColor Cyan
Write-Host "#   T5AI-Board Codex Quota Monitor - Stability Test  #" -ForegroundColor Cyan
Write-Host "#                                                    #" -ForegroundColor Cyan
Write-Host "######################################################" -ForegroundColor Cyan
Write-Host ""
Write-Host "Start time        : $($startTime.ToString('yyyy-MM-dd HH:mm:ss'))"
Write-Host "End time (target) : $($endTime.ToString('yyyy-MM-dd HH:mm:ss'))"
Write-Host "Duration          : $duration_hours hours"
Write-Host "Broker interval   : $broker_interval_minutes minutes"
Write-Host "Serial port       : $port @ $baud_rate"
Write-Host "Log directory     : $logDir"
Write-Host ""

if ($skip_broker_restart) {
    Write-Host "[NOTE] Broker restart loop DISABLED (baseline test)." -ForegroundColor Yellow
}
if ($skip_serial_monitor) {
    Write-Host "[NOTE] Serial monitor DISABLED (running externally)." -ForegroundColor Yellow
}
Write-Host ""

# ---------------------------------------------------------------------------
# Launch child processes
# ---------------------------------------------------------------------------
$monitorJob = $null
$brokerJob  = $null

# --- Start serial monitor ---
if (-not $skip_serial_monitor) {
    Write-Host "[1/3] Starting serial monitor..." -ForegroundColor Green

    $monitorScript = Join-Path $scriptDir "monitor_serial_log.ps1"
    if (-not (Test-Path $monitorScript)) {
        Write-Host "[ERROR] monitor_serial_log.ps1 not found at: $monitorScript" -ForegroundColor Red
        exit 1
    }

    $monitorJob = Start-Job -ScriptBlock {
        param($scriptPath, $portParam, $baudParam, $logDirParam, $stopFlag)
        & $scriptPath -port $portParam -baud_rate $baudParam -log_dir $logDirParam -stop_flag_file $stopFlag
    } -ArgumentList $monitorScript, $port, $baud_rate, $logDir, $monitorStopFlag

    Write-Host "  Serial monitor started (Job ID: $($monitorJob.Id))." -ForegroundColor Gray
    Start-Sleep -Seconds 2
} else {
    Write-Host "[1/3] Serial monitor skipped." -ForegroundColor Yellow
}

# --- Start broker restart loop ---
if (-not $skip_broker_restart) {
    Write-Host "[2/3] Starting broker restart loop..." -ForegroundColor Green

    $brokerScript = Join-Path $scriptDir "restart_broker_loop.ps1"
    if (-not (Test-Path $brokerScript)) {
        Write-Host "[ERROR] restart_broker_loop.ps1 not found at: $brokerScript" -ForegroundColor Red
        exit 1
    }

    $brokerJob = Start-Job -ScriptBlock {
        param($scriptPath, $intervalParam, $durationParam, $logDirParam)
        & $scriptPath -interval_minutes $intervalParam -duration_hours $durationParam -log_dir $logDirParam
    } -ArgumentList $brokerScript, $broker_interval_minutes, $duration_hours, $logDir

    Write-Host "  Broker restart loop started (Job ID: $($brokerJob.Id))." -ForegroundColor Gray
    Start-Sleep -Seconds 2
} else {
    Write-Host "[2/3] Broker restart loop skipped." -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
# Wait for duration with progress updates
# ---------------------------------------------------------------------------
Write-Host "[3/3] Test running. Progress updates every 5 minutes..." -ForegroundColor Green
Write-Host ""

$progressInterval = 5  # minutes
$nextProgress = (Get-Date).AddMinutes($progressInterval)

while ((Get-Date) -lt $endTime) {
    Start-Sleep -Seconds 30

    # Check if child jobs are still running
    if ($monitorJob -and $monitorJob.State -eq "Completed") {
        Write-Host "[WARN] Serial monitor job ended prematurely." -ForegroundColor Yellow
        $monitorJob = $null
    }
    if ($brokerJob -and $brokerJob.State -eq "Completed") {
        Write-Host "[WARN] Broker restart job ended prematurely." -ForegroundColor Yellow
        $brokerJob = $null
    }

    # Progress update
    if ((Get-Date) -ge $nextProgress) {
        $elapsed = (Get-Date) - $startTime
        $remaining = $endTime - (Get-Date)
        $pct = [math]::Min(100, [math]::Round(($elapsed.TotalHours / $duration_hours) * 100, 1))

        $elapsedStr   = "{0:D2}:{1:D2}:{2:D2}" -f $elapsed.Hours, $elapsed.Minutes, $elapsed.Seconds
        $remainingStr = "{0:D2}:{1:D2}:{2:D2}" -f [math]::Max(0,$remaining.Hours), [math]::Max(0,$remaining.Minutes), [math]::Max(0,$remaining.Seconds)

        Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Progress: $pct% | Elapsed: $elapsedStr | Remaining: $remainingStr" -ForegroundColor Cyan

        $nextProgress = $nextProgress.AddMinutes($progressInterval)
    }
}

# ---------------------------------------------------------------------------
# Signal child scripts to stop
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "==============================================" -ForegroundColor Yellow
Write-Host " Test duration reached. Stopping child tasks." -ForegroundColor Yellow
Write-Host "==============================================" -ForegroundColor Yellow

# Signal serial monitor to stop via flag file
if ($monitorJob) {
    Write-Host "Signaling serial monitor to stop..." -ForegroundColor Yellow
    "stop" | Out-File -FilePath $monitorStopFlag -Encoding UTF8
    Start-Sleep -Seconds 3

    if ($monitorJob.State -eq "Running") {
        Write-Host "Force-stopping serial monitor job..." -ForegroundColor Yellow
        Stop-Job -Job $monitorJob
    }
}

# Signal broker restart loop to stop via flag file
# (The broker script runs in a foreground loop; we stop it by stopping the job)
if ($brokerJob) {
    if ($brokerJob.State -eq "Running") {
        Write-Host "Stopping broker restart loop job..." -ForegroundColor Yellow
        Stop-Job -Job $brokerJob
    }
}

Start-Sleep -Seconds 2

# Ensure Mosquitto is left running after test
Write-Host "Ensuring Mosquitto is running (post-test cleanup)..." -ForegroundColor Gray
$mosqRunning = Get-Process -Name "mosquitto" -ErrorAction SilentlyContinue
if (-not $mosqRunning) {
    Write-Host "Restarting Mosquitto..." -ForegroundColor Yellow
    $projectRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
    $confPath = Join-Path $projectRoot "bridge_server\mosquitto.conf"
    $mosqExe = (Get-Command "mosquitto" -ErrorAction SilentlyContinue).Source
    if ($mosqExe -and (Test-Path $confPath)) {
        Start-Process -FilePath $mosqExe -ArgumentList "-c", "`"$confPath`"" -WindowStyle Hidden
    }
}

# Clean up stop flags
foreach ($flag in @($monitorStopFlag, $brokerStopFlag)) {
    if (Test-Path $flag) {
        Remove-Item $flag -Force
    }
}

# ---------------------------------------------------------------------------
# Collect and display results
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "######################################################" -ForegroundColor Cyan
Write-Host "#              STABILITY TEST COMPLETE                #" -ForegroundColor Cyan
Write-Host "######################################################" -ForegroundColor Cyan
Write-Host ""

$actualEnd   = Get-Date
$totalTime   = $actualEnd - $startTime
$totalStr    = "{0:D2}:{1:D2}:{2:D2}" -f $totalTime.Hours, $totalTime.Minutes, $totalTime.Seconds

Write-Host "Actual duration : $totalStr"
Write-Host "Start           : $($startTime.ToString('yyyy-MM-dd HH:mm:ss'))"
Write-Host "End             : $($actualEnd.ToString('yyyy-MM-dd HH:mm:ss'))"
Write-Host ""

# Display job output summaries
if ($monitorJob) {
    Write-Host "--- Serial Monitor Output (last 20 lines) ---" -ForegroundColor White
    $monitorOutput = Receive-Job -Job $monitorJob -ErrorAction SilentlyContinue
    if ($monitorOutput) {
        $lines = $monitorOutput -split "`n"
        $tail = $lines | Select-Object -Last 20
        $tail | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    } else {
        Write-Host "  (no output captured)" -ForegroundColor DarkGray
    }
    Remove-Job -Job $monitorJob -Force -ErrorAction SilentlyContinue
    Write-Host ""
}

if ($brokerJob) {
    Write-Host "--- Broker Restart Output (last 20 lines) ---" -ForegroundColor White
    $brokerOutput = Receive-Job -Job $brokerJob -ErrorAction SilentlyContinue
    if ($brokerOutput) {
        $lines = $brokerOutput -split "`n"
        $tail = $lines | Select-Object -Last 20
        $tail | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    } else {
        Write-Host "  (no output captured)" -ForegroundColor DarkGray
    }
    Remove-Job -Job $brokerJob -Force -ErrorAction SilentlyContinue
    Write-Host ""
}

# List generated log files
Write-Host "--- Generated Log Files ---" -ForegroundColor White
$logFiles = Get-ChildItem -Path $logDir -File | Sort-Object LastWriteTime -Descending | Select-Object -First 10
if ($logFiles) {
    foreach ($f in $logFiles) {
        $sizeKB = [math]::Round($f.Length / 1024, 1)
        Write-Host "  $($f.Name) ($sizeKB KB)" -ForegroundColor Gray
    }
} else {
    Write-Host "  (no log files found)" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "######################################################" -ForegroundColor Cyan
Write-Host "# Next Steps:                                        #" -ForegroundColor Cyan
Write-Host "# 1. Review log files in: $logDir" -ForegroundColor Cyan
Write-Host "# 2. Fill in report_template.md with results" -ForegroundColor Cyan
Write-Host "# 3. File report as comment on Issue #7" -ForegroundColor Cyan
Write-Host "######################################################" -ForegroundColor Cyan
Write-Host ""
