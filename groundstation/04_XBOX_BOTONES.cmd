@echo off
setlocal
cd /d "%~dp0"
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" -B -X utf8 -u "%~dp0xbox_control.py" --monitor %*
pause
