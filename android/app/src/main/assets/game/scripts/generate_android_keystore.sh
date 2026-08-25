#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — Android Release Keystore Generator (Track A5)
#
#  Generates a standard PKCS12 release keystore using JDK keytool.
#  Supports interactive creation, custom parameter flags, and --test mode
#  for headless CI / automated pipeline verification.
#
#  Usage:
#    scripts/generate_android_keystore.sh [options]
#
#  Options:
#    --keystore <path>    Output keystore path (default: caesura-release.keystore)
#    --alias <alias>      Key alias (default: caesura)
#    --storepass <pass>   Keystore password
#    --keypass <pass>     Key password (default: same as storepass)
#    --dname <dname>      Distinguished name (default: "CN=Caesura Game, OU=Release, O=CaesuraEngine, C=JP")
#    --validity <days>    Validity period in days (default: 10000)
#    --keysize <bits>     RSA key size (default: 2048)
#    --test               Headless / CI test mode (uses ephemeral credentials without prompting)
#    -h, --help           Show this help message
# =============================================================================
set -euo pipefail

KEYSTORE="caesura-release.keystore"
ALIAS="caesura"
STOREPASS=""
KEYPASS=""
DNAME="CN=Caesura Game, OU=Release, O=CaesuraEngine, C=JP"
VALIDITY=10000
KEYSIZE=2048
IS_TEST=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --keystore)
            KEYSTORE="$2"; shift 2 ;;
        --alias)
            ALIAS="$2"; shift 2 ;;
        --storepass)
            STOREPASS="$2"; shift 2 ;;
        --keypass)
            KEYPASS="$2"; shift 2 ;;
        --dname)
            DNAME="$2"; shift 2 ;;
        --validity)
            VALIDITY="$2"; shift 2 ;;
        --keysize)
            KEYSIZE="$2"; shift 2 ;;
        --test)
            IS_TEST=1; shift ;;
        -h|--help)
            sed -n '1,22p' "$0"; exit 0 ;;
        *)
            echo "ERROR: Unknown option: $1" >&2; exit 2 ;;
    esac
done

# Locate keytool
KEYTOOL_CMD="keytool"
if ! command -v "$KEYTOOL_CMD" >/dev/null 2>&1; then
    if [[ -n "${JAVA_HOME:-}" && -x "$JAVA_HOME/bin/keytool" ]]; then
        KEYTOOL_CMD="$JAVA_HOME/bin/keytool"
    else
        echo "ERROR: 'keytool' not found in PATH or JAVA_HOME." >&2
        echo "  Please install JDK 17+ or ensure keytool is in your PATH." >&2
        exit 1
    fi
fi

# Configure defaults for test mode or interactive prompt
if [[ "$IS_TEST" -eq 1 ]]; then
    if [[ "$KEYSTORE" == "caesura-release.keystore" ]]; then
        KEYSTORE="caesura-test.keystore"
    fi
    ALIAS="${ALIAS:-caesura-test}"
    STOREPASS="${STOREPASS:-caesura_test_pass}"
    KEYPASS="${KEYPASS:-$STOREPASS}"
    DNAME="CN=Caesura CI Test, OU=Engineering, O=Caesura, C=JP"
    echo "=== Generating Ephemeral Test Keystore (--test mode) ==="
else
    echo "=== Generating Android PKCS12 Release Keystore ==="
    if [[ -z "$STOREPASS" ]]; then
        read -s -p "Enter keystore password (min 6 characters): " STOREPASS
        echo ""
        if [[ -z "$KEYPASS" ]]; then
            read -s -p "Enter key password (press ENTER to match keystore password): " KEYPASS_INPUT
            echo ""
            KEYPASS="${KEYPASS_INPUT:-$STOREPASS}"
        fi
    fi
fi

KEYPASS="${KEYPASS:-$STOREPASS}"

if [[ ${#STOREPASS} -lt 6 ]]; then
    echo "ERROR: Keystore password must be at least 6 characters." >&2
    exit 1
fi

# Remove existing keystore if regenerating in test mode
if [[ "$IS_TEST" -eq 1 && -f "$KEYSTORE" ]]; then
    rm -f "$KEYSTORE"
fi

if [[ -f "$KEYSTORE" && "$IS_TEST" -eq 0 ]]; then
    echo "WARNING: Keystore '$KEYSTORE' already exists. Overwrite? (y/N)"
    read -r response
    if [[ "$response" != "y" && "$response" != "Y" ]]; then
        echo "Aborted."
        exit 0
    fi
    rm -f "$KEYSTORE"
fi

# Ensure parent directory exists
mkdir -p "$(dirname "$KEYSTORE")"

echo "Running keytool..."
"$KEYTOOL_CMD" -genkeypair -v \
    -keystore "$KEYSTORE" \
    -alias "$ALIAS" \
    -keyalg RSA \
    -keysize "$KEYSIZE" \
    -validity "$VALIDITY" \
    -storetype PKCS12 \
    -storepass "$STOREPASS" \
    -keypass "$KEYPASS" \
    -dname "$DNAME"

echo ""
echo "=== Keystore successfully created ==="
echo "  Path     : $KEYSTORE"
echo "  Alias    : $ALIAS"
echo "  Type     : PKCS12"
echo "  Validity : $VALIDITY days"
echo "  Key Size : $KEYSIZE bits RSA"
echo ""
echo "To use this keystore for release builds, export environment variables:"
echo ""
echo "  # Convention A (CAESURA_ANDROID_*):"
echo "  export CAESURA_ANDROID_KEYSTORE=\"$(cd "$(dirname "$KEYSTORE")" && pwd)/$(basename "$KEYSTORE")\""
echo "  export CAESURA_ANDROID_KEYSTORE_PASS=\"$STOREPASS\""
echo "  export CAESURA_ANDROID_KEY_ALIAS=\"$ALIAS\""
echo "  export CAESURA_ANDROID_KEY_PASS=\"$KEYPASS\""
echo ""
echo "  # Convention B (CAESURA_KEYSTORE_*):"
echo "  export CAESURA_KEYSTORE_PATH=\"$(cd "$(dirname "$KEYSTORE")" && pwd)/$(basename "$KEYSTORE")\""
echo "  export CAESURA_KEYSTORE_PASSWORD=\"$STOREPASS\""
echo "  export CAESURA_KEY_ALIAS=\"$ALIAS\""
echo "  export CAESURA_KEY_PASSWORD=\"$KEYPASS\""
echo ""
echo "NOTE: NEVER commit your release keystore or credentials to Git!"
