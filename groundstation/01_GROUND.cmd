@echo off
setlocal
set "BLIMP_TEST_PYTHON=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist "%BLIMP_TEST_PYTHON%" (
  echo No se encontro Python de PlatformIO: %BLIMP_TEST_PYTHON%
  pause
  exit /b 1
)
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -NonInteractive -Command "if (Get-NetTCPConnection -LocalPort 8765 -State Listen -ErrorAction SilentlyContinue) { exit 0 }; exit 1"
if not errorlevel 1 (
  echo Ya hay un puente escuchando en8765. Utiliza Ground existente.
  pause
  exit /b 0
)
echo Ground en COM9. Mantenga esta ventana abierta durante la prueba.
"%BLIMP_TEST_PYTHON%" -B -X utf8 -u "%~dp0telemetry_bridge.py" COM9
pause
