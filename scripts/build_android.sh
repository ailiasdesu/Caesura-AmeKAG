#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — Android cross-compile script (bash / forward-slash paths)
#
#  Cross-compiles the engine's static module libraries for Android via the NDK
#  toolchain. SDL3 must be provided as an Android-configured CMake package via SDL3_DIR
#  (see docs/guides/android-build.md §2.2) — the repo's external/SDL3/SDL3-3.2.0 is a
#  Windows x64 prebuilt, NOT a source tree. A JNI/Activity shell is not in this repo.
#
#  Platform options baked in (see the guide §3.1 for rationale):
#    CAESURA_LIVE2D=OFF          Cubism SDK is desktop-only today
#    CAESURA_ENABLE_FFMPEG=OFF   no Android ffmpeg sysroot build; pl_mpeg fallback
#    SOLOUD_BACKEND_OPENSLES=ON  Android audio backend (no AAudio in vendored)
#
#  Requirements:
#    - Android NDK r26+: set ANDROID_NDK_HOME / ANDROID_NDK, or --ndk <path>
#    - CMake 3.25+
#    - SDL3_DIR pointing at an Android SDL3 CMake package (or --sdl3 <path>)
#    - OPENSSL_ROOT_DIR pointing at an android-arm64 OpenSSL install (or --openssl <path>)
#      (archive crypto links libssl.a/libcrypto.a — same slice contract as iOS)
#
#  Usage:
#    scripts/build_android.sh [--ndk <path>] [--sdl3 <path>]
#                             [--abi arm64-v8a|armeabi-v7a|x86_64]
#                             [--release|--debug] [--platform android-24]
#                             [--target <cmake-target>] [--smoke]
#    --smoke  : build modules only (no executable/APK) — verification-only
#    --target : pass a specific CMake target (e.g. CaesuraEngine / CaesuraRender)
# =============================================================================
set -euo pipefail

# resolve repo root from this script's location (dirname/basename are fine here)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ABI="${ABI:-arm64-v8a}"
BUILD_TYPE=Debug
ANDROID_PLATFORM=android-24
TARGET=""
SMOKE=0

NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK:-}}"
SDL3_DIR="${SDL3_DIR:-}"
OPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-}"

# -- CLI parsing ------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --ndk)      NDK="$2"; shift 2 ;;
        --sdl3)     SDL3_DIR="$2"; shift 2 ;;
        --openssl)  OPENSSL_ROOT_DIR="$2"; shift 2 ;;
        --abi)      ABI="$2"; shift 2 ;;
        --platform) ANDROID_PLATFORM="$2"; shift 2 ;;
        --release)  BUILD_TYPE=Release; shift ;;
        --debug)    BUILD_TYPE=Debug; shift ;;
        --smoke)    SMOKE=1; shift ;;
        --target)   TARGET="$2"; shift 2 ;;
        -h|--help)  sed -n '1,60p' "$0"; exit 0 ;;
        *) echo "ERROR: unknown option: $1"; exit 2 ;;
    esac
done

# -- Auto-discover NDK if not given ------------------------------------------
if [[ -z "$NDK" || ! -d "$NDK" ]]; then
    # common install locations (order matters; first with a toolchain wins)
    for candidate in \
        "$LOCALAPPDATA/Android/Sdk/ndk"/* \
        "$HOME/Android/Sdk/ndk"/* \
        "${ANDROID_HOME:-}/ndk"/* \
        "${ANDROID_SDK_ROOT:-}/ndk"/* \
        "/opt/android-ndk" \
        "/usr/local/android-ndk"; do
        if [[ -d "$candidate/build/cmake" ]]; then
            NDK="$candidate"
            break
        fi
    done
fi

if [[ -z "$NDK" || ! -d "$NDK" ]]; then
    echo "ERROR: Android NDK not found."
    echo "  Set ANDROID_NDK_HOME (or ANDROID_NDK), or pass --ndk <path>."
    echo "  NDK download: https://developer.android.com/ndk/downloads"
    echo "  (suggest r26 or newer)"
    exit 1
fi

# -- Clear diagnostics for the plan's mandatory prerequisites --------------
if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: CMake not found in PATH (3.25+ required; NDK toolchain needs it)."
    echo "  Install CMake (e.g. https://cmake.org/download/) or add it to PATH."
    exit 1
fi
if [[ ! -d "$REPO_ROOT/assets" ]]; then
    echo "ERROR: game assets root missing: $REPO_ROOT/assets"
    echo "  The Android package ships demo assets under assets/; restore or create it."
    exit 1
fi

TOOLCHAIN="$NDK/build/cmake/android.toolchain.cmake"
if [[ ! -f "$TOOLCHAIN" ]]; then
    echo "ERROR: toolchain not found: $TOOLCHAIN"
    echo "  (NDK r23+ required; the toolchain ships with the NDK)"
    exit 1
fi

if [[ -z "$SDL3_DIR" || ! -f "$SDL3_DIR/SDL3Config.cmake" ]]; then
    echo "ERROR: SDL3_DIR not set or invalid (no SDL3Config.cmake found)."
    echo "  The repo bundles only a Windows x64 SDL3 prebuilt. Build/install an"
    echo "  Android SDL3 and point SDL3_DIR at its lib/cmake/SDL3 directory."
    echo "  See docs/guides/android-build.md, section 2.2."
    exit 2
fi

if [[ -z "$OPENSSL_ROOT_DIR" || ! -f "$OPENSSL_ROOT_DIR/lib/libssl.a" || ! -f "$OPENSSL_ROOT_DIR/lib/libcrypto.a" ]]; then
    echo "ERROR: OPENSSL_ROOT_DIR not set, or no android-arm64 slice (lib/libssl.a + lib/libcrypto.a)."
    echo "  Build OpenSSL with: ./Configure android-arm64 --prefix=<dir>; make; make install_sw"
    echo "  (see the ios-compile probe in .github/workflows/ci.yml for the same recipe)"
    exit 2
fi

BUILD_DIR="$REPO_ROOT/build-android-$ABI"
mkdir -p "$BUILD_DIR"

echo "=== Caesura (AmeKAG) — Android cross-compile ==="
echo "  NDK      : $NDK"
echo "  abi      : $ABI"
echo "  platform : $ANDROID_PLATFORM"
echo "  type     : $BUILD_TYPE"
echo "  SDL3_DIR : $SDL3_DIR"
echo "  target   : ${TARGET:-<all>}  smoke=${SMOKE}"
echo "  out      : $BUILD_DIR"

# -- 1) Configure ------------------------------------------------------------
cmake -G "${CMAKE_GENERATOR:-Ninja}" -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCAESURA_LIVE2D=OFF \
    -DCAESURA_ENABLE_FFMPEG=OFF \
    -DSDL3_DIR="$SDL3_DIR" \
    -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" \
    -DSOLOUD_BACKEND_OPENSLES=ON 2>&1 | tee "$BUILD_DIR/configure.log"

# -- 2) Build ----------------------------------------------------------------
EXTRA=()
if [[ -n "$TARGET" ]]; then
    EXTRA+=(--target "$TARGET")
fi
if [[ "$SMOKE" == "1" || -z "$TARGET" ]]; then
    # --smoke (or default): build the module-lib graph only — ensures all
    # sources compile under NDK clang without producing an executable/APK.
    EXTRA+=(--target CaesuraAmeKAG)   # module graph + libCaesuraAmeKAG.so (R1 MODULE)
fi

echo "=== Building ($BUILD_TYPE) ==="
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "${EXTRA[@]}" 2>&1 \
    | tee "$BUILD_DIR/build.log"

echo ""
echo "=== Build complete ($BUILD_DIR) ==="
echo "  Configure log: $BUILD_DIR/configure.log"
echo "  Build log    : $BUILD_DIR/build.log"
echo ""
echo "NOTE: This compiles the engine module graph for Android."
echo "  - A JNI/Activity shell is NOT included in this repo; APK assembly needs an"
echo "    external app module (see docs/guides/android-build.md 3.4, R8)."
echo "  - A shared android library (libCaesuraAmeKAG.so) requires reworking the"
echo "    top-level executable target into a MODULE/SHARED target (R1)."
echo "  - Render backend on device: launch with --backend opengl (R2)."
echo "  - Real-device verification (touch/IME/audio/lifecycle/DPI): pending."