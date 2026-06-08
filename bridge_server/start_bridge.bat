@echo off
chcp 65001 >nul 2>&1
title Codex Bridge Server

echo ============================================
echo   Codex Bridge Server - One Click Start
echo ============================================
echo.

cd /d "%~dp0"

:: --- Ensure runtime directory exists ---
if not exist "runtime" mkdir runtime

:: --- 1. Start Mosquitto MQTT Broker ---
echo [1/4] Starting Mosquitto MQTT Broker...
tasklist /FI "IMAGENAME eq mosquitto.exe" 2>nul | find /I "mosquitto.exe" >nul
if %ERRORLEVEL%==0 (
    echo       Mosquitto already running, skip.
) else (
    if exist "C:\Program Files\Mosquitto\mosquitto.exe" (
        start "" /B "C:\Program Files\Mosquitto\mosquitto.exe" -c "%~dp0mosquitto.conf" -d
        timeout /t 1 /nobreak >nul
        echo       Mosquitto started on port 1883.
        :: Write Mosquitto PID
        for /f "tokens=2" %%a in ('tasklist /FI "IMAGENAME eq mosquitto.exe" /FO LIST ^| findstr /i "PID:"') do (
            echo %%a> "runtime\mosquitto.pid"
            echo       Mosquitto PID: %%a
        )
    ) else (
        echo       [WARN] Mosquitto not found, MQTT disabled.
        echo       Install: https://mosquitto.org/download/
    )
)

:: --- 2. Setup Python virtual environment ---
echo [2/4] Setting up Python environment...
if exist ".venv\Scripts\python.exe" (
    echo       Virtual environment found.
) else (
    echo       Creating virtual environment...
    python -m venv .venv
    if %ERRORLEVEL% neq 0 (
        echo       [ERROR] Failed to create virtual environment.
        echo       Make sure Python is installed and on PATH.
        pause
        exit /b 1
    )
    echo       Installing requirements...
    .venv\Scripts\pip.exe install -r requirements.txt -q
    if %ERRORLEVEL% neq 0 (
        echo       [WARN] pip install had issues, continuing anyway...
    )
    echo       Virtual environment ready.
)

:: --- 3. Check Python dependencies in venv ---
echo [3/4] Checking dependencies...
.venv\Scripts\pip.exe show paho-mqtt >nul 2>&1
if %ERRORLEVEL%==0 (
    echo       paho-mqtt installed.
) else (
    echo       Installing paho-mqtt...
    .venv\Scripts\pip.exe install paho-mqtt -q
)

:: --- 4. Get local IP and start bridge ---
echo [4/4] Starting Codex Bridge Server...
echo.

.venv\Scripts\python.exe -c "import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.connect(('10.255.255.255',1)); ip=s.getsockname()[0]; s.close(); print(f'       Local IP: {ip}'); print(f'       HTTP:     http://{ip}:5678/quota'); print(f'       MQTT:     {ip}:1883  topic: codex/quota')"

echo.
echo --------------------------------------------
echo   Server running. Press Ctrl+C to stop.
echo --------------------------------------------
echo.

:: Start bridge server and capture PID
.venv\Scripts\python.exe "%~dp0codex_bridge_server.py" --port 5678 &
set BRIDGE_PID=%ERRORLEVEL%
:: Write bridge PID (the python process we just started)
for /f "tokens=2" %%a in ('tasklist /FI "IMAGENAME eq python.exe" /FO LIST ^| findstr /i "PID:"') do (
    echo %%a> "runtime\bridge.pid"
    echo       Bridge PID: %%a
)

:: Wait for the background process
wait

:: If server exits
echo.
echo Bridge server stopped.
pause
