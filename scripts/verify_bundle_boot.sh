#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — packaged-layout boot smoke (Track M A3/R6 contract)
#
#  Verifies the canonical engine resource root so the mobile bundle design
#  (APK assets/game/** extracted by MainActivity -> --resource-root) stays
#  honest: package first_vn, assemble <root>/{scripts,assets,demo/first_vn},
#  point config.entry_script at the project entry, then boot the REAL engine
#  with --resource-root <bundle> and assert the KAG runner reached "Ready".
#
#  Usage:
#    scripts/verify_bundle_boot.sh [engine-binary]      (default: build/Debug/CaesuraAmeKAG.exe)
#    CAESURA_ENGINE=/path/to/engine scripts/verify_bundle_boot.sh   (cross-platform)
#
#  Passes when the engine logs "[FirstVN] Ready." with no FATAL; windowed
#  (GPU) mode — use xvfb-run on headless CI.
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ENGINE="${CAESURA_ENGINE:-$REPO_ROOT/build/Debug/CaesuraAmeKAG.exe}"
if [[ $# -ge 1 ]]; then ENGINE="$1"; fi
if [[ ! -f "$ENGINE" ]]; then
    echo "ERROR: engine binary not found: $ENGINE"
    echo "  Pass it as argv[1], or set CAESURA_ENGINE."
    exit 2
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
BUNDLE="$WORK/bundle"

echo "=== [bundle-boot] packaging first_vn ==="
bash "$SCRIPT_DIR/package_game.sh" --no-web-build --out "$BUNDLE" "$REPO_ROOT/tests/projects/first_vn" >/dev/null 2>&1 \
    || { echo "ERROR: package_game.sh failed"; exit 1; }

# Mirror the Android staging: the web packaging ships only the baked .ks; the
# native/mobile bundle needs the project entry + its media assets too.
mkdir -p "$BUNDLE/demo/first_vn"
cp "$REPO_ROOT/tests/projects/first_vn/entry.lua" "$BUNDLE/demo/first_vn/"
cp -r "$REPO_ROOT/tests/projects/first_vn/assets" "$BUNDLE/demo/first_vn/"

# Point the runtime at the project entry (same rewrite as the CI probe).
sed -i 's|config.entry_script = .*|config.entry_script = "../demo/first_vn/entry.lua"|' "$BUNDLE/scripts/config.lua"

echo "=== [bundle-boot] launching engine: $ENGINE"
cd "$BUNDLE"
"$ENGINE" --resource-root . --backend opengl --frames 150 > "$WORK/boot.log" 2>&1 || {
    echo "ERROR: engine exited non-zero"; tail -20 "$WORK/boot.log"; exit 1; }

echo "=== [bundle-boot] asserting KAG runner reached Ready ==="
if grep -q "\[FirstVN\] Ready\." "$WORK/boot.log"; then
    grep -E "\[FirstVN\]|Working directory" "$WORK/boot.log"
    echo "RESULT: PASS (bundle layout boots end-to-end)"
    exit 0
fi
echo "RESULT: FAIL — [FirstVN] Ready not found; log tail:"
grep -iE "FATAL|error|FirstVN" "$WORK/boot.log" | head -20 || true
tail -10 "$WORK/boot.log"
exit 1