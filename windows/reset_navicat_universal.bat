@echo off
rem =============================================================
rem reset_navicat_universal.bat - Navicat 试用期重置 (Windows)
rem 规律: A. CLSID 下含 Info / ShellFolder(指向 Navicat) 的子键
rem          = 试用状态区, 删后 Navicat 自动重建并重新计时
rem       B. Registration* 子键清值(PreTrial 试用记录)
rem       C. 32-hex 子键(v12~15 兼容)  D. AppData .32hex 32字节 token
rem       E. Update 子键(仅更新缓存, 自动重建)
rem 连接/界面等明文设置一律不动. 仅依赖系统自带 PowerShell(Win10/11).
rem PS 书写约定: 每行一个引号片段, 行尾 ^ 续行, 语句以 ; 收尾
rem =============================================================
chcp 65001 >nul
title Navicat 试用期重置

echo === Navicat 试用期重置 ===

echo [1/4] 退出 Navicat...
taskkill /IM Navicat.exe /F >nul 2>&1
taskkill /IM NavicatPremium.exe /F >nul 2>&1
timeout /t 2 /nobreak >nul

rem ---- 规律A: 删 CLSID 试用状态区 ----
echo [2/4] 清除 CLSID 加密状态键...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$n = 0;" ^
  "$roots = 'HKCU:\Software\Classes\CLSID','HKCU:\Software\Classes\WOW6432Node\CLSID';" ^
  "foreach ($root in $roots) {" ^
  "  if (-not (Test-Path $root)) { continue };" ^
  "  foreach ($k in Get-ChildItem $root -ErrorAction SilentlyContinue) {" ^
  "    $p = Get-ItemProperty -LiteralPath $k.PSPath -ErrorAction SilentlyContinue;" ^
  "    $isNav = @($p.PSObject.Properties.Name) -contains 'Info' -or" ^
  "            [string]$p.ShellFolder -match 'Navicat|PremiumSoft';" ^
  "    if ($p -and $isNav) {" ^
  "      Remove-Item -LiteralPath $k.PSPath -Recurse -Force;" ^
  "      $n++;" ^
  "      Write-Host ('    已删: ' + $k.PSChildName)" ^
  "    }" ^
  "  }" ^
  "};" ^
  "if ($n) { Write-Host ('    共清除 ' + $n + ' 个') } else { Write-Host '    无状态键,跳过' }"

rem ---- 规律B/C/E: Update 整删 + Registration 清值 + 32-hex 整删 ----
echo [3/4] 清除 NavicatPremium 试用记录...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$b = 'HKCU:\Software\PremiumSoft\NavicatPremium';" ^
  "if (Test-Path $b) {" ^
  "  if (Test-Path ($b + '\Update')) {" ^
  "    Remove-Item ($b + '\Update') -Recurse -Force;" ^
  "    Write-Host '    已删: Update'" ^
  "  };" ^
  "  foreach ($k in Get-ChildItem $b) {" ^
  "    $vs = (Get-Item -LiteralPath $k.PSPath).GetValueNames();" ^
  "    if ($k.PSChildName -cmatch '^Registration\d' -and $vs) {" ^
  "      Remove-ItemProperty -LiteralPath $k.PSPath -Name $vs -Force;" ^
  "      Write-Host ('    已清: ' + $k.PSChildName)" ^
  "    } elseif ($k.PSChildName -cmatch '^[0-9A-F]{32}$') {" ^
  "      Remove-Item -LiteralPath $k.PSPath -Recurse -Force;" ^
  "      Write-Host ('    已删: ' + $k.PSChildName)" ^
  "    }" ^
  "  }" ^
  "} else { Write-Host '    未找到注册表项,跳过' }"

rem ---- 规律D: 删 AppData 隐藏 token ----
echo [4/4] 删除隐藏 token 文件...
set "D=%APPDATA%\PremiumSoft CyberTech\Navicat CC\Navicat Premium"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$d = '%D%';" ^
  "if (Test-Path -LiteralPath $d) {" ^
  "  foreach ($f in Get-ChildItem -LiteralPath $d -Force -File) {" ^
  "    if ($f.Name -cmatch '^\.[0-9A-F]{32}$' -and $f.Length -eq 32) {" ^
  "      Remove-Item -LiteralPath $f.FullName -Force;" ^
  "      Write-Host ('    已删: ' + $f.Name)" ^
  "    }" ^
  "  }" ^
  "} else { Write-Host '    未找到数据目录,跳过' }"

echo.
echo 完成! 重新打开 Navicat 即为全新 14 天试用期。
pause
