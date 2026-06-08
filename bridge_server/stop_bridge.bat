@echo off
chcp 65001 >nul 2>&1
echo Stopping Codex Bridge services...

cd /d "%~dp0"

:: --- Stop bridge server by PID ---
if exist "runtime\bridge.pid" (
    set /p BRIDGE_PID=<"runtime\bridge.pid"
    echo Stopping bridge server (PID: %BRIDGE_PID%)...
    taskkill /PID %BRIDGE_PID% /F >nul 2>&1
    if %ERRORLEVEL%==0 (
        echo Bridge server stopped.
    ) else (
        echo Bridge process not found (already stopped).
    )
    del "runtime\bridge.pid" >nul 2>&1
) else (
    echo PID file not found for bridge server.
    echo Falling back to process name...
    tasklist /FI "IMAGENAME eq python.exe" 2>nul | find /I "python.exe" >nul
    if %ERRORLEVEL%==0 (
        echo [WARN] Found python.exe processes. Killing codex_bridge_server...
        for /f "tokens=2" %%a in ('wmic process where "name='python.exe' and commandline like '%%codex_bridge_server%%'" get processid /value 2^>nul ^| findstr "="') do (
            taskkill /PID %%a /F >nul 2>&1
            echo   Killed PID: %%a
        )
    ) else (
        echo No Python processes found.
    )
)

:: --- Stop Mosquitto by PID ---
if exist "runtime\mosquitto.pid" (
    set /p MOSQUITTO_PID=<"runtime\mosquitto.pid"
    echo Stopping Mosquitto (PID: %MOSQUITTO_PID%)...
    taskkill /PID %MOSQUITTO_PID% /F >nul 2>&1
    if %ERRORLEVEL%==0 (
        echo Mosquitto stopped.
    ) else (
        echo Mosquitto process not found (already stopped).
    )
    del "runtime\mosquitto.pid" >nul 2>&1
) else (
    echo PID file not found for Mosquitto.
    echo Falling back to process name...
    tasklist /FI "IMAGENAME eq mosquitto.exe" 2>nul | find /I "mosquitto.exe" >nul
    if %ERRORLEVEL%==0 (
        taskkill /IM mosquitto.exe /F >nul 2>&1
        echo Mosquitto stopped.
    ) else (
        echo Mosquitto not running.
    )
)

:: --- Clean up runtime directory ---
if exist "runtime\bridge.pid" del "runtime\bridge.pid" >nul 2>&1
if exist "runtime\mosquitto.pid" del "runtime\mosquitto.pid" >nul 2>&1

echo Done.
timeout /t 2 /nobreak >nul
