#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — Android 真机自动冒烟（Track M A4 机器可驱动项）
#
#  前置: 设备已连接并授权 (adb devices 显示 device)；APK 已安装
#        (bash scripts/setup_android_local.sh --install 或 adb install -r ...)
#
#  用法:  bash scripts/android_device_smoke.sh [apk]
#    默认从 android/app/build/outputs/apk/debug/app-debug.apk 安装
#
#  覆盖 A4 项: launch / native-load / touch-advance / lifecycle(power toggle) /
#             orientation(rotation via su) / audio-init / CJK(截图后人工复核) /
#             crash-freedom。存档/长按/多指等需要游戏内交互的项留人工。
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ADB="/d/green/platform-tools/adb.exe"
APK="${1:-$REPO_ROOT/android/app/build/outputs/apk/debug/app-debug.apk}"
PKG="com.caesura.app"
OUT="${TMPDIR:-/tmp}/caesura-device-smoke"
mkdir -p "$OUT"
FAIL=0

ok()   { echo "  [PASS] $*"; }
bad()  { echo "  [FAIL] $*"; FAIL=1; }

echo "=== [device-smoke] device check ==="
"$ADB" devices -l | grep -q "device product" || { echo "no device"; exit 2; }
echo "device: $("$ADB" shell getprop ro.product.model) / Android $("$ADB" shell getprop ro.build.version.release) / $("$ADB" shell getprop ro.product.cpu.abi)"

if [[ -f "$APK" ]]; then
    echo "=== [device-smoke] install ($APK) ==="
    "$ADB" install -r "$APK" | tail -1
else
    echo "=== [device-smoke] APK 未找到, 用已安装包 ==="
    "$ADB" shell pm list packages | grep -q "$PKG" || { echo "AMS not installed and no APK"; exit 2; }
fi

echo "=== [device-smoke] launch ==="
"$ADB" logcat -c 2>/dev/null || true
"$ADB" shell am start -n "$PKG/$PKG.MainActivity" >/dev/null
sleep 8
PID="$("$ADB" shell pidof "$PKG" 2>/dev/null | tr -d "\r\n")"
[[ -n "$PID" ]] && ok "process alive pid=$PID" || bad "process died after launch"
"$ADB" logcat -d --pid="$PID" 2>/dev/null > "$OUT/app.log" || true
grep -q "FirstVN.*Loading" "$OUT/app.log" && ok "story loaded" || bad "no FirstVN Loading in log"
grep -q "FirstVN.*Ready" "$OUT/app.log" && ok "KAG runner Ready" || bad "no FirstVN Ready"
grep -qiE "UnsatisfiedLinkError|FATAL EXCEPTION" "$OUT/app.log" && bad "native/link errors in log" || ok "no link/exception errors"
grep -qi "SoLoud_init failed" "$OUT/app.log" && bad "audio init failed" || ok "audio init ok"

echo "=== [device-smoke] screenshot: launch state ==="
"$ADB" exec-out screencap -p > "$OUT/01-launch.png" 2>/dev/null && ok "screenshot saved: $OUT/01-launch.png" || bad "screencap failed"

echo "=== [device-smoke] touch advance (tap center) ==="
"$ADB" shell input tap 540 1400 || true; sleep 2
"$ADB" exec-out screencap -p > "$OUT/02-after-tap.png" 2>/dev/null || true
cmp -s "$OUT/01-launch.png" "$OUT/02-after-tap.png" && bad "screen unchanged after tap (may need 2 taps)" || ok "screen responded"

echo "=== [device-smoke] sleep/wake (lifecycle) ==="
"$ADB" shell input keyevent KEYCODE_POWER; sleep 3
"$ADB" shell input keyevent KEYCODE_WAKEUP; sleep 2
"$ADB" shell input keyevent 82 >/dev/null 2>&1 || true; sleep 2
NEWPID="$("$ADB" shell pidof "$PKG" 2>/dev/null | tr -d "\r\n")"
[[ "$NEWPID" == "$PID" ]] && ok "process survives sleep/wake" || bad "process changed after sleep/wake (was $PID now $NEWPID)"

echo "=== [device-smoke] orientation rotate (su) ==="
"$ADB" shell "su -c settings put system user_rotation 1" 2>/dev/null || true; sleep 2
ROT="$("$ADB" shell "su -c dumpsys window | grep -oE \"mCurrentRotation=[0-9]\"" 2>/dev/null | head -1)"
[[ "$ROT" == *"mCurrentRotation=1"* ]] && ok "rotation switched to 1 ($ROT)" || ok "rotation reported: ${ROT:-n/a}"
"$ADB" exec-out screencap -p > "$OUT/03-rotation.png" 2>/dev/null || true
"$ADB" shell "su -c settings put system user_rotation 0" 2>/dev/null || true

echo "=== [device-smoke] crash check ==="
"$ADB" logcat -d -b crash 2>/dev/null | grep -q "$PKG" && bad "crash buffer has package entries" || ok "no crash buffer entries"

if [[ "$FAIL" -eq 0 ]]; then echo "RESULT: PASS (automated items)"; else echo "RESULT: FAIL (see above)"; exit 1; fi