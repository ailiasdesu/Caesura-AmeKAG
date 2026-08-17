#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — verify_template.sh
#
#  End-to-end verification for the new-project template (demo/template/).
#  Content-agnostic: asserts the template "splits once, reaches [end] with
#  zero errors", not any specific wording. Reuses the generic headless driver.
#
#  Steps
#   1. ks_check  — static contract check of story.ks (goal: zero warnings)
#   2. headless  — kag_runner drives the whole script to [end] (DONE)
#   3. branches  — reachability probe for the two choice routes
#                   (forest_path / city_lights) that funnel into *credits
#   4. web smoke — informational manual step (not executed here)
#
#  Usage (from repo root):  bash scripts/verify_template.sh
#  Exit: 0 = all checks passed, 1 = any check failed.
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
cd "$ROOT" || { echo "[verify-template] cannot cd repo root"; exit 1; }

LUA="external/lua/lua.exe"
if [ ! -f "$LUA" ]; then LUA="$(command -v lua || true)"; fi
if [ -z "$LUA" ] || ! [ -e "$LUA" ]; then
    echo "[verify-template] FATAL: no Lua interpreter (expected external/lua/lua.exe)"; exit 1
fi

STORY="${TEMPLATE_STORY:-demo/template/story.ks}"
BRANCHES="${TEMPLATE_BRANCHES:-forest city}"
DRIVER="tests/scripts/sample_game_headless.lua"
FRAME_BUDGET="${TEMPLATE_FRAMES:-200000}"

PASS=0; FAIL=0
note() { echo "[verify-template] $*"; }

check() { # check <name> <exitcode> [detail]
    if [ "$2" -eq 0 ]; then PASS=$((PASS + 1)); note "PASS  $1"
    else FAIL=$((FAIL + 1)); note "FAIL  $1  (exit=$2 ${3:-})"; fi
}

echo ""
echo "================================================================"
echo "  Template verification — $STORY"
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
printf "%s\n" "$MAIN_OUT" | grep -E "RESULT|ENDING|FATAL" | sed 's/^/  /' || true
if [ "$MAIN_RC" -eq 0 ] && printf "%s\n" "$MAIN_OUT" | grep -q "RESULT DONE"; then
    check "headless: template runs to DONE" 0
else
    check "headless: template runs to DONE" "$MAIN_RC"
fi

# ---- 3. Choice-route reachability ----
note "Step 3: choice-route reachability (branches -> *credits -> [end])"
for b in $BRANCHES; do
    RL="$(SAMPLE_STORY="$STORY" SAMPLE_ENDING="$b" SAMPLE_FRAMES="$FRAME_BUDGET" "$LUA" "$DRIVER" 2>&1 | grep -E "RESULT|ENDING" || true)"
    printf "%s\n" "$RL" | sed 's/^/  /'
    if printf "%s\n" "$RL" | grep -q "ENDING_NOT_FOUND"; then
        check "$b branch reachable -> DONE" 2
    elif printf "%s\n" "$RL" | grep -q "RESULT DONE"; then
        check "$b branch reachable -> DONE" 0
    else
        check "$b branch reachable -> DONE" 1
    fi
done

# ---- 4. Web smoke (informational) ----
echo ""
note "Step 4: Web smoke (manual — not executed here)"
note "  bash scripts/package_game.sh --out dist/template-tmp $STORY"
note "  then serve dist/template-tmp and pick the scene from the dropdown."

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

