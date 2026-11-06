# auto-reset-proxy -- Navicat 18 自动重置 winmm.dll（v4）

## 与 clean-proxy 的区别
clean-proxy（旧复刻版）：只复刻原版补丁，不管试用状态。
auto-reset-proxy（本目录）：在复刻基础上新增三道防线，
让 Navicat 永远保持 14 天满试用、试用/注册窗口永不停留、永不到期。

## 实现原理（v4，四轮实测迭代得出）
- winmm.dll 是 navicat.exe 的**静态导入**（已在导入表确认），进程初始化即加载；
- 【教训 v1】启动中途删键 = Navicat 核心库读到状态后转眼消失 => 判定异常 => 弹注册窗口。废弃；
- 【教训 v2】依赖 DLL_PROCESS_DETACH 删键不可靠：Navicat 部分退出路径不执行 DETACH。仅保留不依赖；
- 【v4 三道防线】
  1. **弹窗自动关闭**：启动后 45 秒内每 200ms 扫描本进程的 TRegistrationSubForm
     （试用提醒/注册窗口），一出现立即发送 WM_CLOSE（等效用户点 X，存活 <0.2 秒）；
  2. **45 秒兜底删键**：启动流程结束后删除试用状态键（rc 写入日志），
     状态不会随会话老化 => 永远 14 天 => 到期不存在；
  3. **退出时删键**：正常关闭 Navicat 时 DLL_PROCESS_DETACH 再次删除（双保险）。
- 状态键：`HKCU\Software\PremiumSoft\NavicatPremium\Registration18XCS`

## 附带功能（继承自 clean-proxy）
- 180+1 个 winmm 导出逐序号转发系统 winmm.dll（多媒体 API 无损）；
- 11 字节补丁（与 2023 原版逐字节一致，保持复刻身份；对 18 无副作用）；
- 仅宿主为 navicat.exe 时生效，其他进程加载不动作。

## 日志
%TEMP%\navicat_autoreset.log
- `auto-close TRegistrationSubForm` —— 试用/注册窗口被自动关闭
- `late_reset rc=0` —— 45 秒兜底删键成功
- `detach_reset rc=0` —— 正常关闭时删键成功

## 使用
1. 双击 `apply_autoreset_18.bat`，UAC 点一次"是"；
2. 看 `deploy_log.txt` 出现 `HASH MATCH - DEPLOY_AUTORESET SUCCESS`；
3. 之后正常使用：启动时试用窗口只闪现不到 0.2 秒即自动消失，主界面直进，
   永远 14 天满试用，永不到期，无需任何手动维护。

## 回滚
- 部署时上一版自动备份为 `C:\Program Files\Navicat\Navicat Premium 18\winmm.dll.prev.bak`
  （需管理员权限覆盖回去）；
- 或改用旧复刻版：`windows\clean-proxy\winmm.dll`（配合 `windows\apply_winmm_patch_18.bat`）；
- 手动重置试用：`windows\reset_trial_18.bat`（关 Navicat 后运行）。

## 构建方式
双击 `build.bat`（需 MinGW-w64 x86_64 的 gcc：PATH 里已有则直接用；否则把脚本里的
`MINGW_BIN` 改成本机 MinGW 的 bin 目录）。产物 SHA256 应为
`AC0115830B01E21FAC75E1A826BE4C5A7400BFA5BCD96509CBAEE7CED65AD9C2`。
clean-proxy 构建命令同理（不带 `-ladvapi32`）。

## 版本
- v1（启动时删键）：废弃 —— 触发注册窗口。
- v2（仅退出删键）：废弃 —— DETACH 不总执行，且窗口问题未解决。
- v3（加窗口自动关闭）：验证通过，但缺 45 秒兜底删键。
- **v4（当前）：自动关窗 + 45 秒兜底删键 + 退出删键。全部实测通过。**
  winmm.dll SHA256 = AC0115830B01E21FAC75E1A826BE4C5A7400BFA5BCD96509CBAEE7CED65AD9C2
  实测记录：两轮启动验证（2026-11-04 17:16 / 17:18），弹窗存活 <0.2s、
  late_reset rc=0、删后无任何新窗口、主窗口 17~20s 直进。
