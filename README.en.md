# navicat-trial-reset

Resets the Navicat Premium trial period on macOS and Windows. Single file, no dependencies. For learning purposes only — please support the official product: [navicat premium](https://www.navicat.com/download/navicat-premium).

## Usage

- macOS: double-click `mac/reset_navicat_universal.command`, or run `sh mac/reset_navicat_universal.sh`
- Windows: double-click `windows/reset_navicat_universal.bat`

No admin/sudo rights needed. The script kills Navicat processes first, cleans the state, and the next launch starts a fresh 14-day trial. Only trial state is removed — database connections, query history and UI settings are all preserved.

## How it works

The trial start date is stored encrypted with one primary record plus backups; removing only the primary lets a snapshot restore the old value. The scripts match storage structure patterns instead of hardcoded names; plaintext settings (connections, UI) are untouched.

macOS:

- `~/Library/Preferences/com.navicat.NavicatPremium.plist`: remove all top-level 32-char uppercase-hex keys (trial record primary/backup, state snapshot)
- `~/Library/Application Support/PremiumSoft CyberTech/Navicat CC/Navicat Premium/`: remove hidden `.<32hex>` files that are exactly 32 bytes
- kill cfprefsd afterwards so the prefs cache does not write back

Windows:

- `HKCU\Software\Classes\CLSID` (incl. WOW6432Node): remove whole subkeys that have an `Info` value, or whose `ShellFolder` data points to Navicat (where the 16/17/18 trial clock lives; Navicat rebuilds them on next launch)
- `HKCU\Software\PremiumSoft\NavicatPremium`: clear all values of `Registration*` subkeys (PreTrial trial record); remove 32-hex subkeys (v12~15); remove the `Update` subkey (update-check cache only)
- `%APPDATA%\PremiumSoft CyberTech\Navicat CC\Navicat Premium`: remove hidden `.<32hex>` files that are exactly 32 bytes

## Verified

| Navicat version | Result |
|---|---|
| 18.0.3 | verified on macOS and Windows 10 |

Other versions are not individually verified; the scripts match storage-structure patterns and should work the same — add a row after verifying. If a reset does not stick, open an issue.

## Acknowledgments

The idea comes from [pretend-m/navicat_for_mac_reset](https://github.com/pretend-m/navicat_for_mac_reset).

## License

[MIT](LICENSE)
