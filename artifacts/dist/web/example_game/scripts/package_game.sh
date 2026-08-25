#!/usr/bin/env bash
# ==============================================================================
#  Caesura (AmeKAG) — package_game.sh
#
#  One-click packaging: turn a KAG .ks game into a distributable web bundle.
#  Content-author focus:  bash scripts/package_game.sh demo/example_game
#  produces dist/<game>/ — a self-contained static site the web player serves
#  (works on any static host: GitHub Pages / itch.io / Netlify / S3).
#
#  Pipeline (fail-fast via set -e)
#    1. resolve input    — a demo dir (all its .ks) or explicit .ks paths
#    2. ks_check         — static contract gate (zero errors required)
#    3. ks_bake --web    — bake the game scenes into cache/story/story.lua
#    4. assemble         — copy the built web player + runtime dirs + assets
#    5. manifest         — MANIFEST.txt (file tree + sizes)
#    6. (--release)      — print the CPack desktop-Release handoff (see
#                          docs/guides/release-process.md); not executed here.
#
#  Usage (from repo root; git bash)
#    bash scripts/package_game.sh                       # default demo/example_game
#    bash scripts/package_game.sh demo/example_game      # a whole game dir
#    bash scripts/package_game.sh path/to/game.ks        # a single scene
#    bash scripts/package_game.sh demo/tutorial          # or any dir of .ks
#    bash scripts/package_game.sh --release demo/example_game
#    bash scripts/package_game.sh --no-web-build demo/example_game
#
#  Options
#    --out <dir>       package destination (default dist/<game-name>)
#    --assets <dir>    asset root to ship (default: repo assets/ shared pool)
#    --no-web-build    reuse an existing web/dist; do not (re)build it
#    --release         also print the CPack desktop-Release handoff (docs only)
#    --entry <scene>   nominate the entry scene (recorded in the manifest)
#
#  Exit: 0 = packaged, 1 = any step failed.
# ==============================================================================

set -euo pipefail

readonly HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly ROOT="$(cd "$HERE/.." && pwd)"
cd "$ROOT"

# Lua interpreter: the vendored lua.exe is Windows-only — Linux/macOS CI
# must use the platform interpreter (uname MINGW*/MSYS* = git-bash).
case "$(uname -s 2>/dev/null || true)" in
    MINGW*|MSYS*|CYGWIN*)
        LUA="external/lua/lua.exe"
        [ -f "$LUA" ] || LUA="" ;;
    *) LUA="" ;;
esac
if [ -z "$LUA" ]; then
    LUA="$(command -v lua5.4 || command -v lua || true)"
fi
if [ -z "$LUA" ] || [ ! -e "$LUA" ]; then
    echo "[package] FATAL: no Lua interpreter (vendored lua.exe unavailable on this OS)."; exit 1
fi

# ------------------------------------------------------------------ options --
OUT=""
ASSET_SRC="assets"
NO_WEB_BUILD=0
RELEASE=0
ENTRY=""
DEFAULT_INPUT="demo/example_game"

while [ $# -gt 0 ]; do
    case "$1" in
        --out)           OUT="$2";        shift 2;;
        --assets)        ASSET_SRC="$2";  shift 2;;
        --no-web-build)  NO_WEB_BUILD=1;  shift;;
        --release)       RELEASE=1;       shift;;
        --entry)         ENTRY="$2";      shift 2;;
        -h|--help)       sed -n "1,64p" "$BASH_SOURCE"; exit 0;;
        *)               break;;
    esac
done

# --------------------------------------------------- 1. resolve input ------
POSITIONAL=("$@")
if [ "${#POSITIONAL[@]}" -eq 0 ]; then POSITIONAL=("$DEFAULT_INPUT"); fi

KAGS=()
for p in "${POSITIONAL[@]}"; do
    if [ -d "$p" ]; then
        while IFS= read -r f; do KAGS+=("$f"); done < <(find "$p" -name "*.ks" | sort)
    elif [ -f "$p" ] && [[ "$p" == *.ks ]]; then
        KAGS+=("$p")
    else
        echo "[package] FATAL: not a game dir or .ks file: $p"; exit 1
    fi
done
if [ "${#KAGS[@]}" -eq 0 ]; then
    echo "[package] FATAL: no .ks scenes found in input."; exit 1
fi
echo "[package] input: ${#KAGS[@]} scene(s) -> $(printf "%s " "${KAGS[@]}")"

FIRST="${KAGS[0]}"
GAME_NAME="$(basename "$(dirname "$FIRST")")"
if [ "$GAME_NAME" = "." ] || [ -z "$GAME_NAME" ]; then GAME_NAME="$(basename "$FIRST" .ks)"; fi
if [ -z "$OUT" ]; then OUT="dist/$GAME_NAME"; fi

# -------------------------------------------------- 2. ks_check (gate) ------
echo ""
echo "[package] Step 1/5: ks_check (contract gate)"
for k in "${KAGS[@]}"; do
    if ! "$LUA" scripts/ks_check.lua "$k"; then
        echo "[package] FAIL: contract check failed for $k"; exit 1
    fi
done
echo "[package] ks_check: all scenes pass contracts"

# --------------------------------------------------- 3. ks_bake --web ------
echo ""
echo "[package] Step 2/5: ks_bake --web (story bundle)"
STAGE="$(mktemp -d)"
trap "rm -rf \"$STAGE\"" EXIT
if [ -n "$ENTRY" ]; then
    ES=""
    for k in "${KAGS[@]}"; do
        if [ "$(basename "$k")" = "$ENTRY" ] || [ "$k" = "$ENTRY" ]; then ES="$k"; break; fi
    done
    if [ -z "$ES" ]; then
        ES="$ENTRY"
        if [ ! -f "$ES" ]; then echo "[package] FAIL: --entry scene not found: $ENTRY"; exit 1; fi
        KAGS=("$ES" "${KAGS[@]}")
    fi
fi
if ! "$LUA" scripts/ks_bake.lua "${KAGS[@]}" --web "$STAGE"; then
    echo "[package] FAIL: ks_bake web bundle failed"; exit 1
fi
BUNDLE="$STAGE/story.lua"

# ---------------------------------------------------- 4. assemble -----------
echo ""
echo "[package] Step 3/5: assemble web player + runtime"
WEB_DIST="web/dist"
if [ "$NO_WEB_BUILD" -eq 0 ]; then
    if [ -d web/node_modules/vite ] || [ -d node_modules/vite ]; then
        echo "[package]   (re)building web player -> $WEB_DIST"
        ( cd web && node_modules/.bin/vite build >/dev/null 2>&1 )
    else
        echo "[package]   node_modules missing — reusing existing $WEB_DIST"
        echo "[package]   (to rebuild: cd web && npm install && node_modules/.bin/vite build)"
    fi
fi
if [ ! -f "$WEB_DIST/index.html" ]; then
    echo ""
    echo "[package] FAIL: web player not built (missing $WEB_DIST/index.html)."
    echo "[package]   Build it once with:  (cd web && npm install && node_modules/.bin/vite build)"
    exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT/cache/story" "$OUT/demo/$GAME_NAME" "$OUT/web-assets" "$OUT/scripts" "$OUT/$ASSET_SRC"

cp "$WEB_DIST/index.html"        "$OUT/index.html"
[ -f "$WEB_DIST/sw.js" ] && cp "$WEB_DIST/sw.js" "$OUT/sw.js" || ([ -f "web/sw.js" ] && cp "web/sw.js" "$OUT/sw.js" || true)
[ -f "$WEB_DIST/manifest.webmanifest" ] && cp "$WEB_DIST/manifest.webmanifest" "$OUT/manifest.webmanifest" || ([ -f "web/manifest.webmanifest" ] && cp "web/manifest.webmanifest" "$OUT/manifest.webmanifest" || true)
cp "$WEB_DIST"/web-assets/*      "$OUT/web-assets/"
cp -r "$WEB_DIST"/scripts/.      "$OUT/scripts/"

# The web player bridge.js fetches scriptsBase + index.json -- regenerate it
# for the packaged script tree so a packaged game boots without manual
# bundle edits (Validation-Release task book §9).
if [ -f "$ROOT/web/gen-index.mjs" ] && command -v node >/dev/null 2>&1; then
    node "$ROOT/web/gen-index.mjs" "$OUT/scripts" "$OUT/scripts/index.json" >/dev/null 2>&1 \
        || echo "[package] WARN: scripts index.json generation failed"
fi

# prune dev-only artifacts from the packaged script tree
find "$OUT/scripts" -type d -name "__pycache__" -prune -exec rm -rf {} + 2>/dev/null || true
find "$OUT/scripts" -type f -name "*.pyc" -delete 2>/dev/null || true

if [ -d "$ASSET_SRC" ]; then
    cp -r "$ASSET_SRC/." "$OUT/$ASSET_SRC/"
else
    echo "[package] WARN: asset root [$ASSET_SRC] not found — shipping without game assets"
fi

for k in "${KAGS[@]}"; do
    cp "$k" "$OUT/demo/$GAME_NAME/$(basename "$k")"
done

cp "$BUNDLE" "$OUT/cache/story/story.lua"

# ---------------------------------------------------- 5. manifest -----------
echo ""
echo "[package] Step 4/5: manifest"
{
    echo "Caesura (AmeKAG) web package: $GAME_NAME"
    echo "built: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "scenes: ${#KAGS[@]}"
    if [ -n "$ENTRY" ]; then echo "entry scene: $ENTRY"; fi
    echo "---"
    echo "files (size bytes, path):"
    find "$OUT" -type f -printf "%s\t%P\n" | sort -k2
    echo "---"
    echo "total KB: $(du -sk "$OUT" | cut -f1)"
} > "$OUT/MANIFEST.txt"

echo ""
echo "=================================================================="
echo "  PACKAGE COMPLETE -> $OUT"
echo "    scenes:  ${#KAGS[@]}  (pick one from the web scene dropdown)"
echo "    bundle:  $OUT/cache/story/story.lua"
echo "    assets:  $OUT/$ASSET_SRC/"
if [ -n "$ENTRY" ]; then echo "    entry:   $ENTRY"; fi
echo "    manifest: $OUT/MANIFEST.txt"
echo "------------------------------------------------------------------"
echo "  Serve locally:  cd [$OUT] && python -m http.server 8080"
echo "  Or upload to itch.io / Netlify / GitHub Pages / S3."
echo "=================================================================="

# --------------------------------------------------- 6. --release -----------
if [ "$RELEASE" -eq 1 ]; then
    echo ""
    echo "[package] --release: desktop CPack handoff (see docs/guides/release-process.md)"
    echo "    cmake --build build --config Release --parallel"
    echo "    cd build && cpack -C Release -G ZIP && cd .."
    echo "    git tag -a vX.Y.Z -m [Caesura (AmeKAG) vX.Y.Z] && git push origin vX.Y.Z"
    echo "    gh release create vX.Y.Z build/CaesuraAmeKAG-*-Windows-AMD64.zip --title [TITLE] --notes-file CHANGELOG.md --draft"
fi

exit 0