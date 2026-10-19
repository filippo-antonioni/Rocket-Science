@echo off
setlocal
cd /d "%~dp0"
set "ROCKET_PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist "%ROCKET_PY%" (
  echo Python di PlatformIO non trovato. Aprire VSCode e installare PlatformIO.
  pause
  exit /b 1
)
"%ROCKET_PY%" tools\banco.py
pause
