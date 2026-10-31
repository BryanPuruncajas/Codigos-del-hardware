@echo off
setlocal
cd /d "%~dp0"
echo SOLO LECTURA. No arma, no mueve actuadores ni cambia modos.
echo Requiere ESP28, Ground reiniciado y blimp desarmado en modo0.
echo Bloquee el frame completamente inmovil antes de continuar.
pause
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u "%~dp0check_vertical_baseline.py" --seconds 30 --stationary
pause
