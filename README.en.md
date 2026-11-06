# navicat-trial-reset

Resets the Navicat Premium trial period on macOS and Windows. For learning purposes only — please support the official product: [navicat premium](https://www.navicat.com/download/navicat-premium).

## Usage

- macOS: double-click `mac/reset_navicat_universal.command`, or run `sh mac/reset_navicat_universal.sh` (no sudo needed)

- Windows (Navicat 18.x, pick one):
  1. **Auto reset (recommended, deploy once and forget)**: double-click `windows/auto-reset-proxy/apply_autoreset_18.bat`, accept the UAC prompt. From then on, every Navicat launch closes the trial/registration popup within 0.2 s and deletes the trial state key after startup — permanently 14 full trial days, never expires, zero maintenance.
  2. **Manual reset**: fully close Navicat, then double-click `windows/reset_trial_18.bat`. Re-run whenever the trial is about to expire.
  3. **Clean DLL patch**: double-click `windows/apply_winmm_patch_18.bat` to deploy the clean-room `winmm.dll` from `windows/clean-proxy/` (plain C source in the same directory, fully auditable; patch bytes are byte-identical to the original Wimmm patch, and memory is only written when the signature matches).

- Uninstall: delete `C:\Program Files\Navicat\Navicat Premium 18\winmm.dll` to restore the stock state (the deployer backs up the original as `winmm.dll.orig.bak`, the previous version as `winmm.dll.prev.bak`).
- Logs: deployment actions go to `windows\deploy_log.txt`; DLL runtime behavior goes to `%TEMP%\navicat_autoreset.log`.

Only trial state is removed — database connections, query history and UI settings are all preserved.

## How it works

The trial start date is stored encrypted with one primary record plus backups; removing only the primary lets a snapshot restore the old value. The scripts match storage structure patterns instead of hardcoded names; plaintext settings (connections, UI) are untouched.

### macOS

- `~/Library/Preferences/com.navicat.NavicatPremium.plist`: remove all top-level 32-char uppercase-hex keys (trial record primary/backup, state snapshot)
- `~/Library/Application Support/PremiumSoft CyberTech/Navicat CC/Navicat Premium/`: remove hidden `.<32hex>` files that are exactly 32 bytes
- kill cfprefsd afterwards so the prefs cache does not write back

### Windows 18.x

- The trial master state lives in `HKCU\Software\PremiumSoft\NavicatPremium\Registration18XCS` (encrypted values, including the trial start date)
- **Key fact (proven by an A/B test)**: deleting this key **while Navicat is running has no effect** — on exit/continue it writes the in-memory start date back (ciphertext differs each time, the date does not). Every "deleting it didn't work" attempt came from this. Resets must happen with Navicat fully closed; `reset_trial_18.bat` taskkills first, then deletes
- `winmm.dll` is a static import of navicat.exe, so a proxy DLL with the same name in the app folder gets loaded by the loader first: all 181 winmm exports are forwarded to the system DLL (multimedia APIs untouched), and only when the host is `navicat.exe` does it verify an 11-byte signature at RVA `0x993699` in memory before patching (falls back to a whole-.text scan; if nothing matches, memory stays untouched). Reverse-engineering details: [winmm.dll逆向报告.md](winmm.dll逆向报告.md) (Chinese)
- `auto-reset-proxy` (v4) adds three lines of defense on top of the clean build:
  1. For 45 s after launch, poll every 200 ms for `TRegistrationSubForm` windows (trial reminder / registration) and `WM_CLOSE` them on sight (visible <0.2 s)
  2. After the startup sequence completes, delete `Registration18XCS` (45 s fallback delete, so state never ages across sessions)
  3. Delete again in `DLL_PROCESS_DETACH` on graceful exit (double insurance)
  Version history and lessons learned (v1 startup-time delete triggers the registration window, etc.): [windows/auto-reset-proxy/README.md](windows/auto-reset-proxy/README.md) (Chinese)
- The real source of the "N days remaining" popup, plus the A/B experiment comparing the original 2023 DLL with the clean rebuild (registry evidence in `windows/Registration18XCS.*.reg`): [弹窗真相_A-B实测报告.md](弹窗真相_A-B实测报告.md) (Chinese)

### Windows 16 and earlier (legacy approach)

The original `windows/reset_navicat_universal.bat` (v1.0.0: removing CLSID keys containing an `Info` subkey, `PremiumSoft\NavicatPremium\Registration*` keys, the `navicat_cred` credential-manager entry, and 32-byte token files under `PremiumSoft CyberTech`) does not affect Navicat 17/18 — the 18.x trial start date is not stored in any of those places. The script has been removed from the repo; fetch it from the v1.0.0 commit history if needed.

## Verified

| Navicat version | Result |
|---|---|
| 18.0.3 | verified on macOS and Windows 10 |
| 18.0.5 | verified on Windows 10 (auto-reset-proxy v4: popup lifetime <0.2 s, permanently 14 full trial days) |

Other versions are not individually verified; the scripts match storage-structure patterns and should work the same — add a row after verifying. If a reset does not stick, open an issue.

## Acknowledgments

The idea comes from [pretend-m/navicat_for_mac_reset](https://github.com/pretend-m/navicat_for_mac_reset).

## License

[MIT](LICENSE)
