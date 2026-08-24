#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — packaged-layout boot smoke (Track M A3/R6 contract)
#
#  Verifies the canonical engine resource root so the mobile bundle design
#  (APK assets/game/** extracted by MainActivity -> --resource-root) stays
#  honest: assemble <root>/{scripts,assets,<game>}, point config.entry_script
#  at the project entry, then boot the REAL engine with --resource-root
#  <bundle> and assert the runner reached its ready marker.
#
#  Usage:
#    scripts/verify_bundle_boot.sh [engine-binary] [game]
#      engine : default $CAESURA_ENGINE or build/Debug/CaesuraAmeKAG.exe
#      game   : first_vn (default) | demo
#        first_vn -> <root>/demo/first_vn (sed config)  marker: "[FirstVN] Ready."
#        demo     -> <root>/demo (repo demo/, default config)  marker:
#                   "[Demo Entry] KAG+Lua hybrid scripting active."
#
#  Passes when the marker appears with no FATAL; windowed (GPU) mode — use
#  xvfb-run on headless CI.
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ENGINE="${CAESURA_ENGINE:-$REPO_ROOT/build/Debug/CaesuraAmeKAG.exe}"
if [[ $# -ge 1 ]]; then ENGINE="$1"; fi
GAME="${2:-first_vn}"
if [[ ! -f "$ENGINE" ]]; then
    echo "ERROR: engine binary not found: $ENGINE"
    echo "  Pass it as argv[1], or set CAESURA_ENGINE."
    exit 2
fi
# Absolute path: the launch happens after cd into the bundle (relative
# engine paths would silently point inside the bundle dir on CI).
ENGINE="$(cd "$(dirname "$ENGINE")" && pwd)/$(basename "$ENGINE")"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
BUNDLE="$WORK/bundle"

echo "=== [bundle-boot] assembling the canonical resource root ($GAME) ==="
# Same assembly as the android-compile probe (A3): <root>/{scripts,assets,<game>}
# and config.entry_script pointing at the project entry. package_game.sh is
# NOT used: its web pipeline needs web/dist which CI runners only build in
# the web job, and the native bundle needs no ks_bake.
mkdir -p "$BUNDLE/demo"
cp -r "$REPO_ROOT/scripts" "$BUNDLE/scripts"
cp -r "$REPO_ROOT/assets"  "$BUNDLE/assets"

MARKER=""
case "$GAME" in
    first_vn)
        mkdir -p "$BUNDLE/demo/first_vn"
        cp -r "$REPO_ROOT/tests/projects/first_vn/." "$BUNDLE/demo/first_vn/"
        sed -i 's|config.entry_script = .*|config.entry_script = "../demo/first_vn/entry.lua"|' "$BUNDLE/scripts/config.lua"
        MARKER='[FirstVN] Ready.'
        ;;
    demo)
        cp -r "$REPO_ROOT/demo/." "$BUNDLE/demo/"
        # repo config already points at ../demo/entry.lua (default)
        MARKER='[Demo Entry] KAG+Lua hybrid scripting active.'
        ;;
    *)
        echo "ERROR: unknown game: $GAME (supported: first_vn, demo)"
        exit 2
        ;;
esac

echo "=== [bundle-boot] launching engine: $ENGINE"
cd "$BUNDLE"
"$ENGINE" --resource-root . --backend opengl --frames 150 > "$WORK/boot.log" 2>&1 || {
    echo "ERROR: engine exited non-zero"; tail -20 "$WORK/boot.log"; exit 1; }

echo "=== [bundle-boot] asserting marker: $MARKER"
if grep -qF "$MARKER" "$WORK/boot.log"; then
    grep -E "Working directory|Demo Entry|FirstVN" "$WORK/boot.log" || true
    echo "RESULT: PASS ($GAME bundle layout boots end-to-end)"
    exit 0
fi
echo "RESULT: FAIL — marker not found; log tail:"
grep -iE "FATAL|error|FirstVN|Demo Entry" "$WORK/boot.log" | head -20 || true
tail -10 "$WORK/boot.log"
exit 1