#!/bin/bash
# =============================================================
# reset_navicat_universal.command — Mac 双击版(免安装,单文件)
# 自动探测 /Applications 下的 Navicat 应用 → 退出 → 按结构规律清除
#   规律1: plist 顶层 32-hex 键全删(加密状态区)
#   规律2: App Support 下 32hex 名+32字节隐藏文件全删(token)
# 首次双击如提示"无法验证开发者":右键 → 打开 → 再点打开(仅一次)
# =============================================================

PLIST="$HOME/Library/Preferences/com.navicat.NavicatPremium.plist"
APPDIR="$HOME/Library/Application Support/PremiumSoft CyberTech/Navicat CC/Navicat Premium"

echo "=== Navicat 试用期重置(Mac 通用版) ==="
echo

# [1/4] 退出所有 Navicat 应用(自动探测应用名)
echo "[1/4] 退出 Navicat…"
for APP in /Applications/Navicat*.app; do
  [ -d "$APP" ] || continue
  N=$(basename "$APP" .app)
  osascript -e "quit app \"$N\"" 2>/dev/null
done
for i in {1..10}; do
  pgrep -fi "Navicat" >/dev/null || break
  sleep 1
done
pgrep -fi "Navicat" >/dev/null && { pkill -fi "Navicat"; sleep 2; }
killall cfprefsd 2>/dev/null && sleep 1
echo "    已退出全部 Navicat 并清理偏好缓存"

# [2/4] 删除加密状态区(规律1)
echo "[2/4] 清除 plist 加密状态键…"
if [ ! -f "$PLIST" ]; then
  echo "    未找到 plist,跳过"
else
  N=0
  KEYS=$(plutil -p "$PLIST" | grep -oE '^  "[0-9A-F]{32}"' | tr -d '" ')
  if [ -z "$KEYS" ]; then
    echo "    无加密键,跳过"
  else
    while IFS= read -r K; do
      [ -z "$K" ] && continue
      /usr/libexec/PlistBuddy -c "Delete :$K" "$PLIST" 2>/dev/null && { echo "    已删: $K"; N=$((N+1)); }
    done <<< "$KEYS"
    echo "    共清除 $N 个(明文设置未动)"
  fi
fi

# [3/4] 删除 token(规律2)
echo "[3/4] 删除隐藏 token 文件…"
if [ ! -d "$APPDIR" ]; then
  echo "    未找到数据目录,跳过"
else
  CNT=$(find "$APPDIR" -maxdepth 1 -type f -size 32c \
    -name '.????????????????????????????????' 2>/dev/null | wc -l | tr -d ' ')
  find "$APPDIR" -maxdepth 1 -type f -size 32c \
    -name '.????????????????????????????????' -delete 2>/dev/null
  echo "    已删除 $CNT 个"
fi

# [4/4] 完成
echo "[4/4] 完成!重新打开 Navicat 即为全新 14 天试用期。"
echo
read -p "按回车键关闭本窗口…"
