@echo off
chcp 65001 >nul 2>&1
cd /d "%~dp0"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0bridge_control.ps1" -Action stop

timeout /t 2 /nobreak >nul
