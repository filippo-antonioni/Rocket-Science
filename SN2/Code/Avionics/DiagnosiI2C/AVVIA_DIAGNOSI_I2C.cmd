@echo off
setlocal
set "I2C_TEST_PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist "%I2C_TEST_PY%" (
  echo Python di PlatformIO non trovato.
  pause
  exit /b 1
)
"%I2C_TEST_PY%" "%~dp0prova.py" --port COM6
pause
