#!/usr/bin/env bash
# =====================================================================
# verify_web_package.sh -- assert the shape of a packaged web site
#
# Input: a dist/<game>/ directory produced by scripts/package_game.sh.
# Output: 25 deterministic file assertions (no browser, no engine), grouped:
#   1. layout      -- player, runtime dirs, bundle, scenes, manifest present
#   2. index.html  -- relative ./web-assets/ refs (subpath hosting), local
#                     wasm pin, no unpkg/CDN dependency
#   3. story bundle-- non-empty Lua literal; every packaged scene keyed;
#                     every asset the bundle references actually shipped
#   4. scripts     -- scripts/index.json byte-identical to a fresh
#                     web/gen-index.mjs run; kag/init.lua; no __pycache__
#   5. isolation   -- no .js under assets/, no media under web-assets/
#   6. manifest    -- header line and the two load-bearing entries
#
# Deliberately NOT tolerant: a missing directory or a missing file inside
# it is a FAILURE, never a silent pass. --skip-if-missing exits 77 (the
# ctest SKIP convention) only when there is no package at all.
#
# Trust: read-only, and DIST_DIR is trusted as given (the callers are CI
# steps and the contract tests). Asset paths extracted from story.lua come
# from content authors -- they are only ever expanded inside double quotes
# and tested with [ -f ], never eval'd.
#
# usage:
#   bash scripts/verify_web_package.sh [DIST_DIR] [--skip-if-missing]
#   DIST_DIR defaults to the newest dist/*/ that contains an index.html.
# exit: 0 all pass / 1 any fail / 2 nothing to verify / 77 skip
# =====================================================================
set -u

DIST=""
SKIP_IF_MISSING=0
for arg in "$@"; do
    case "$arg" in
        --skip-if-missing) SKIP_IF_MISSING=1 ;;
        -h|--help)         sed -n '2,29p' "$0"; exit 0 ;;
        -*)                printf 'unknown option: %s\n' "$arg"
                           printf 'usage: bash scripts/verify_web_package.sh [DIST_DIR] [--skip-if-missing]\n'
                           exit 2 ;;
        *)                 DIST="$arg" ;;
    esac
done

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PASS=0
FAIL=0
declare -a FAILURES=()

ok()    { PASS=$((PASS+1)); printf '  [PASS] %s\n' "$1"; }
bad()   { FAIL=$((FAIL+1)); FAILURES+=("$1"); printf '  [FAIL] %s -- %s\n' "$1" "${2:-}"; }
head_() { printf '\n== %s\n' "$1"; }

# ---------------------------------------------------------------- 0. locate
head_ "0. locate the packaged site"
if [ -z "$DIST" ]; then
    # Auto-selection is mtime-based, so SAY which directory won and what else
    # was in the running: dist/ accumulates one subdir per packaged game.
    CANDS="$(ls -td "$REPO_ROOT"/dist/*/ 2>/dev/null | while IFS= read -r d; do
                 [ -f "$d/index.html" ] && printf '%s\n' "${d%/}"; done)"
    DIST="$(printf '%s\n' "$CANDS" | head -1)"
    NCAND="$(printf '%s\n' "$CANDS" | grep -c . || true)"
    if [ "$NCAND" -gt 1 ] 2>/dev/null; then
        printf '%s packaged sites under dist/, picking the newest by mtime:\n' "$NCAND"
        printf '%s\n' "$CANDS" | sed 's/^/  /'
    fi
fi
if [ -z "$DIST" ] || [ ! -d "$DIST" ]; then
    if [ -n "$DIST" ]; then
        printf 'no packaged site at the given path: %s\n' "$DIST"
    else
        printf 'no packaged site found (looked for %s/dist/*/index.html)\n' "$REPO_ROOT"
    fi
    printf 'produce one with:\n'
    printf '  bash scripts/package_game.sh tests/projects/first_vn\n'
    if [ "$SKIP_IF_MISSING" = "1" ]; then
        printf 'SKIP: no package to verify (--skip-if-missing)\n'
        exit 77
    fi
    printf 'FAIL: nothing to verify. Package a game, or pass --skip-if-missing.\n'
    exit 2
fi
DIST="${DIST%/}"
printf 'package: %s\n' "$DIST"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/caesura-webverify.XXXXXX")"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT

# --------------------------------------------------------------- 1. layout
head_ "1. layout"
[ -f "$DIST/index.html" ]           && ok "index.html present"        || bad "index.html present" "missing $DIST/index.html"
[ -d "$DIST/web-assets" ]           && ok "web-assets/ present"       || bad "web-assets/ present" "missing $DIST/web-assets"
NJS="$(find "$DIST/web-assets" -maxdepth 1 -type f -name '*.js' 2>/dev/null | wc -l | tr -d ' ')"
[ "$NJS" -ge 1 ]                    && ok "web-assets/*.js >= 1 ($NJS)" || bad "web-assets/*.js >= 1" "found $NJS"
[ -f "$DIST/web-assets/glue.wasm" ] && ok "web-assets/glue.wasm present (local Lua VM wasm)" || bad "web-assets/glue.wasm present" "missing -- player would fetch it from unpkg"
[ -d "$DIST/scripts" ]              && ok "scripts/ present"          || bad "scripts/ present" "missing $DIST/scripts"
NASSET="$(find "$DIST/assets" -type f 2>/dev/null | head -1 | wc -l | tr -d ' ')"
[ -d "$DIST/assets" ] && [ "$NASSET" -ge 1 ] && ok "assets/ present and non-empty" || bad "assets/ present and non-empty" "missing or empty $DIST/assets"
STORY="$DIST/cache/story/story.lua"
[ -f "$STORY" ]                     && ok "cache/story/story.lua present" || bad "cache/story/story.lua present" "missing $STORY"
KS=()
while IFS= read -r k; do KS+=("$k"); done < <(find "$DIST/demo" -type f -name '*.ks' 2>/dev/null | LC_ALL=C sort)
[ "${#KS[@]}" -ge 1 ]               && ok "demo/<game>/*.ks >= 1 (${#KS[@]})" || bad "demo/<game>/*.ks >= 1" "no scenes under $DIST/demo"
MANIFEST="$DIST/MANIFEST.txt"
[ -f "$MANIFEST" ]                  && ok "MANIFEST.txt present"      || bad "MANIFEST.txt present" "missing $MANIFEST"

# ----------------------------------------------------------- 2. index.html
head_ "2. index.html"
INDEX="$DIST/index.html"
if [ -f "$INDEX" ] && grep -qF './web-assets/' "$INDEX"; then
    ok "index.html references ./web-assets/ relatively (subpath hosting)"
else
    bad "index.html references ./web-assets/ relatively" "no './web-assets/' in index.html -- an absolute /web-assets/ breaks non-root mounts"
fi
if [ -f "$INDEX" ] && grep -qF '__CAESURA_WASM_FILE__' "$INDEX"; then
    ok "index.html pins the Lua VM wasm to the local copy"
else
    bad "index.html pins the Lua VM wasm to the local copy" "no __CAESURA_WASM_FILE__ in index.html"
fi
if [ -f "$INDEX" ] && grep -qi 'unpkg' "$INDEX"; then
    bad "index.html has no unpkg/CDN reference" "unpkg mentioned in index.html"
else
    ok "index.html has no unpkg/CDN reference"
fi

# ---------------------------------------------------------- 3. story bundle
head_ "3. story bundle"
if [ -s "$STORY" ]; then ok "story.lua non-empty"; else bad "story.lua non-empty" "missing or 0 bytes: $STORY"; fi
if [ -f "$STORY" ] && [ "$(head -c 8 "$STORY")" = "return {" ]; then
    ok "story.lua is a Lua literal (starts with 'return {')"
else
    bad "story.lua is a Lua literal (starts with 'return {')" "unexpected prefix"
fi
MISSING_SCENES=""
for k in "${KS[@]}"; do
    n="$(basename "$k")"
    if [ ! -f "$STORY" ] || ! grep -qF "[\"$n\"]" "$STORY"; then MISSING_SCENES="$MISSING_SCENES $n"; fi
done
if [ "${#KS[@]}" -ge 1 ] && [ -z "$MISSING_SCENES" ]; then
    ok "every packaged scene is keyed in story.lua (${#KS[@]})"
else
    bad "every packaged scene is keyed in story.lua" "not keyed:${MISSING_SCENES:- (no scenes)}"
fi
# story.lua is a single-line literal; the asset list is ["assets"]={"a","b"}
# and asset strings never contain '}' (paths), so a non-greedy [^}]* is exact.
ASSET_LIST=""
if [ -f "$STORY" ]; then
    ASSET_LIST="$(grep -oE '\["assets"\]=\{[^}]*\}' "$STORY" | head -1 | sed 's/.*={//; s/}//' | tr -d '"' | tr ',' '\n' | grep . || true)"
fi
if [ -f "$STORY" ] && grep -qE '\["assets"\]=\{' "$STORY"; then
    ok "story.lua declares an [\"assets\"] list"
else
    bad "story.lua declares an [\"assets\"] list" "no [\"assets\"]={ in story.lua"
fi
NREF="$(printf '%s\n' "$ASSET_LIST" | grep -c . || true)"
MISSING_ASSETS=""
while IFS= read -r a; do
    [ -n "$a" ] || continue
    [ -f "$DIST/$a" ] || MISSING_ASSETS="$MISSING_ASSETS $a"
done <<< "$ASSET_LIST"
if [ -z "$MISSING_ASSETS" ]; then
    ok "every asset story.lua references is shipped ($NREF assets referenced)"
else
    bad "every asset story.lua references is shipped" "missing under $DIST/:$MISSING_ASSETS"
fi

# ---------------------------------------------------------- 4. scripts tree
head_ "4. scripts tree"
INDEX_JSON="$DIST/scripts/index.json"
[ -f "$INDEX_JSON" ] && ok "scripts/index.json present" || bad "scripts/index.json present" "missing $INDEX_JSON -- the player boots via scriptsBase + index.json"
if ! command -v node >/dev/null 2>&1; then
    bad "scripts/index.json matches a fresh gen-index run" "node not on PATH -- cannot regenerate for comparison"
elif [ -f "$INDEX_JSON" ] && node "$REPO_ROOT/web/gen-index.mjs" "$DIST/scripts" "$WORK/index.json" >/dev/null 2>&1 \
     && cmp -s <(tr -d '\r' < "$INDEX_JSON") <(tr -d '\r' < "$WORK/index.json"); then
    ok "scripts/index.json matches a fresh gen-index run (byte-identical)"
else
    bad "scripts/index.json matches a fresh gen-index run" "stale or unreadable: $INDEX_JSON"
fi
[ -f "$DIST/scripts/kag/init.lua" ] && ok "scripts/kag/init.lua present" || bad "scripts/kag/init.lua present" "missing"
NPYC="$(find "$DIST/scripts" \( -name __pycache__ -o -name '*.pyc' \) 2>/dev/null | wc -l | tr -d ' ')"
[ "$NPYC" -eq 0 ] && ok "no __pycache__ / *.pyc in scripts/" || bad "no __pycache__ / *.pyc in scripts/" "found $NPYC"

# ------------------------------------------------------------ 5. isolation
head_ "5. isolation (engine vs game assets)"
LEAK_JS="$(find "$DIST/assets" -type f -name '*.js' 2>/dev/null | head -3 | tr '\n' ' ')"
[ -z "$LEAK_JS" ] && ok "assets/ contains no .js" || bad "assets/ contains no .js" "found: $LEAK_JS"
LEAK_MEDIA="$(find "$DIST/web-assets" -type f \( -iname '*.png' -o -iname '*.wav' -o -iname '*.ogg' \) 2>/dev/null | head -3 | tr '\n' ' ')"
[ -z "$LEAK_MEDIA" ] && ok "web-assets/ contains no game media" || bad "web-assets/ contains no game media" "found: $LEAK_MEDIA"

# -------------------------------------------------------------- 6. manifest
head_ "6. manifest"
if [ -f "$MANIFEST" ] && head -n 1 "$MANIFEST" | grep -q '^Caesura (AmeKAG) web package:'; then
    ok "MANIFEST.txt header line"
else
    bad "MANIFEST.txt header line" "first line is not 'Caesura (AmeKAG) web package: <game>'"
fi
if [ -f "$MANIFEST" ] && grep -qE '[[:space:]]index\.html$' "$MANIFEST" && grep -qE '[[:space:]]cache/story/story\.lua$' "$MANIFEST"; then
    ok "MANIFEST.txt lists index.html and cache/story/story.lua"
else
    bad "MANIFEST.txt lists index.html and cache/story/story.lua" "one or both entries absent"
fi

# --------------------------------------------------------------- summary
printf '\n== summary: PASS %d / FAIL %d\n' "$PASS" "$FAIL"
if [ "$FAIL" -gt 0 ]; then
    for f in "${FAILURES[@]}"; do printf '  FAILED: %s\n' "$f"; done
    exit 1
fi
exit 0
