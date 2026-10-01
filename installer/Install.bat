@echo off
rem Instant Karma setup: opens the installer window (uses Windows PowerShell, which is part of Windows).
if not exist "%~dp0InstantKarma\InstantKarma-Setup.ps1" (
  echo Please extract the whole zip to a folder first, then run Install.bat from there.
  pause
  exit /b 1
)
start "" "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -STA -WindowStyle Hidden -File "%~dp0InstantKarma\InstantKarma-Setup.ps1"
