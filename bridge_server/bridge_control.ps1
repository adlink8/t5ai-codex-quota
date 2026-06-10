[CmdletBinding()]
param(
    [ValidateSet("start", "stop", "status")]
    [string]$Action = "start",
    [int]$BridgePort = 5678,
    [int]$MqttPort = 1883,
    [int]$RefreshSeconds = 5,
    [int]$RecentCount = 7
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RuntimeDir = Join-Path $ScriptDir "runtime"
$BridgePidFile = Join-Path $RuntimeDir "bridge.pid"
$MosquittoPidFile = Join-Path $RuntimeDir "mosquitto.pid"
$BridgeOutLog = Join-Path $RuntimeDir "bridge_stdout.log"
$BridgeErrLog = Join-Path $RuntimeDir "bridge_stderr.log"
$ControlLog = Join-Path $RuntimeDir "bridge_control.log"
$MosquittoLog = Join-Path $ScriptDir "mosquitto.log"
$BridgeScript = Join-Path $ScriptDir "codex_bridge_server.py"
$MosquittoConf = Join-Path $ScriptDir "mosquitto.conf"
$PythonExe = Join-Path $ScriptDir ".venv\Scripts\python.exe"
$PipExe = Join-Path $ScriptDir ".venv\Scripts\pip.exe"
$MosquittoExe = "C:\Program Files\Mosquitto\mosquitto.exe"

function Ensure-Runtime {
    if (-not (Test-Path $RuntimeDir)) {
        New-Item -ItemType Directory -Path $RuntimeDir -Force | Out-Null
    }
}

function Write-ControlLog {
    param([string]$Message)
    Ensure-Runtime
    $line = "[{0:yyyy-MM-dd HH:mm:ss}] {1}" -f (Get-Date), $Message
    Add-Content -Path $ControlLog -Value $line -Encoding UTF8
}

function Get-CommandLineProcess {
    param([string]$NamePattern, [string]$CommandPattern)
    Get-CimInstance Win32_Process |
        Where-Object {
            $_.Name -match $NamePattern -and
            $_.CommandLine -and
            $_.CommandLine -like "*$CommandPattern*"
        }
}

function Stop-PidFileProcess {
    param([string]$PidFile, [string]$Label)
    if (-not (Test-Path $PidFile)) { return }

    $pidText = (Get-Content -Path $PidFile -ErrorAction SilentlyContinue | Select-Object -First 1).Trim()
    if ($pidText -match '^\d+$') {
        $proc = Get-Process -Id ([int]$pidText) -ErrorAction SilentlyContinue
        if ($proc) {
            Write-Host "Stopping $Label PID $pidText ..."
            Write-ControlLog "Stopping $Label PID $pidText from pid file"
            Stop-Process -Id ([int]$pidText) -Force -ErrorAction SilentlyContinue
        }
    }
    Remove-Item -Path $PidFile -Force -ErrorAction SilentlyContinue
}

function Stop-BridgeServices {
    param([switch]$KillControllers)
    Ensure-Runtime

    if ($KillControllers) {
        foreach ($p in Get-CimInstance Win32_Process | Where-Object {
            $_.ProcessId -ne $PID -and
            $_.Name -match 'powershell.exe|pwsh.exe' -and
            $_.CommandLine -and
            $_.CommandLine -notlike '* -Command *' -and
            $_.CommandLine -like '*-File*bridge_control.ps1*' -and
            $_.CommandLine -like '*start*'
        }) {
            Write-Host "Stopping bridge monitor PID $($p.ProcessId) ..."
            Write-ControlLog "Stopping bridge monitor PID $($p.ProcessId)"
            Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
        }
    }

    Stop-PidFileProcess -PidFile $BridgePidFile -Label "bridge"
    Stop-PidFileProcess -PidFile $MosquittoPidFile -Label "mosquitto"

    foreach ($p in Get-CommandLineProcess -NamePattern "python.exe" -CommandPattern "codex_bridge_server.py") {
        Write-Host "Stopping bridge orphan PID $($p.ProcessId) ..."
        Write-ControlLog "Stopping bridge orphan PID $($p.ProcessId)"
        Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
    }

    foreach ($p in Get-CommandLineProcess -NamePattern "mosquitto.exe" -CommandPattern "mosquitto.conf") {
        Write-Host "Stopping mosquitto orphan PID $($p.ProcessId) ..."
        Write-ControlLog "Stopping mosquitto orphan PID $($p.ProcessId)"
        Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
    }

    Remove-Item -Path $BridgePidFile, $MosquittoPidFile -Force -ErrorAction SilentlyContinue
}

function Reconcile-BridgeProcesses {
    param([int]$InitialPid = 0)
    $bridgeProcesses = @(Get-CommandLineProcess -NamePattern "python.exe" -CommandPattern "codex_bridge_server.py")
    if ($bridgeProcesses.Count -eq 0) {
        return
    }

    if ($bridgeProcesses.Count -gt 1) {
        $keepPid = ($bridgeProcesses |
            Where-Object { $_.ProcessId -ne $InitialPid } |
            Sort-Object ProcessId |
            Select-Object -First 1).ProcessId
    } else {
        $keepPid = ($bridgeProcesses | Select-Object -First 1).ProcessId
    }
    Set-Content -Path $BridgePidFile -Value $keepPid -Encoding ASCII
    Write-ControlLog "Reconciled bridge PID to $keepPid (initial=$InitialPid)"

    foreach ($p in $bridgeProcesses) {
        if ($p.ProcessId -ne $keepPid) {
            Write-Host "Stopping duplicate bridge PID $($p.ProcessId) ..."
            Write-ControlLog "Stopping duplicate bridge PID $($p.ProcessId); keeping $keepPid"
            Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
        }
    }
}

function Ensure-PythonEnv {
    if (-not (Test-Path $PythonExe)) {
        Write-Host "Creating Python virtual environment ..."
        python -m venv (Join-Path $ScriptDir ".venv")
    }

    if (Test-Path (Join-Path $ScriptDir "requirements.txt")) {
        & $PipExe install -r (Join-Path $ScriptDir "requirements.txt") -q
    }
    & $PipExe show paho-mqtt *> $null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Installing paho-mqtt ..."
        & $PipExe install paho-mqtt -q
    }
}

function Get-LocalIp {
    try {
        $udp = [System.Net.Sockets.UdpClient]::new()
        $udp.Connect("10.255.255.255", 1)
        $ip = $udp.Client.LocalEndPoint.Address.ToString()
        $udp.Dispose()
        return $ip
    } catch {
        return "127.0.0.1"
    }
}

function Start-BridgeServices {
    Ensure-Runtime
    Stop-BridgeServices -KillControllers
    Ensure-PythonEnv

    if (Test-Path $MosquittoExe) {
        Write-Host "Starting Mosquitto MQTT broker ..."
        $mosq = Start-Process -FilePath $MosquittoExe `
            -ArgumentList @("-c", $MosquittoConf) `
            -WorkingDirectory $ScriptDir `
            -WindowStyle Hidden `
            -PassThru
        Set-Content -Path $MosquittoPidFile -Value $mosq.Id -Encoding ASCII
        Write-ControlLog "Started mosquitto PID $($mosq.Id)"
    } else {
        Write-Host "[WARN] Mosquitto not found: $MosquittoExe"
    }

    Write-Host "Starting Codex bridge ..."
    $bridge = Start-Process -FilePath $PythonExe `
        -ArgumentList @($BridgeScript, "--port", "$BridgePort", "--mqtt-port", "$MqttPort") `
        -WorkingDirectory $ScriptDir `
        -WindowStyle Hidden `
        -PassThru
    Set-Content -Path $BridgePidFile -Value $bridge.Id -Encoding ASCII
    Write-ControlLog "Started bridge PID $($bridge.Id)"
    Start-Sleep -Seconds 2
    Reconcile-BridgeProcesses -InitialPid $bridge.Id
}

function Get-ServiceSnapshot {
    $bridgePid = if (Test-Path $BridgePidFile) { (Get-Content $BridgePidFile | Select-Object -First 1).Trim() } else { "" }
    $mqttPid = if (Test-Path $MosquittoPidFile) { (Get-Content $MosquittoPidFile | Select-Object -First 1).Trim() } else { "" }
    [pscustomobject]@{
        BridgePid = $bridgePid
        BridgeRunning = ($bridgePid -match '^\d+$' -and (Get-Process -Id ([int]$bridgePid) -ErrorAction SilentlyContinue) -ne $null)
        MqttPid = $mqttPid
        MqttRunning = ($mqttPid -match '^\d+$' -and (Get-Process -Id ([int]$mqttPid) -ErrorAction SilentlyContinue) -ne $null)
    }
}

function Get-QuotaLine {
    param([string]$LocalIp)
    try {
        $quota = Invoke-RestMethod -Uri "http://127.0.0.1:$BridgePort/quota" -TimeoutSec 3
        $primary = $quota.primary
        $secondary = $quota.secondary
        $secondaryText = if ($secondary) { " | S:$($secondary.remaining_percent)%" } else { "" }
        return "{0:HH:mm:ss} plan={1} P:{2}% used:{3}% left:{4}{5}" -f (
            Get-Date),
            $quota.plan_type,
            $primary.remaining_percent,
            $primary.used_percent,
            $primary.resets_in,
            $secondaryText
    } catch {
        return "{0:HH:mm:ss} quota unavailable: {1}" -f (Get-Date), $_.Exception.Message
    }
}

function Show-Monitor {
    $localIp = Get-LocalIp
    $recent = New-Object System.Collections.Generic.Queue[string]

    while ($true) {
        $line = Get-QuotaLine -LocalIp $localIp
        $recent.Enqueue($line)
        while ($recent.Count -gt $RecentCount) { [void]$recent.Dequeue() }

        $svc = Get-ServiceSnapshot
        Clear-Host
        Write-Host "Codex Bridge Monitor"
        Write-Host "HTTP: http://$localIp`:$BridgePort/quota"
        Write-Host "MQTT: $localIp`:$MqttPort topic=codex/quota"
        Write-Host "Bridge: $($svc.BridgeRunning) PID=$($svc.BridgePid) | Mosquitto: $($svc.MqttRunning) PID=$($svc.MqttPid)"
        Write-Host ""
        Write-Host "Recent quota snapshots (latest $RecentCount):"
        foreach ($item in $recent) { Write-Host "  $item" }
        Write-Host ""
        Write-Host "[Q] Stop bridge + MQTT and exit   [X] Exit monitor only   [Enter] Refresh"

        $deadline = (Get-Date).AddSeconds($RefreshSeconds)
        while ((Get-Date) -lt $deadline) {
            if ([Console]::KeyAvailable) {
                $key = [Console]::ReadKey($true)
                if ($key.Key -eq "Q") {
                    Stop-BridgeServices
                    return
                }
                if ($key.Key -eq "X") {
                    return
                }
                if ($key.Key -eq "Enter") {
                    break
                }
            }
            Start-Sleep -Milliseconds 100
        }
    }
}

switch ($Action) {
    "start" {
        Start-BridgeServices
        Start-Sleep -Seconds 2
        Show-Monitor
    }
    "stop" {
        Stop-BridgeServices -KillControllers
        Write-Host "Stopped Codex bridge services."
    }
    "status" {
        Get-ServiceSnapshot | Format-List
    }
}
