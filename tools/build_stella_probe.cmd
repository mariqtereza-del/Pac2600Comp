@echo off
rem Build tools\stella_probe (Stella 7.0 core measuring tool) into build\stella_probe.
rem Uses the x64 Developer environment (DECISIONS.md T-B2-2).
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Workload.VCTools -property installationPath`) do set "VSPATH=%%i"
rem A full Visual Studio (a GitHub runner has one) has no "Build Tools" workload; the C++ compiler component is in both (Stage J7, T-J7-1).
if not defined VSPATH for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (echo ERROR: Build Tools not found by vswhere & exit /b 1)
call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 -no_logo 2>nul
cd /d "%~dp0.."
cmake -S tools\stella_probe -B build\stella_probe -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 2
cmake --build build\stella_probe --target stella_probe stella_timing stella_sound stella_script stella_bot stella_coverage stella_pctrace || exit /b 3
echo BUILD OK
