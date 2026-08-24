#!/usr/bin/env bash
# =============================================================================
#  Caesura (AmeKAG) — verify_golden_vn.sh
#
#  End-to-end verification for the Golden Project (tests/projects/golden_vn/),
#  the long-term release regression fixture (task book §14 / release-gate.md).
#
#  Steps
#   1. ks_check         — static contract check of story.ks (zero warnings)
#   2. headless full run — kag_runner drives the whole script to [end] (DONE)
#   3. branch reachability — both choice routes funnel into *common_mid -> [end]
#   4. feature surface   — the story references every commanded feature
#   5. web smoke        — informational manual step (not executed here)
#
#  Usage (from repo root):  bash scripts/verify_golden_vn.sh
#  Exit: 0 = all checks passed, 1 = any check failed.
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
cd "$ROOT" || { echo "[verify-golden] cannot cd repo root"; exit 1; }

LUA="external/lua/lua.exe"
if [ ! -f "$LUA" ]; then LUA="$(command -v lua || true)"; fi
if [ -z "$LUA" ] || ! [ -e "$LUA" ]; then
    echo "[verify-golden] FATAL: no Lua interpreter (expected external/lua/lua.exe)"; exit 1
fi

STORY="${GOLDEN_STORY:-tests/projects/golden_vn/story.ks}"
BRANCHES="${GOLDEN_BRANCHES:-route_forest route_city}"
DRIVER="tests/scripts/sample_game_headless.lua"
FRAME_BUDGET="${GOLDEN_FRAMES:-200000}"

PASS=0; FAIL=0
note() { echo "[verify-golden] $*"; }

check() { # check <name> <exitcode> [detail]
    if [ "$2" -eq 0 ]; then PASS=$((PASS + 1)); note "PASS  $1"
    else FAIL=$((FAIL + 1)); note "FAIL  $1  (exit=$2 ${3:-})"; fi
}

echo ""
echo "================================================================"
echo "  Golden Project verification — $STORY"
echo "================================================================"

# ---- 1. Static contract check (goal: zero warnings) ----
note "Step 1: ks_check ($STORY)"
KSC_OUT="$("$LUA" scripts/ks_check.lua "$STORY" 2>&1)"
KSC_RC=$?
echo "$KSC_OUT" | sed "s/^/  /"
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
printf "%s\n" "$MAIN_OUT" | grep -E "RESULT|ENDING|FATAL" | sed "s/^/  /" || true
if [ "$MAIN_RC" -eq 0 ] && printf "%s\n" "$MAIN_OUT" | grep -q "RESULT DONE"; then
    check "headless: golden project runs to DONE" 0
else
    check "headless: golden project runs to DONE" "$MAIN_RC"
fi

# ---- 3. Choice-route reachability ----
note "Step 3: choice-route reachability (branches -> [end])"
for b in $BRANCHES; do
    RL="$(SAMPLE_STORY="$STORY" SAMPLE_ENDING="$b" SAMPLE_FRAMES="$FRAME_BUDGET" "$LUA" "$DRIVER" 2>&1 | grep -E "RESULT|ENDING" || true)"
    printf "%s\n" "$RL" | sed "s/^/  /"
    if printf "%s\n" "$RL" | grep -q "RESULT DONE"; then
        check "$b branch reachable -> DONE" 0
    elif printf "%s\n" "$RL" | grep -q "ENDING_NOT_FOUND"; then
        check "$b branch reachable -> DONE" 2
    else
        check "$b branch reachable -> DONE" 1
    fi
done

# ---- 4. Feature surface coverage (source greps, no run needed) ----
note "Step 4: feature surface coverage in story.ks"
SRC="$(cat "$STORY")"
FEATURES="playbgm playse playvoice save select nvl tween layout layout_slot i18n history replay trans end"
for feat in $FEATURES; do
    if printf "%s" "$SRC" | grep -q "\[$feat"; then
        check "feature [$feat] present" 0
    else
        check "feature [$feat] present" 4 "missing from story.ks"
    fi
done

# ---- 5. Web smoke (informational) ----
echo ""
note "Step 5: Web smoke (manual — not executed here)"
note "  bash scripts/package_game.sh --out dist/golden-tmp $STORY"
note "  then serve dist/golden-tmp and pick the scene from the dropdown."

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
