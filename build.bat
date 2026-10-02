@echo off
rem Builds RetroAmp (Release) into build\Release\RetroAmp.exe
setlocal
cd /d "%~dp0"
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 || goto :error
cmake --build build --config Release -- /m /v:minimal || goto :error
echo.
echo Done: build\Release\RetroAmp.exe
exit /b 0
:error
echo Build failed.
exit /b 1
