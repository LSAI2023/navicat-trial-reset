# navicat-trial-reset

Navicat Premium 试用期重置工具，macOS / Windows 双平台。仅供学习研究，请支持正版：[navicat premium](https://www.navicat.com.cn/download/navicat-premium)。

## 使用

- macOS：双击 `mac/reset_navicat_universal.command`，或 `sh mac/reset_navicat_universal.sh`（无需 sudo）

- Windows（Navicat 18.x，任选其一）：
  1. **自动重置（推荐，一次部署永久生效）**：双击 `windows/auto-reset-proxy/apply_autoreset_18.bat`，UAC 点一次"是"。之后每次启动 Navicat：试用提醒/注册窗口 0.2 秒内自动关闭，启动结束后自动删除试用状态键——永远 14 天满试用、永不到期、无需维护。
  2. **手动重置**：完全关闭 Navicat 后双击 `windows/reset_trial_18.bat`。试用将满时再跑一次即可。
  3. **纯净 DLL 补丁**：双击 `windows/apply_winmm_patch_18.bat`，部署 `windows/clean-proxy/` 的净化重制版 `winmm.dll`（明文 C 源码同目录可审计；补丁字节与原版 Wimmm 补丁逐字节等效，且签名不符不写内存）。

- 还原：删除 `C:\Program Files\Navicat\Navicat Premium 18\winmm.dll` 即回到系统原版（部署时原文件自动备份为 `winmm.dll.orig.bak` / 上一版为 `winmm.dll.prev.bak`）。
- 日志：部署动作写入 `windows\deploy_log.txt`；DLL 运行时行为写入 `%TEMP%\navicat_autoreset.log`。

重置只清除试用状态，数据库连接、查询历史、界面设置等用户数据全部保留。

## 原理

试用起始日期被加密存储且一主多备，只删主记录会被快照恢复旧值。脚本不硬编码键名，按存储结构规律清除；连接、界面等明文设置不受影响。

### macOS

- `~/Library/Preferences/com.navicat.NavicatPremium.plist`：顶层 32 位大写 hex 键（试用记录主/备、状态快照）全部删除
- `~/Library/Application Support/PremiumSoft CyberTech/Navicat CC/Navicat Premium/`：`.<32hex>` 且恰为 32 字节的隐藏 token 文件删除
- 清理后 kill cfprefsd，防止偏好缓存写回

### Windows 18.x

- 试用主状态在 `HKCU\Software\PremiumSoft\NavicatPremium\Registration18XCS`（加密值，含试用起始日）
- **关键事实（A/B 实测证实）**：Navicat **运行期间**删该键无效——它会退出/继续运行时把内存里的旧起始日期加密写回（密文每次不同，日期不变）。历史上"删了没用"全部源于此。所有重置必须在 Navicat 完全关闭后进行，`reset_trial_18.bat` 即先 taskkill 再删键
- `winmm.dll` 是 navicat.exe 的静态导入，与主程序同目录的代理 DLL 会被加载器优先加载：全量转发系统 winmm 的 181 个导出（多媒体 API 无损），仅当宿主为 `navicat.exe` 时对内存 RVA `0x993699` 做 11 字节签名校验并补丁（硬编码点未命中再全 .text 扫描，仍不中则不动内存）。逆向细节见 [winmm.dll逆向报告.md](winmm.dll逆向报告.md)
- `auto-reset-proxy`（v4）在纯净版之上叠加三道防线：
  1. 启动后 45 秒内每 200ms 扫描本进程 `TRegistrationSubForm`（试用提醒/注册窗口），一出现立即 `WM_CLOSE`（存活 <0.2 秒）
  2. 启动流程结束后删除 `Registration18XCS`（45 秒兜底删键，状态不随会话老化）
  3. 正常退出时 `DLL_PROCESS_DETACH` 再删一次（双保险）
  版本演进与教训（v1 启动删键会触发注册窗口等）见 [windows/auto-reset-proxy/README.md](windows/auto-reset-proxy/README.md)
- "剩余 N 天"弹窗的真实来源、原版 DLL 与复刻版的 A/B 对比实验（含注册表证据 `windows/Registration18XCS.*.reg`）见 [弹窗真相_A-B实测报告.md](弹窗真相_A-B实测报告.md)

### Windows 16 及更早（历史方案）

原 `windows/reset_navicat_universal.bat`（v1.0.0：清理 `HKCU\Software\Classes\CLSID` 下含 `Info` 子键的键、`PremiumSoft\NavicatPremium` 的 `Registration*` 子键、凭据管理器 `navicat_cred`、`PremiumSoft CyberTech` 目录的 32 字节 token 文件）对 Navicat 17/18 无效——18 的试用起始日不在这些存储中。该脚本已从仓库移除，需要可从 v1.0.0 提交历史获取。

## 已验证

| Navicat 版本 | 结果 |
|---|---|
| 18.0.3 | macOS / Windows 10 实测通过 |
| 18.0.5 | Windows 10 实测通过（auto-reset-proxy v4：弹窗存活 <0.2s，永远 14 天满试用） |

其他版本未逐一验证，脚本按存储结构规律工作，应该同样可用，验证后在表中加一行即可。重置无效请提 issue。

## 致谢

思路来自 [pretend-m/navicat_for_mac_reset](https://github.com/pretend-m/navicat_for_mac_reset)。

## License

[MIT](LICENSE)
