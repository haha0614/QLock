@echo off
setlocal
set ROOT=%~dp0
call "%ROOT%build.cmd"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\pack.ps1"
if errorlevel 1 (echo PACK FAILED & exit /b 1)
