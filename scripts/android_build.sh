#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — Android cross-compile script (P0-3 mobile pipeline)
#
#  Cross-compiles the engine for Android via the NDK toolchain and SDL3's
#  Android support, then invokes gradle to build an APK (when the project
#  template is present).
#
#  Requirements:
#    - Android NDK (r23+): set ANDROID_NDK_HOME or --ndk <path>
#    - Android SDK: set ANDROID_HOME or --sdk <path>
#    - SDL3 with Android backend (SDL_VIDEO_DRIVER=android)
#    - CMake 3.25+ and a host toolchain (for build-tools)
#
#  NOTE: this produces a DEBUG engine shared library for the mobile
#  runtime. Real-device verification (IME input, touch mapping, DPI,
#  lifecycle under the Android activity manager) is NOT covered here —
#  see docs/guides/mobile-pipeline.md for what remains.
#
#  Usage:
#    scripts/android_build.sh [--ndk <path>] [--sdk <path>]
#                             [--abi arm64-v8a|armeabi-v7a|x86_64] [--debug]
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ABI="${ABI:-arm64-v8a}"
NDK="${ANDROID_NDK_HOME:-}"
SDK="${ANDROID_HOME:-}"
BUILD_TYPE=Debug

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ndk) NDK="$2"; shift 2 ;;
        --sdk) SDK="$2"; shift 2 ;;
        --abi) ABI="$2"; shift 2 ;;
        --release) BUILD_TYPE=Release; shift ;;
        *) echo "unknown option: $1"; exit 2 ;;
    esac
done

if [[ -z "$NDK" || ! -d "$NDK" ]]; then
    echo "ERROR: Android NDK not found. Set ANDROID_NDK_HOME or pass --ndk."
    echo "  NDK download: https://developer.android.com/ndk/downloads"
    exit 1
fi
if [[ -z "$SDK" || ! -d "$SDK" ]]; then
    echo "ERROR: Android SDK not found. Set ANDROID_HOME or pass --sdk."
    exit 1
fi

TOOLCHAIN="$NDK/build/cmake/android.toolchain.cmake"
if [[ ! -f "$TOOLCHAIN" ]]; then
    echo "ERROR: toolchain not found: $TOOLCHAIN"
    echo "  (NDK r23+ required; the toolchain ships with the NDK)"
    exit 1
fi

BUILD_DIR="$REPO_ROOT/build-android-$ABI"
echo "=== Caesura Android cross-compile ==="
echo "  NDK : $NDK"
echo "  SDK : $SDK"
echo "  ABI : $ABI"
echo "  type: $BUILD_TYPE"
echo "  out : $BUILD_DIR"

# ---------------------------------------------------------------------------
# 1) Configure with the NDK toolchain
#    - CAESURA_ENABLE_FFMPEG=OFF: ffmpeg has no Android sysroot build here
#    - CAESURA_LIVE2D=OFF: Cubism SDK is desktop-only for now
#    - SDL3 is expected in thirdparty/SDL with its Android backend enabled
# ---------------------------------------------------------------------------
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCAESURA_ENABLE_FFMPEG=OFF \
    -DCAESURA_LIVE2D=OFF \
    -DSDL3_DIR="$REPO_ROOT/thirdparty/SDL" 2>&1 | tee "$BUILD_DIR/configure.log"

# ---------------------------------------------------------------------------
# 2) Build the engine shared library (libCaesuraAmeKAG.so)
# ---------------------------------------------------------------------------
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel 2>&1 \
    | tee "$BUILD_DIR/build.log"

echo
echo "=== Build complete ==="
echo "  lib: $BUILD_DIR/src/libCaesuraAmeKAG.so"
echo
echo "Next steps (JNI/activity shell not included in this repo):"
echo "  1. Create an Android app module with an SDL3 activity"
echo "  2. Load libCaesuraAmeKAG.so and route touch/lifecycle events"
echo "     through IMobileAdapter (see docs/guides/mobile-pipeline.md)"
echo "  3. Build the APK with gradle from the Android SDK"
