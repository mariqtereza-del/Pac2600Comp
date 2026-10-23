@echo off
rem Start the reference Stella 7.0 app (D-002) with all settings kept in its own folder.
rem -baseinappdir: Stella stores its config next to Stella.exe instead of %APPDATA% (DECISIONS.md T-C1-2).
rem Extra arguments (for example a ROM path in later stages) are passed on to Stella.
setlocal
set "STELLA_DIR=F:\Ports\Tools\Stella-7.0c"
if not exist "%STELLA_DIR%\Stella.exe" (echo ERROR: "%STELLA_DIR%\Stella.exe" not found & exit /b 1)
start "" "%STELLA_DIR%\Stella.exe" -baseinappdir %*
