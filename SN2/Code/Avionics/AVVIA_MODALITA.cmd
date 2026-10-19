@echo off
setlocal
cd /d "%~dp0"
set "AVIONICS_PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist "%AVIONICS_PY%" (
  echo Python di PlatformIO non trovato. Aprire VSCode e controllare PlatformIO.
  pause
  exit /b 1
)
"%AVIONICS_PY%" tools\avionics.py
pause
