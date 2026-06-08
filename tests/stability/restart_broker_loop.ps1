<#
.SYNOPSIS
    Restart Mosquitto broker in a loop for stability testing.

.DESCRIPTION
    Periodically stops and restarts the Mosquitto MQTT broker to simulate
    broker outages during T5AI-Board stability tests. Each restart event
    is logged with a timestamp to a CSV file.

.PARAMETER interval_minutes
    Interval between broker restarts in minutes. Default: 10.

.PARAMETER duration_hours
    Total duration of the test in hours. Default: 4.

.PARAMETER mosquitto_conf
    Path to the mosquitto.conf file. Defaults to the project's config.

.PARAMETER log_dir
    Directory for log output. Defaults to tests\stability\logs\.

.EXAMPLE
    .\restart_broker_loop.ps1
    .\restart_broker_loop.ps1 -interval_minutes 5 -duration_hours 2
#>

[CmdletBinding()]
param(
    [double]$interval_minutes = 10,
    [double]$duration_hours   = 4,
    [string]$mosquitto_conf   = "",
    [string]$log_dir          = ""
)

# ---------------------------------------------------------------------------
# Encoding & path setup
# ---------------------------------------------------------------------------
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

# Resolve project root (two levels up from this script)
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)

if (-not $mosquitto_conf) {
    $mosquitto_conf = Join-Path $projectRoot "bridge_server\mosquitto.conf"
}
if (-not $log_dir) {
    $log_dir = Join-Path $scriptDir "logs"
}

# Ensure log directory exists
if (-not (Test-Path $log_dir)) {
    New-Item -ItemType Directory -Path $log_dir -Force | Out-Null
}

# ---------------------------------------------------------------------------
# Log file setup
# ---------------------------------------------------------------------------
$timestamp   = Get-Date -Format "yyyyMMdd_HHmmss"
$csvFile     = Join-Path $log_dir "broker_restart_${timestamp}.csv"

# Write CSV header
"EventIndex,Timestamp,Action,Result,Details" | Out-File -FilePath $csvFile -Encoding UTF8

# ---------------------------------------------------------------------------
# Helper: find mosquitto executable
# ---------------------------------------------------------------------------
function Find-Mosquitto {
    # Check common installation paths
    $candidates = @(
        "mosquitto",
        "C:\Program Files\mosquitto\mosquitto.exe",
        "C:\Program Files (x86)\mosquitto\mosquitto.exe",
        "$env:LOCALAPPDATA\Programs\mosquitto\mosquitto.exe"
    )
    foreach ($path in $candidates) {
        $resolved = Get-Command $path -ErrorAction SilentlyContinue
        if ($resolved) {
            return $resolved.Source
        }
        if (Test-Path $path) {
            return $path
        }
    }
    return $null
}

# ---------------------------------------------------------------------------
# Helper: stop mosquitto
# ---------------------------------------------------------------------------
function Stop-Mosquitto {
    $procs = Get-Process -Name "mosquitto" -ErrorAction SilentlyContinue
    if ($procs) {
        $procs | ForEach-Object {
            try {
                Stop-Process -Id $_.Id -Force -ErrorAction Stop
            } catch {
                # Process may have already exited
            }
        }
        Start-Sleep -Seconds 1
        return $true
    }
    return $false
}

# ---------------------------------------------------------------------------
# Helper: start mosquitto
# ---------------------------------------------------------------------------
function Start-Mosquitto {
    param([string]$exePath, [string]$confPath)

    if (-not (Test-Path $confPath)) {
        return @{ Success = $false; Error = "Config file not found: $confPath" }
    }

    try {
        $proc = Start-Process -FilePath $exePath `
            -ArgumentList "-c", "`"$confPath`"" `
            -PassThru `
            -WindowStyle Hidden `
            -ErrorAction Stop
        Start-Sleep -Seconds 2

        if (-not $proc.HasExited) {
            return @{ Success = $true; PID = $proc.Id }
        } else {
            return @{ Success = $false; Error = "Process exited immediately with code $($proc.ExitCode)" }
        }
    } catch {
        return @{ Success = $false; Error = $_.Exception.Message }
    }
}

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
$mosquittoExe = Find-Mosquitto
if (-not $mosquittoExe) {
    Write-Host "[ERROR] mosquitto executable not found. Please install Mosquitto or update PATH." -ForegroundColor Red
    exit 1
}

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " Mosquitto Broker Restart Loop" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "Mosquitto path : $mosquittoExe"
Write-Host "Config file    : $mosquitto_conf"
Write-Host "Interval       : $interval_minutes minutes"
Write-Host "Duration       : $duration_hours hours"
Write-Host "Log file       : $csvFile"
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

$endTime    = (Get-Date).AddHours($duration_hours)
$eventIndex = 0
$totalRestarts    = 0
$totalFailures    = 0
$stopWaitSeconds  = 5

while ((Get-Date) -lt $endTime) {
    $eventIndex++
    $now = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

    Write-Host "[$now] Event #$eventIndex - Stopping Mosquitto..." -ForegroundColor Yellow

    # --- Stop ---
    $wasRunning = Stop-Mosquitto
    $stopResult = if ($wasRunning) { "stopped" } else { "not_running" }
    "$eventIndex,$now,stop,$stopResult," | Out-File -FilePath $csvFile -Encoding UTF8 -Append

    # Wait before restart
    Write-Host "[$now] Waiting $stopWaitSeconds seconds before restart..." -ForegroundColor Gray
    Start-Sleep -Seconds $stopWaitSeconds

    # --- Start ---
    $restartNow = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[$restartNow] Event #$eventIndex - Starting Mosquitto..." -ForegroundColor Green

    $result = Start-Mosquitto -exePath $mosquittoExe -confPath $mosquitto_conf

    if ($result.Success) {
        $totalRestarts++
        Write-Host "[$restartNow] Mosquitto started (PID: $($result.PID))" -ForegroundColor Green
        "$eventIndex,$restartNow,start,success,PID=$($result.PID)" | Out-File -FilePath $csvFile -Encoding UTF8 -Append
    } else {
        $totalFailures++
        Write-Host "[$restartNow] FAILED to start Mosquitto: $($result.Error)" -ForegroundColor Red
        "$eventIndex,$restartNow,start,failure,$($result.Error)" | Out-File -FilePath $csvFile -Encoding UTF8 -Append
    }

    # --- Wait for next interval ---
    $nextWake = (Get-Date).AddMinutes($interval_minutes)
    if ($nextWake -ge $endTime) {
        Write-Host ""
        Write-Host "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] Duration reached. Stopping loop." -ForegroundColor Cyan
        break
    }

    $remaining = ($nextWake - (Get-Date)).TotalSeconds
    if ($remaining -gt 0) {
        Write-Host "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] Next restart in $interval_minutes minutes. Sleeping..." -ForegroundColor Gray
        Start-Sleep -Seconds ([math]::Floor($remaining))
    }
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " Broker Restart Loop - SUMMARY" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "Total restart events : $eventIndex"
Write-Host "Successful restarts  : $totalRestarts"
Write-Host "Failed restarts      : $totalFailures"
Write-Host "Log file             : $csvFile"
Write-Host "==============================================" -ForegroundColor Cyan

# Append summary to CSV
"" | Out-File -FilePath $csvFile -Encoding UTF8 -Append
"# SUMMARY" | Out-File -FilePath $csvFile -Encoding UTF8 -Append
"# TotalEvents,$eventIndex" | Out-File -FilePath $csvFile -Encoding UTF8 -Append
"# SuccessfulRestarts,$totalRestarts" | Out-File -FilePath $csvFile -Encoding UTF8 -Append
"# FailedRestarts,$totalFailures" | Out-File -FilePath $csvFile -Encoding UTF8 -Append
