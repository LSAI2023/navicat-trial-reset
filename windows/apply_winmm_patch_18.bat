@echo off
rem =============================================================
rem apply_winmm_patch_18.bat - 部署净化重制版 winmm.dll 到 Navicat 18
rem 产物: windows\clean-proxy\winmm.dll (本仓库自建, 源码同目录)
rem 行为: 与原 Wimmm 补丁等效(见 winmm.dll逆向报告.md), 但补丁
rem       线程加载后立即生效(毫秒级), 且带签名校验, 不匹配不写内存。
rem 还原: 删除安装目录中的 winmm.dll 即完全还原(备份在同目录 .orig.bak)。
rem 日志: 本脚本所有动作写入 deploy_log.txt, 窗口闪退也能查记录。
rem =============================================================
setlocal
cd /d "%~dp0"
set "LOG=%~dp0deploy_log.txt"
echo ====== %date% %time% 部署开始 ====== >> "%LOG%"
echo 正在以管理员权限执行, 请留意 UAC 弹窗并点"是"...

net session >nul 2>&1
if %errorlevel% neq 0 (
  echo 当前不是管理员, 正在请求提权... 本窗口即将关闭属正常现象。
  echo 已请求管理员权限，请在 UAC 弹窗点"是" >> "%LOG%"
  pause
  powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs -Wait"
  echo ====== %date% %time% 提权实例已结束 ====== >> "%LOG%"
  exit /b
)

set "SRC=%~dp0clean-proxy\winmm.dll"
set "DST=C:\Program Files\Navicat\Navicat Premium 18\winmm.dll"
set "DSTV=C:\Program Files\Navicat\Navicat Premium 18\version.dll"

echo [1/5] 记录日志并退出 Navicat...
echo [1/5] 退出 Navicat >> "%LOG%"
taskkill /IM Navicat.exe /F >nul 2>&1
timeout /t 2 /nobreak >nul

echo [2/5] 校验源文件...
echo [2/5] SRC=%SRC% >> "%LOG%"
if not exist "%SRC%" (
  echo    [错误] 找不到源文件: %SRC%
  echo [错误] 源文件不存在 >> "%LOG%"
  pause
  exit /b 2
)
echo    源文件存在, 大小: %~zSRC 字节

echo [3/5] 清理可能残留的 version.dll 补丁...
if exist "%DSTV%" (
  del /f /q "%DSTV%" >nul
  echo    已移除 version.dll >> "%LOG%"
  echo    已移除 version.dll
) else (
  echo    无 version.dll 残留
)

echo [4/5] 备份现有 winmm.dll 并部署净化版...
if exist "%DST%" (
  copy /y "%DST%" "%DST%.orig.bak" >nul
  echo    已备份: winmm.dll.orig.bak >> "%LOG%"
  echo    已备份原文件为 winmm.dll.orig.bak
)
copy /y "%SRC%" "%DST%" >nul
if exist "%DST%" (
  for %%A in ("%DST%") do echo    已部署: %%~zA 字节
  echo [4/5] 部署成功 >> "%LOG%"
) else (
  echo    [错误] 复制失败, 请检查 360 等安全软件是否拦截
  echo [错误] 复制失败 >> "%LOG%"
  pause
  exit /b 2
)

echo [5/5] 启动 Navicat...
echo [5/5] 启动 Navicat >> "%LOG%"
start "" "C:\Program Files\Navicat\Navicat Premium 18\navicat.exe"

echo.
echo ============================================================
echo 完成! 部署日志: %LOG%
echo 还原方式: 删除安装目录里的 winmm.dll 即回到系统原版;
echo           或把 winmm.dll.orig.bak 改回 winmm.dll。
echo 本窗口保持打开, 按任意键关闭...
echo ============================================================
pause
echo ====== %date% %time% 部署结束 ====== >> "%LOG%"
