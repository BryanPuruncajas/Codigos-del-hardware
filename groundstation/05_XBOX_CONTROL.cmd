@echo off
setlocal
cd /d "%~dp0"
echo CONTROL CON MOTORES. Requiere Ground actualizado y firmware 42.
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u "%~dp0xbox_control.py" --actuate %*
pause
