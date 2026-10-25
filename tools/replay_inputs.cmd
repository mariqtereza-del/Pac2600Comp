@echo off
rem Play back a recording made with tools\record_inputs.cmd (drop the .inputs file on this script, or give its path).
rem Your keys are ignored while the recording runs; afterwards the keyboard works again.
setlocal
if "%~1"=="" (echo Drop a .inputs file on this script, or give its path. & pause & exit /b 1)
start "" "%~dp0..\build\host\pacman2600.exe" --replay "%~1"
