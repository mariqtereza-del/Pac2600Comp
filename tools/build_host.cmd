@echo off
rem Build the host program (src\host) into build\host.
rem Uses the x64 Developer environment (DECISIONS.md T-B2-2).
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Workload.VCTools -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (echo ERROR: Build Tools not found by vswhere & exit /b 1)
call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 -no_logo 2>nul
cd /d "%~dp0.."
cmake -S src\host -B build\host -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 2
cmake --build build\host --target pacman2600 pacman2600_ref test_rom_finder test_layout test_windowstate test_controls test_inputlog test_trace test_menu test_background test_framerate test_hdr test_options test_crt test_volume test_feedback test_d3d_pin test_achievements test_stats || exit /b 3
echo BUILD OK
