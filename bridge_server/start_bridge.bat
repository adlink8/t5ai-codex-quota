@echo off
chcp 65001 >nul 2>&1
title Codex Bridge Monitor
cd /d "%~dp0"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0bridge_control.ps1" -Action start

echo.
echo Bridge monitor exited.
pause
