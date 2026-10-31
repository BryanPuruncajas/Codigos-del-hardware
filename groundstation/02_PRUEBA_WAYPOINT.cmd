@echo off
setlocal
set "BLIMP_TEST_PYTHON=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist "%BLIMP_TEST_PYTHON%" (
  echo No se encontro Python de PlatformIO: %BLIMP_TEST_PYTHON%
  pause
  exit /b 1
)
cd /d "%~dp0"
echo PRUEBA CON MOTORES. Ground debe estar abierto en otra ventana.
echo Duracion: 60 segundos despues de la cuenta por voz.
"%BLIMP_TEST_PYTHON%" -B -X utf8 -u "%~dp0check_single_waypoint.py" --actuate --seconds 60
pause
