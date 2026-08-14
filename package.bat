@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package.ps1"
if errorlevel 1 (
    echo.
    echo Package failed.
    pause
    exit /b %ERRORLEVEL%
)
echo.
pause
