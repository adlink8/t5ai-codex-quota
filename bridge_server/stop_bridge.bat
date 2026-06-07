@echo off
chcp 65001 >nul 2>&1
echo Stopping Codex Bridge services...

:: Stop bridge server (python process)
taskkill /FI "WINDOWTITLE eq Codex Bridge Server" /F >nul 2>&1

:: Stop Mosquitto
tasklist /FI "IMAGENAME eq mosquitto.exe" 2>nul | find /I "mosquitto.exe" >nul
if %ERRORLEVEL%==0 (
    taskkill /IM mosquitto.exe /F >nul 2>&1
    echo Mosquitto stopped.
) else (
    echo Mosquitto not running.
)

echo Done.
timeout /t 2 /nobreak >nul
