#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — 本机 Android 构建链一键装配/构建（2026-08-24, D:\green 布局）
#
#  用途：一次性把本机 Android 工具链环境变量 + SDL3/OpenSSL android 切片 +
#        引擎交叉编译 + 本地 APK 组装打通，不依赖 CI。已装组件自动跳过。
#
#  用法:  bash scripts/setup_android_local.sh [--smoke|--apk|--install]
#    --smoke   : 只交叉编译模块库(验证工具链)
#    --apk     : 继续装配 + gradle assembleDebug 出本地 APK
#    --install : 继续 adb install -r 到已连接设备（默认安装后启动）
#    缺省      : 全链（smoke → apk → install）
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# -- 本机路径（绿色化 D:\green 布局） -----------------------------------
export JAVA_HOME="${JAVA_HOME:-/d/green/android-tools/jdk-17.0.20+8}"
export ANDROID_HOME="${ANDROID_HOME:-/d/green}"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export PATH="$JAVA_HOME/bin:/d/green/android-tools/gradle-8.9/bin:/d/green/android-tools/strawberry-perl/perl/bin:$PATH"
NDK="${NDK:-$ANDROID_HOME/ndk/27.3.13750724}"
SRC_DIR=/d/green/android-build-src
SDL3_DIR="${SDL3_DIR:-$SRC_DIR/sdl3-android/lib/cmake/SDL3}"
OPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-$SRC_DIR/openssl-android}"
ADB="/d/green/platform-tools/adb.exe"

MODE="${1:---all}"
for a in "$@"; do case "$a" in --smoke|--apk|--install) MODE="$a";; esac; done

require() {
    if [[ ! -d "$1" ]]; then echo "ERROR: missing $2: $1"; exit 2; fi
}
require "$JAVA_HOME" "JDK17"; require "$NDK" "NDK r27.3"; require "$ANDROID_HOME/cmdline-tools/latest" "cmdline-tools"

# -- 1) SDL3 android slice (install 到 sdl3-android) ---------------------
if [[ -f "$SDL3_DIR/SDL3Config.cmake" ]]; then
    echo "[local-android] SDL3 slice exists: $SDL3_DIR"
else
    echo "[local-android] building SDL3 android slice (arm64-v8a)..."
    rm -rf "$SRC_DIR/SDL/build-android" "$SRC_DIR/sdl3-android"
    # VS 2022 generator would hijack the NDK toolchain with MSBuild's built-in
    # Android targets (r23c); MinGW Makefiles keeps the NDK in control.
    cmake -G "Ninja" -S "$SRC_DIR/SDL" -B "$SRC_DIR/SDL/build-android" \
        -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
        -DCMAKE_BUILD_TYPE=Release -DSDL_STATIC=ON -DSDL_SHARED=ON \
        -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_DOCS=OFF \
        -DCMAKE_INSTALL_PREFIX="$SRC_DIR/sdl3-android"
    cmake --build "$SRC_DIR/SDL/build-android" --parallel 8
    cmake --install "$SRC_DIR/SDL/build-android"
fi

# -- 2) OpenSSL android slice -------------------------------------------
if [[ -f "$OPENSSL_ROOT_DIR/lib/libssl.a" && -f "$OPENSSL_ROOT_DIR/lib/libcrypto.a" ]]; then
    echo "[local-android] OpenSSL slice exists: $OPENSSL_ROOT_DIR"
else
    echo "[local-android] configuring OpenSSL android-arm64..."
    (cd "$SRC_DIR/openssl" && export ANDROID_NDK_ROOT="$NDK" && \
     export PATH="$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin:$PATH" && \
     ./Configure android-arm64 --prefix="$OPENSSL_ROOT_DIR" -D__ANDROID_API__=24 && \
     make -j8 && make install_sw)
fi

# -- 3) 引擎交叉编译（smoke=模块库） ------------------------------------
echo "[local-android] engine cross-compile (--smoke) ..."
bash "$SCRIPT_DIR/build_android.sh" \
    --ndk "$NDK" --sdl3 "$SDL3_DIR" --openssl "$OPENSSL_ROOT_DIR" --smoke

if [[ "$MODE" == "--smoke" ]]; then echo "[local-android] SMOKE DONE"; exit 0; fi

# -- 4) 装配 + gradle APK ------------------------------------------------
ABI_OUT="$REPO_ROOT/build-android-arm64-v8a"
JNI="$REPO_ROOT/android/app/src/main/jniLibs/arm64-v8a"
mkdir -p "$JNI"
cp "$ABI_OUT/libCaesuraAmeKAG.so" "$JNI/"
cp "$SRC_DIR/sdl3-android/lib/libSDL3.so" "$JNI/"
rm -rf "$REPO_ROOT/android/app/src/main/assets/game"
mkdir -p "$REPO_ROOT/android/app/src/main/assets/game/demo"
cp -r "$REPO_ROOT/scripts" "$REPO_ROOT/android/app/src/main/assets/game/scripts"
cp -r "$REPO_ROOT/assets"  "$REPO_ROOT/android/app/src/main/assets/game/assets"
cp -r "$REPO_ROOT/tests/projects/first_vn" "$REPO_ROOT/android/app/src/main/assets/game/demo/first_vn"
sed -i 's|config.entry_script = .*|config.entry_script = "../demo/first_vn/entry.lua"|' "$REPO_ROOT/android/app/src/main/assets/game/scripts/config.lua"
echo "[local-android] gradle assembleDebug ..."
(cd "$REPO_ROOT/android" && gradle --no-daemon --console=plain assembleDebug)
APK="$REPO_ROOT/android/app/build/outputs/apk/debug/app-debug.apk"
[[ -f "$APK" ]] || { echo "ERROR: APK not produced"; exit 1; }
echo "[local-android] APK: $APK"

if [[ "$MODE" == "--apk" ]]; then echo "[local-android] APK DONE"; exit 0; fi

# -- 5) 安装 + 启动 -----------------------------------------------------
echo "[local-android] adb install -r ..."
"$ADB" install -r "$APK"
echo "[local-android] launching ..."
"$ADB" shell am start -n com.caesura.app/com.caesura.app.MainActivity
echo "[local-android] INSTALL DONE (logcat: adb logcat -s SDL:V CaesuraAmeKAG:V)"