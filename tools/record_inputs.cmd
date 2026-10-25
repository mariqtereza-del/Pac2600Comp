@echo off
rem Play the game and record every frame's input (Stage D13). The recording is written when the window is closed
rem to recordings\play-<date>-<time>.inputs. To watch it again, drop that file on tools\replay_inputs.cmd.
setlocal
set "ROOT=%~dp0.."
if not exist "%ROOT%\recordings" mkdir "%ROOT%\recordings"
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%i"
start "" "%ROOT%\build\host\pacman2600.exe" --record "%ROOT%\recordings\play-%STAMP%.inputs"
