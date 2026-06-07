@echo off
chcp 65001 >nul 2>&1
title Codex Bridge Server

echo ============================================
echo   Codex Bridge Server - One Click Start
echo ============================================
echo.

cd /d "%~dp0"

:: --- 1. Start Mosquitto MQTT Broker ---
echo [1/3] Starting Mosquitto MQTT Broker...
tasklist /FI "IMAGENAME eq mosquitto.exe" 2>nul | find /I "mosquitto.exe" >nul
if %ERRORLEVEL%==0 (
    echo       Mosquitto already running, skip.
) else (
    if exist "C:\Program Files\Mosquitto\mosquitto.exe" (
        start "" /B "C:\Program Files\Mosquitto\mosquitto.exe" -c "%~dp0mosquitto.conf" -d
        timeout /t 1 /nobreak >nul
        echo       Mosquitto started on port 1883.
    ) else (
        echo       [WARN] Mosquitto not found, MQTT disabled.
        echo       Install: https://mosquitto.org/download/
    )
)

:: --- 2. Check Python dependencies ---
echo [2/3] Checking Python dependencies...
pip show paho-mqtt >nul 2>&1
if %ERRORLEVEL%==0 (
    echo       paho-mqtt installed.
) else (
    echo       Installing paho-mqtt...
    pip install paho-mqtt -q
)

:: --- 3. Get local IP and start bridge ---
echo [3/3] Starting Codex Bridge Server...
echo.

python -c "import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.connect(('10.255.255.255',1)); ip=s.getsockname()[0]; s.close(); print(f'       Local IP: {ip}'); print(f'       HTTP:     http://{ip}:5678/quota'); print(f'       MQTT:     {ip}:1883  topic: codex/quota')"

echo.
echo --------------------------------------------
echo   Server running. Press Ctrl+C to stop.
echo --------------------------------------------
echo.

python "%~dp0codex_bridge_server.py" --port 5678

:: If server exits
echo.
echo Bridge server stopped.
pause
