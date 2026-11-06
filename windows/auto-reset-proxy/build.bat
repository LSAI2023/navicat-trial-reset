@echo off
rem Build auto-reset winmm.dll (x64 PE).
rem Toolchain: any MinGW-w64 x86_64 gcc.
rem   1) if gcc is already on PATH it is used as-is;
rem   2) otherwise MINGW_BIN below is prepended to PATH - edit it to your
rem      local MinGW bin directory.
set "MINGW_BIN=C:\mingw64\bin"
where gcc >nul 2>&1 || set "PATH=%MINGW_BIN%;%PATH%"
cd /d %~dp0
gcc -O2 -shared -o winmm.dll proxy.c winmm.def -Wl,--subsystem,windows -lkernel32 -luser32 -ladvapi32
if errorlevel 1 (
  echo BUILD FAILED
  pause
  exit /b 1
)
echo BUILD OK
powershell -NoProfile -Command "(Get-FileHash -Algorithm SHA256 '%~dp0winmm.dll').Hash"
pause
