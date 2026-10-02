@echo off
rem Builds RetroAmp (Release) and the Inno Setup installer -> installer\Output\RetroAmp_Setup_<version>.exe
setlocal
cd /d "%~dp0"
call build.bat || exit /b 1
set ISCC="%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist %ISCC% set ISCC="%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist %ISCC% (
  echo Inno Setup 6 not found - install it from https://jrsoftware.org/isdl.php
  exit /b 1
)
%ISCC% /Q installer\RetroAmp.iss || exit /b 1
echo.
echo Done: installer\Output\
