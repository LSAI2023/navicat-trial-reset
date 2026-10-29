# navicat-trial-reset

Navicat Premium 试用期重置脚本，macOS / Windows 双平台，单文件、无依赖。仅供学习研究，请支持正版：[navicat premium](https://www.navicat.com.cn/download/navicat-premium)。

## 使用

- macOS：双击 `mac/reset_navicat_universal.command`，或 `sh mac/reset_navicat_universal.sh`
- Windows：双击 `windows/reset_navicat_universal.bat`

无需管理员 / sudo 权限。脚本会先结束 Navicat 进程再清理，完成后重新打开即为新的 14 天试用期。重置只清除试用状态，数据库连接、查询历史、界面设置等用户数据全部保留。

## 原理

试用起始日期被加密存储且一主多备，只删主记录会被快照恢复旧值。脚本不硬编码键名，按存储结构规律清除；连接、界面等明文设置不受影响。

macOS：

- `~/Library/Preferences/com.navicat.NavicatPremium.plist`：顶层 32 位大写 hex 键（试用记录主/备、状态快照）全部删除
- `~/Library/Application Support/PremiumSoft CyberTech/Navicat CC/Navicat Premium/`：`.<32hex>` 且恰为 32 字节的隐藏 token 文件删除
- 清理后 kill cfprefsd，防止偏好缓存写回

Windows：

- `HKCU\Software\Classes\CLSID`（含 WOW6432Node）下：含 `Info` 值、或 `ShellFolder` 数据指向 Navicat 的子键，整键删除（16/17/18 的试用计时在此，Navicat 重启后自动重建）
- `HKCU\Software\PremiumSoft\NavicatPremium` 下：`Registration*` 子键清空所有值（PreTrial 试用记录）；32 位大写 hex 子键（v12~15）删除；`Update` 子键删除（仅更新缓存，自动重建）
- `%APPDATA%\PremiumSoft CyberTech\Navicat CC\Navicat Premium` 下：`.<32hex>` 且 32 字节的隐藏 token 文件删除

## 已验证

| Navicat 版本 | 结果 |
|---|---|
| 18.0.3 | macOS / Windows 10 实测通过 |

其他版本未逐一验证，脚本按存储结构规律工作，应该同样可用，验证后在表中加一行即可。重置无效请提 issue。

## 致谢

思路来自 [pretend-m/navicat_for_mac_reset](https://github.com/pretend-m/navicat_for_mac_reset)。

## License

[MIT](LICENSE)
