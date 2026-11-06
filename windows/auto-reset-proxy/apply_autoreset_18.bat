@echo off
rem Deploy auto-reset winmm.dll (triggers one UAC prompt)
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell -Verb RunAs -ArgumentList '-ExecutionPolicy','Bypass','-File','%~dp0deploy_autoreset.ps1'"
echo UAC triggered - click Yes, then check deploy_log.txt in this folder.
pause
