#!/bin/bash
# =============================================================
# reset_navicat_universal.sh — Navicat 试用期重置·跨机器通用版
#
# 设计原则:不硬编码任何键名/文件名,只认结构规律 →
#   规律1: plist 里所有「32位大写hex命名的顶层键」= 加密状态区
#          (试用记录主/备、会话状态快照、按需状态键) → 全删
#   规律2: App Support 目录下「文件名32hex + 恰好32字节」的隐藏
#          文件 = 应用实例 token → 全删
#   其余(可读键名的明文设置区)一律不动 → 连接/界面设置全保留
#
# 用法: sh reset_navicat_universal.sh ["Navicat 应用显示名"]
#   例: sh reset_navicat_universal.sh "<你的应用名>"
#   不传参默认 "Navicat Premium"。无需 sudo,无需重启电脑。
# 适配: macOS,Navicat Premium 16.x/17.x/18.x(理论兼容,按结构工作)
# =============================================================

APP_NAME="${1:-Navicat Premium}"

PLIST="$HOME/Library/Preferences/com.navicat.NavicatPremium.plist"
APPDIR="$HOME/Library/Application Support/PremiumSoft CyberTech/Navicat CC/Navicat Premium"

# ---------- [1/4] 退出 Navicat(防止内存态写回) ----------
echo "[1/4] 退出 Navicat…"
osascript -e "quit app \"$APP_NAME\"" 2>/dev/null
for i in {1..10}; do
  pgrep -fi "Navicat" >/dev/null || break
  sleep 1
done
# 兜底:结束所有残留 Navicat 进程(兼容 Premium/Premium 2/Lite 等命名)
pgrep -fi "Navicat" >/dev/null && { pkill -fi "Navicat"; sleep 2; }
echo "    已退出"

# ---------- [2/4] 清偏好缓存(防止 plist 从缓存复活) ----------
echo "[2/4] 清除偏好缓存守护(cfprefsd)…"
killall cfprefsd 2>/dev/null && { sleep 1; echo "    已清"; } || echo "    无需清理"

# ---------- [3/4] 删除加密状态区(规律1:顶层32hex键全删) ----------
echo "[3/4] 清除 plist 加密状态键…"
if [ ! -f "$PLIST" ]; then
  echo "    未找到 $PLIST,跳过(可能未安装或为 Lite 版)"
else
  KEYS=$(plutil -p "$PLIST" | grep -oE '^  "[0-9A-F]{32}"' | tr -d '" ')
  if [ -z "$KEYS" ]; then
    echo "    无加密键,跳过"
  else
    N=0
    while IFS= read -r K; do
      [ -z "$K" ] && continue
      /usr/libexec/PlistBuddy -c "Delete :$K" "$PLIST" 2>/dev/null && { echo "    已删: $K"; N=$((N+1)); }
    done <<< "$KEYS"
    echo "    共清除 $N 个加密状态键(试用主/备+状态快照,明文设置未动)"
  fi
fi

# ---------- [4/4] 删除应用实例 token(规律2:32hex名+32字节) ----------
echo "[4/4] 删除隐藏 token 文件…"
if [ ! -d "$APPDIR" ]; then
  echo "    未找到 $APPDIR,跳过"
else
  CNT=$(find "$APPDIR" -maxdepth 1 -type f -size 32c \
    -name '.????????????????????????????????' 2>/dev/null | wc -l | tr -d ' ')
  find "$APPDIR" -maxdepth 1 -type f -size 32c \
    -name '.????????????????????????????????' -delete 2>/dev/null
  echo "    已删除 $CNT 个 token(多实例共存时会一并重置,无害)"
fi

echo "完成!打开 $APP_NAME 即为全新 14 天试用期。"
echo "(本脚本按结构规律工作,不依赖具体键名/文件名,可在其他 Mac 上直接使用)"
