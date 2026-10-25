@echo off
setlocal
set "POWER_TEST_PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist "%POWER_TEST_PY%" (
  echo Python di PlatformIO non trovato: %POWER_TEST_PY%
  pause
  exit /b 1
)
"%POWER_TEST_PY%" "%~dp0tools\prova.py"
pause
