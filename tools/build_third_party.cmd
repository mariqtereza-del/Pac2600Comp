@echo off
rem Build the third-party libraries on their own (stage B7 check) into build\third_party.
rem Uses the x64 Developer environment (DECISIONS.md T-B2-2).
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Workload.VCTools -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (echo ERROR: Build Tools not found by vswhere & exit /b 1)
call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 -no_logo 2>nul
cd /d "%~dp0.."
cmake -S third_party -B build\third_party -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 2
cmake --build build\third_party --target SDL3-shared stella_libretro || exit /b 3
echo BUILD OK
