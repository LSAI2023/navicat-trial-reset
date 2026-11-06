@echo off
chcp 65001 >nul
echo === Navicat 18 试用期重置（正确流程版）===
echo.
echo [1/3] 关闭 Navicat...
taskkill /IM navicat.exe /F 2>nul
timeout /t 2 /nobreak >nul
echo [2/3] 删除试用状态键（必须在关闭状态下才有效）...
reg delete "HKCU\Software\PremiumSoft\NavicatPremium\Registration18XCS" /f
echo [3/3] 完成。下次启动 Navicat = 全新 14 天试用，无"剩余 8 天"弹窗。
echo.
echo 原理：Navicat 运行中删键会被旧状态写回（实测证实），必须先关闭再删。
echo 试用期将满时再跑一次本脚本即可。
echo.
pause
