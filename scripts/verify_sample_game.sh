#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — verify_sample_game.sh
#
#  End-to-end verification for the sample game (demo/example_game/story.ks).
#  Drives the draft through the engine verification facilities and reports
#  PASS/FAIL. Content-agnostic: asserts "runs to [end] with zero errors", not
#  specific story wording.
#
#  Steps
#   1. ks_check  — static contract check of story.ks (goal: zero warnings)
#   2. headless  — kag_runner drives the whole script to [end] (DONE)
#   3. endings   — reachability probe for the three ending labels
#                   (ending_zero / ending_companion / ending_promise)
#   4. web smoke — informational manual step (not executed here)
#
#  Usage (from repo root):  bash scripts/verify_sample_game.sh
#  Exit: 0 = all checks passed, 1 = any check failed.
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
cd "$ROOT" || { echo "[verify] cannot cd repo root"; exit 1; }

# Lua interpreter probe -- same three levels as scripts/caesura_build.py::find_lua:
#   packaged external/lua/lua[.exe] (release-package artifact; gitignored in a
#   checkout) -> build-tree lua_cli product (build/lua/<config>/lua[.exe], which
#   a fresh clone has after cmake --build) -> PATH lua5.4 / lua.
# FATAL only when ALL levels miss, listing every location probed (honest
# diagnostics; never a silent skip). Keep in sync with caesura_build.find_lua.
LUA=""
for _luacand in \
    external/lua/lua.exe external/lua/lua \
    build/lua/Release/lua.exe build/lua/Release/lua \
    build/lua/Debug/lua.exe build/lua/Debug/lua \
    build/lua/RelWithDebInfo/lua.exe build/lua/RelWithDebInfo/lua \
    build/lua/MinSizeRel/lua.exe build/lua/MinSizeRel/lua \
    build/lua/lua.exe build/lua/lua
do
    if [ -f "$_luacand" ]; then LUA="$_luacand"; break; fi
done
if [ -z "$LUA" ]; then
    LUA="$(command -v lua5.4 2>/dev/null || true)"
    [ -n "$LUA" ] || LUA="$(command -v lua 2>/dev/null || true)"
fi
if [ -z "$LUA" ] || [ ! -e "$LUA" ]; then
    echo "[verify] FATAL: no Lua interpreter (probed: external/lua/lua[.exe], build/lua/{Release,Debug,RelWithDebInfo,MinSizeRel}/lua[.exe], build/lua/lua[.exe], PATH lua5.4/lua)"; exit 1
fi

STORY="${SAMPLE_STORY:-demo/example_game/story.ks}"
ENDINGS="ending_zero ending_companion ending_promise"
DRIVER="tests/scripts/sample_game_headless.lua"
FRAME_BUDGET="${SAMPLE_FRAMES:-200000}"

PASS=0; FAIL=0
note() { echo "[verify] $*"; }

check() { # check <name> <exitcode> [detail]
    if [ "$2" -eq 0 ]; then PASS=$((PASS + 1)); note "PASS  $1"
    else FAIL=$((FAIL + 1)); note "FAIL  $1  (exit=$2 ${3:-})"; fi
}

echo ""
echo "================================================================"
echo "  Sample game verification — $STORY"
echo "================================================================"

# ---- 1. Static contract check (goal: zero warnings) ----
note "Step 1: ks_check ($STORY)"
KSC_OUT="$("$LUA" scripts/ks_check.lua "$STORY" 2>&1)"
KSC_RC=$?
echo "$KSC_OUT" | sed 's/^/  /'
WARN_COUNT="$(printf "%s" "$KSC_OUT" | grep -c "^[WARN]" || true)"
if [ "$KSC_RC" -eq 1 ]; then
    check "ks_check: clean contract" 1 "($WARN_COUNT warnings)"
elif [ "$WARN_COUNT" -gt 0 ]; then
    note "  (informational: $WARN_COUNT lint warning(s), not a CI gate)"
    check "ks_check: clean contract" 0
else
    check "ks_check: clean contract, zero warnings" 0
fi

# ---- 2. Headless full run to [end] ----
note "Step 2: headless full run to [end] (frame budget=$FRAME_BUDGET)"
MAIN_OUT="$(SAMPLE_STORY="$STORY" SAMPLE_FRAMES="$FRAME_BUDGET" "$LUA" "$DRIVER" 2>&1)"
MAIN_RC=$?
printf "%s
" "$MAIN_OUT" | grep -E "RESULT|ENDING|FATAL" | sed 's/^/  /' || true
if [ "$MAIN_RC" -eq 0 ] && printf "%s
" "$MAIN_OUT" | grep -q "RESULT DONE"; then
    check "headless: full story runs to DONE" 0
else
    check "headless: full story runs to DONE" "$MAIN_RC"
fi

# ---- 3. Three-ending reachability ----
note "Step 3: three-ending reachability"
for e in $ENDINGS; do
    RL="$(SAMPLE_STORY="$STORY" SAMPLE_ENDING="$e" SAMPLE_FRAMES="$FRAME_BUDGET" "$LUA" "$DRIVER" 2>&1 | grep -E "RESULT|ENDING" || true)"
    printf "%s
" "$RL" | sed 's/^/  /'
    if printf "%s
" "$RL" | grep -q "ENDING_NOT_FOUND"; then
        check "$e reachable -> DONE" 2
    elif printf "%s
" "$RL" | grep -q "RESULT DONE"; then
        check "$e reachable -> DONE" 0
    else
        check "$e reachable -> DONE" 1
    fi
done

# ---- 4. Web smoke (informational) ----
echo ""
note "Step 4: Web smoke (manual — not executed here)"
note "  The web player runs the shipped demo/example_game/story.ks bundle, not"
note "  story.ks. To verify the draft in-browser you must first fold it into"
note "  the web bundle, then drive cache/story/story.lua via web/story.bundle.sweep.test.js."
note "  See docs/guides/sample-game-verification.md for the manual walkthrough."

echo ""
echo "================================================================"
TOTAL=$((PASS + FAIL))
if [ "$FAIL" -eq 0 ]; then
    echo "  RESULT: PASS ($PASS/$TOTAL checks)"
    echo "================================================================"
    exit 0
else
    echo "  RESULT: FAIL ($FAIL/$TOTAL checks failed)"
    echo "================================================================"
    exit 1
fi