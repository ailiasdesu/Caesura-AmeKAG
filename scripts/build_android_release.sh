#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — Android Release Packaging & Verification Script (Track A5)
#
#  Automates the complete release build pipeline:
#    1. (Optional) Ephemeral test key generation or env-driven signing configuration
#    2. Native library staging (libCaesuraAmeKAG.so, libSDL3.so)
#    3. Game asset bundle staging (scripts/, assets/, demo/)
#    4. Release APK packaging (assembleRelease)
#    5. Release App Bundle packaging (bundleRelease)
#    6. Artifact validation (zipalign -c 4 and apksigner verify)
#
#  Usage:
#    scripts/build_android_release.sh [options]
#
#  Options:
#    --abi <abi>            Target ABI: arm64-v8a (default), armeabi-v7a, x86_64
#    --keystore <path>      Path to release keystore file
#    --storepass <pass>     Keystore password
#    --alias <alias>        Key alias
#    --keypass <pass>       Key password
#    --ephemeral-key,       Generate an ephemeral test keystore and sign for CI/testing
#    --test-key
#    --skip-build-so        Skip compiling native .so if already present
#    --skip-apk             Skip APK packaging (assembleRelease)
#    --skip-aab             Skip AAB packaging (bundleRelease)
#    --skip-verify          Skip zipalign and apksigner verification
#    -h, --help             Show this help message
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ABI="${ABI:-arm64-v8a}"
KEYSTORE_PATH=""
KEYSTORE_PASS=""
KEY_ALIAS=""
KEY_PASS=""
USE_EPHEMERAL_KEY=0
SKIP_BUILD_SO=0
SKIP_APK=0
SKIP_AAB=0
SKIP_VERIFY=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --abi)
            ABI="$2"; shift 2 ;;
        --keystore)
            KEYSTORE_PATH="$2"; shift 2 ;;
        --storepass)
            KEYSTORE_PASS="$2"; shift 2 ;;
        --alias)
            KEY_ALIAS="$2"; shift 2 ;;
        --keypass)
            KEY_PASS="$2"; shift 2 ;;
        --ephemeral-key|--test-key)
            USE_EPHEMERAL_KEY=1; shift ;;
        --skip-build-so)
            SKIP_BUILD_SO=1; shift ;;
        --skip-apk)
            SKIP_APK=1; shift ;;
        --skip-aab)
            SKIP_AAB=1; shift ;;
        --skip-verify)
            SKIP_VERIFY=1; shift ;;
        -h|--help)
            sed -n '1,27p' "$0"; exit 0 ;;
        *)
            echo "ERROR: Unknown option: $1" >&2; exit 2 ;;
    esac
done

echo "==================================================================="
echo " Caesura (AmeKAG) — Android Release Packaging & Signing Pipeline"
echo "==================================================================="
echo "  Repo root : $REPO_ROOT"
echo "  ABI       : $ABI"

# -----------------------------------------------------------------------------
# 0. Auto-discover JDK and Android SDK
# -----------------------------------------------------------------------------
if [[ -z "${JAVA_HOME:-}" || ! -d "${JAVA_HOME:-}" ]]; then
    for cand in "/d/green/android-tools/jdk-17.0.20+8" "D:/green/android-tools/jdk-17.0.20+8" "/d/green/PCL/jdk-17.0.6" "D:/green/PCL/jdk-17.0.6" "C:/Program Files/Java/jdk-17"* "C:/Program Files/Eclipse Adoptium/jdk-17"*; do
        if [[ -d "$cand" && (-x "$cand/bin/keytool" || -f "$cand/bin/keytool.exe" || -f "$cand/bin/keytool") ]]; then
            export JAVA_HOME="$cand"
            export PATH="$cand/bin:$PATH"
            break
        fi
    done
fi

if [[ -z "${ANDROID_HOME:-}" && -z "${ANDROID_SDK_ROOT:-}" ]]; then
    for cand in "/d/green" "D:/green" "$HOME/AppData/Local/Android/Sdk" "/c/Android/sdk" "C:/Android/sdk"; do
        if [[ -d "$cand/build-tools" || -d "$cand/platforms" ]]; then
            export ANDROID_HOME="$cand"
            export ANDROID_SDK_ROOT="$cand"
            break
        fi
    done
fi

# Ensure git usr/bin is in PATH for standard unix utilities if on Windows
if [[ -d "/c/Program Files/Git/usr/bin" ]]; then
    export PATH="/c/Program Files/Git/usr/bin:$PATH"
fi

# -----------------------------------------------------------------------------
# 1. Setup Signing Configuration
# -----------------------------------------------------------------------------
if [[ "$USE_EPHEMERAL_KEY" -eq 1 ]]; then
    EPHEMERAL_KS="$REPO_ROOT/android/app/build/tmp/ephemeral-test.keystore"
    mkdir -p "$(dirname "$EPHEMERAL_KS")"
    echo "--- Generating ephemeral test keystore for verification ---"
    bash "$SCRIPT_DIR/generate_android_keystore.sh" --test --keystore "$EPHEMERAL_KS"
    export CAESURA_ANDROID_KEYSTORE="$EPHEMERAL_KS"
    export CAESURA_ANDROID_KEYSTORE_PASS="caesura_test_pass"
    export CAESURA_ANDROID_KEY_ALIAS="caesura-test"
    export CAESURA_ANDROID_KEY_PASS="caesura_test_pass"
elif [[ -n "$KEYSTORE_PATH" ]]; then
    export CAESURA_ANDROID_KEYSTORE="$KEYSTORE_PATH"
    export CAESURA_ANDROID_KEYSTORE_PASS="${KEYSTORE_PASS:-}"
    export CAESURA_ANDROID_KEY_ALIAS="${KEY_ALIAS:-}"
    export CAESURA_ANDROID_KEY_PASS="${KEY_PASS:-$KEYSTORE_PASS}"
fi

ACTIVE_KEYSTORE="${CAESURA_ANDROID_KEYSTORE:-${CAESURA_KEYSTORE_PATH:-}}"
if [[ -n "$ACTIVE_KEYSTORE" && -f "$ACTIVE_KEYSTORE" ]]; then
    echo "  Signing   : ENABLED (Keystore: $ACTIVE_KEYSTORE)"
else
    echo "  Signing   : OPT-IN / UNSIGNED (No valid keystore configured)"
fi

# -----------------------------------------------------------------------------
# 2. Stage Native Libraries & Game Assets
# -----------------------------------------------------------------------------
JNILIBS_DIR="$REPO_ROOT/android/app/src/main/jniLibs/$ABI"
mkdir -p "$JNILIBS_DIR"

SO_FILE="$REPO_ROOT/build-android-$ABI/libCaesuraAmeKAG.so"
if [[ "$SKIP_BUILD_SO" -eq 0 && ! -f "$JNILIBS_DIR/libCaesuraAmeKAG.so" && -f "$SO_FILE" ]]; then
    echo "--- Staging prebuilt libCaesuraAmeKAG.so ---"
    cp "$SO_FILE" "$JNILIBS_DIR/"
fi

ASSETS_GAME_DIR="$REPO_ROOT/android/app/src/main/assets/game"
if [[ -d "$REPO_ROOT/scripts" && -d "$REPO_ROOT/assets" ]]; then
    echo "--- Staging game assets & scripts ---"
    rm -rf "$ASSETS_GAME_DIR"
    mkdir -p "$ASSETS_GAME_DIR/demo"
    cp -r "$REPO_ROOT/scripts" "$ASSETS_GAME_DIR/scripts"
    cp -r "$REPO_ROOT/assets" "$ASSETS_GAME_DIR/assets"
    if [[ -d "$REPO_ROOT/tests/projects/first_vn" ]]; then
        cp -r "$REPO_ROOT/tests/projects/first_vn" "$ASSETS_GAME_DIR/demo/first_vn"
    fi
fi

# -----------------------------------------------------------------------------
# 3. Locate or Prepare Gradle
# -----------------------------------------------------------------------------
GRADLE="gradle"
if [[ -x "$REPO_ROOT/android/gradlew" ]]; then
    GRADLE="$REPO_ROOT/android/gradlew"
elif [[ -f "$REPO_ROOT/android/gradlew.bat" && ("$OSTYPE" == "msys" || "$OSTYPE" == "cygwin") ]]; then
    GRADLE="$REPO_ROOT/android/gradlew.bat"
elif command -v gradle >/dev/null 2>&1; then
    GRADLE="gradle"
elif [[ -x "/d/green/android-tools/gradle-8.9/bin/gradle" ]]; then
    GRADLE="/d/green/android-tools/gradle-8.9/bin/gradle"
elif [[ -f "/d/green/android-tools/gradle-8.9/bin/gradle.bat" ]]; then
    GRADLE="/d/green/android-tools/gradle-8.9/bin/gradle.bat"
elif [[ -f "D:/green/android-tools/gradle-8.9/bin/gradle.bat" ]]; then
    GRADLE="D:/green/android-tools/gradle-8.9/bin/gradle.bat"
else
    echo "WARNING: System gradle or gradlew not found. Attempting to download Gradle 8.9..."
    TEMP_GRADLE_ZIP="/tmp/gradle-8.9-bin.zip"
    TEMP_GRADLE_DIR="/tmp/gradle-8.9-dist"
    mkdir -p "$TEMP_GRADLE_DIR"
    curl -sSL https://services.gradle.org/distributions/gradle-8.9-bin.zip -o "$TEMP_GRADLE_ZIP"
    unzip -qo "$TEMP_GRADLE_ZIP" -d "$TEMP_GRADLE_DIR"
    GRADLE="$TEMP_GRADLE_DIR/gradle-8.9/bin/gradle"
fi

# -----------------------------------------------------------------------------
# 4. Build Release APK (assembleRelease)
# -----------------------------------------------------------------------------
if [[ "$SKIP_APK" -eq 0 ]]; then
    echo ""
    echo "==================================================================="
    echo " Building Release APK (assembleRelease)..."
    echo "==================================================================="
    (cd "$REPO_ROOT/android" && "$GRADLE" --no-daemon --console=plain assembleRelease -x lintVitalAnalyzeRelease -x lintVitalReportRelease -x lintVitalRelease)
fi

# -----------------------------------------------------------------------------
# 5. Build Release AAB (bundleRelease)
# -----------------------------------------------------------------------------
if [[ "$SKIP_AAB" -eq 0 ]]; then
    echo ""
    echo "==================================================================="
    echo " Building Release App Bundle (bundleRelease)..."
    echo "==================================================================="
    (cd "$REPO_ROOT/android" && "$GRADLE" --no-daemon --console=plain bundleRelease -x lintVitalAnalyzeRelease -x lintVitalReportRelease -x lintVitalRelease)
fi

# -----------------------------------------------------------------------------
# 6. Artifact Verification (zipalign & apksigner)
# -----------------------------------------------------------------------------
if [[ "$SKIP_VERIFY" -eq 0 ]]; then
    echo ""
    echo "==================================================================="
    echo " Verifying Release Artifacts..."
    echo "==================================================================="

    # Find build-tools binaries (zipalign, apksigner)
    ZIPALIGN_BIN=""
    APKSIGNER_BIN=""

    SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
    if [[ -n "$SDK_ROOT" && -d "$SDK_ROOT/build-tools" ]]; then
        for bt in $(ls -d "$SDK_ROOT/build-tools/"* 2>/dev/null | sort -V -r); do
            if [[ -z "$ZIPALIGN_BIN" ]]; then
                if [[ -x "$bt/zipalign" ]]; then ZIPALIGN_BIN="$bt/zipalign"
                elif [[ -f "$bt/zipalign.exe" ]]; then ZIPALIGN_BIN="$bt/zipalign.exe"
                elif [[ -f "$bt/zipalign" ]]; then ZIPALIGN_BIN="$bt/zipalign"
                fi
            fi
            if [[ -z "$APKSIGNER_BIN" ]]; then
                if [[ -x "$bt/apksigner" ]]; then APKSIGNER_BIN="$bt/apksigner"
                elif [[ -f "$bt/apksigner.bat" ]]; then APKSIGNER_BIN="$bt/apksigner.bat"
                elif [[ -f "$bt/apksigner" ]]; then APKSIGNER_BIN="$bt/apksigner"
                fi
            fi
            [[ -n "$ZIPALIGN_BIN" && -n "$APKSIGNER_BIN" ]] && break
        done
    fi

    if [[ -z "$ZIPALIGN_BIN" ]] && command -v zipalign >/dev/null 2>&1; then
        ZIPALIGN_BIN="zipalign"
    fi
    if [[ -z "$APKSIGNER_BIN" ]] && command -v apksigner >/dev/null 2>&1; then
        APKSIGNER_BIN="apksigner"
    fi

    # 6.1 APK Verification
    APK_OUT="$REPO_ROOT/android/app/build/outputs/apk/release/app-release.apk"
    if [[ ! -f "$APK_OUT" ]]; then
        APK_OUT="$REPO_ROOT/android/app/build/outputs/apk/release/app-release-unsigned.apk"
    fi

    if [[ -f "$APK_OUT" ]]; then
        echo "Found Release APK: $APK_OUT"
        if [[ -n "$ZIPALIGN_BIN" ]]; then
            echo "-> Running 4-byte zipalign check..."
            if "$ZIPALIGN_BIN" -c -v 4 "$APK_OUT" >/dev/null 2>&1; then
                echo "   [PASS] APK alignment verification succeeded (4-byte boundary)."
            else
                echo "   [FAIL] APK is not 4-byte aligned!" >&2
                exit 1
            fi
        else
            echo "   [SKIP] zipalign not found in SDK or PATH."
        fi

        if [[ -n "$APKSIGNER_BIN" ]]; then
            echo "-> Running apksigner signature verification..."
            if "$APKSIGNER_BIN" verify --verbose --print-certs "$APK_OUT" 2>&1; then
                echo "   [PASS] APK signature verification succeeded."
            else
                if [[ -z "$ACTIVE_KEYSTORE" ]]; then
                    echo "   [INFO] Unsigned APK as expected (no signing key configured)."
                else
                    echo "   [FAIL] APK signature verification failed!" >&2
                    exit 1
                fi
            fi
        else
            echo "   [SKIP] apksigner not found in SDK or PATH."
        fi
    fi

    # 6.2 AAB Verification
    AAB_OUT="$REPO_ROOT/android/app/build/outputs/bundle/release/app-release.aab"
    if [[ ! -f "$AAB_OUT" ]]; then
        AAB_OUT="$REPO_ROOT/android/app/build/outputs/bundle/release/app-release-unsigned.aab"
    fi

    if [[ -f "$AAB_OUT" ]] && command -v unzip >/dev/null 2>&1; then
        echo "Found Release AAB: $AAB_OUT"
        echo "-> Checking AAB archive structure..."
        AAB_ENTRIES=$(unzip -l "$AAB_OUT")
        if echo "$AAB_ENTRIES" | grep -q "base/manifest/AndroidManifest.xml"; then
            echo "   [PASS] AAB manifest structure valid."
        else
            echo "   [WARN] AndroidManifest.xml missing from AAB base module."
        fi
    fi
fi

echo ""
echo "==================================================================="
echo " Release Build Pipeline Completed Successfully!"
echo "==================================================================="
