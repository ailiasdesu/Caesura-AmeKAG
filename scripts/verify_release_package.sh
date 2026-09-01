#!/usr/bin/env bash
# =====================================================================
# verify_release_package.sh -- prove the STRANGER path on a release ZIP
#
# What it proves (and why each step exists):
#   1. the archive really contains the editor frontend, the engine
#      executable and the runtime data dirs;
#   2. the engine, launched from the EXTRACTED directory (outside the
#      repository), actually SERVES the editor -- HTTP 200 with the
#      editor HTML, not 404 and not "serving API only";
#   3. the default-deny auth gate is still closed (unauthenticated GET /
#      must be 401) -- this script must never "fix" a red run by
#      relaxing authentication;
#   4. a stranger can obtain the token without reading source: with no
#      CAESURA_EDITOR_TOKEN configured the engine writes
#      .caesura-editor-token into its working directory;
#   5. demo/ is present and NON-EMPTY (anchored on a stable file) --
#      ProjectContext.looksLikeEngineRoot requires scripts + demo, and a
#      package missing demo/ silently degrades the editor's Project Manager
#      sourceRoot resolution (an empty demo/ dir is equally useless).
#
# RUN SERIALLY on the machine. The port-hygiene guard below is a WHOLE-MACHINE
# check: it sees every CaesuraAmeKAG process by name, and aborts (exit 2) while
# ANY engine is running -- the point, not a bug, because a leftover engine can
# dual-bind 127.0.0.1:9876 (Windows SO_REUSEADDR) and answer with its own
# (deleted) webRoot and token, faking 404/401 failures against a good package.
# Never run two editor-engine verifications in parallel on one host. A harness
# that cleans up engines BY IMAGE NAME (taskkill //IM CaesuraAmeKAG.exe) kills a
# parallel session's engine mid-run too -- always clean by PID.
#
# Deliberately NOT tolerant: a missing ZIP or a missing file inside it is
# a FAILURE, never a silent pass. --skip-if-missing exits 77 (the ctest
# SKIP convention) and prints the exact command that produces the ZIP.
#
# usage:
#   bash scripts/verify_release_package.sh [ZIP] [--skip-if-missing]
#                                          [--port N | --port=N] [--keep]
#   ARCHIVE defaults to the newest build/CaesuraAmeKAG-*.zip or
#   build/CaesuraAmeKAG-*.tar.gz (Zip = Windows CPack, tar.gz = Linux/Sprint6-L1)
#   Unknown -flags are rejected instead of being read as an archive path: reading
#   "--port 9999" as a filename produced the baffling diagnostic
#   "no release archive at the given path: 9999".
# =====================================================================
set -u

ZIP=""
SKIP_IF_MISSING=0
PORT=9876
KEEP=0
WANT_PORT=0
for arg in "$@"; do
    if [ "$WANT_PORT" = "1" ]; then PORT="$arg"; WANT_PORT=0; continue; fi
    case "$arg" in
        --skip-if-missing) SKIP_IF_MISSING=1 ;;
        --keep)            KEEP=1 ;;
        --port=*)          PORT="${arg#--port=}" ;;
        --port)            WANT_PORT=1 ;;
        -h|--help)         sed -n '2,34p' "$0"; exit 0 ;;
        -*)                printf 'unknown option: %s\n' "$arg"
                           printf 'usage: bash scripts/verify_release_package.sh [ZIP] [--skip-if-missing] [--port N] [--keep]\n'
                           exit 2 ;;
        *)                 ZIP="$arg" ;;
    esac
done
if [ "$WANT_PORT" = "1" ]; then printf -- '--port needs a value\n'; exit 2; fi
case "$PORT" in
    ''|*[!0-9]*) printf -- '--port must be a number, got: %s\n' "$PORT"; exit 2 ;;
esac

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PASS=0
FAIL=0
declare -a FAILURES=()

ok()   { PASS=$((PASS+1)); printf '  [PASS] %s\n' "$1"; }
bad()  { FAIL=$((FAIL+1)); FAILURES+=("$1"); printf '  [FAIL] %s -- %s\n' "$1" "${2:-}"; }
head_() { printf '\n== %s\n' "$1"; }

# ---------------------------------------------------------------- 0. ZIP
head_ "0. locate the release archive"
if [ -z "$ZIP" ]; then
    # A DESKTOP package only: build/ also collects the web bundle
    # (CaesuraAmeKAG-<v>-web.zip) and the CPack SOURCE archive
    # (CaesuraAmeKAG-<v>-Source.zip); picking either by mtime made every content
    # assertion below fail against an archive that was never meant to contain an
    # executable -- a confusing red that says nothing about the desktop package.
    # CPack names desktop archives <name>-<version>-<System>-<Arch>.zip;
    # Sprint6-L1 also ships the Linux flavor as <same>-Linux-x86_64.tar.gz, so
    # accept BOTH container formats here (a wrong container is as misleading as
    # a wrong flavor -- excluding it would produce the same confusing red).
    CANDS="$(ls -t "$REPO_ROOT"/build/CaesuraAmeKAG-*.zip "$REPO_ROOT"/build/CaesuraAmeKAG-*.tar.gz 2>/dev/null \
           | grep -v '\.sha256$' | grep -v -- '-web\.zip$' | grep -v -- '-Source\.zip$' \
           | grep -v -- '-web\.tar\.gz$' | grep -v -- '-Source\.tar\.gz$' || true)"
    ZIP="$(printf '%s\n' "$CANDS" | head -1)"
    # Auto-selection is mtime-based, so SAY which archive won and what else was
    # in the running. A silent pick is how a stale package gets verified and
    # reported as if it were the one just built.
    NCAND="$(printf '%s\n' "$CANDS" | grep -c . || true)"
    if [ "$NCAND" -gt 1 ] 2>/dev/null; then
        printf '%s desktop candidates in build/, picking the newest by mtime:\n' "$NCAND"
        printf '%s\n' "$CANDS" | sed 's/^/  /'
    fi
fi
if [ -z "$ZIP" ] || [ ! -f "$ZIP" ]; then
    if [ -n "$ZIP" ]; then
        printf 'no release archive at the given path: %s\n' "$ZIP"
    else
        printf 'no release archive found (looked for %s/build/CaesuraAmeKAG-*.zip and -*.tar.gz)\n' "$REPO_ROOT"
    fi
    printf 'produce one with:\n'
    printf '  cmake --build build --config Release --parallel\n'
    printf '  cd build && cpack -C Release -G ZIP\n'
    if [ "$SKIP_IF_MISSING" = "1" ]; then
        printf 'SKIP: no package to verify (--skip-if-missing)\n'
        exit 77
    fi
    printf 'FAIL: nothing to verify. Build a package, or pass --skip-if-missing.\n'
    exit 2
fi
printf 'archive: %s (%s bytes)\n' "$ZIP" "$(wc -c < "$ZIP" | tr -d ' ')"

# ------------------------------------------------------------- 1. extract
head_ "1. extract to a temporary directory OUTSIDE the repository"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/caesura-relverify.XXXXXX")"
case "$WORK" in
    "$REPO_ROOT"*) printf 'FAIL: temp dir %s is inside the repo\n' "$WORK"; exit 2 ;;
esac
# Engine-process ownership, NOT by name. list_engine_pids() below returns
# EVERY CaesuraAmeKAG on the box -- a foreign engine (another session's editor,
# a teammate's smoke run) shares the name but not our identity, and name-based
# ALIVE/kill let its answers and its token leak into this run's verdict (the
# confusing 401s in t3). Identity = the UNIQUE extraction dir (mktemp), so the
# run marks its own engines by the caesura-relverify.XXXXXX marker in their
# command line. A foreign engine (another session's extraction, a teammate's
# smoke run) is started from a different directory and can never match, whether
# it predates us or appears mid-run -- a pid snapshot diff cannot tell a
# mid-run arrival from ours; the dir marker can. If a foreign engine holds
# 127.0.0.1:9876, OUR engine fails to bind and exits, own_engine_pids() turns
# empty and the run fails loudly with OUR stderr -- a foreign 404/401 can no
# longer be inherited into the verdict.
own_engine_pids() {
    local mark="$(basename "$WORK")"
    if command -v powershell >/dev/null 2>&1; then
        powershell -NoProfile -Command \
            "Get-CimInstance Win32_Process -Filter \"Name='CaesuraAmeKAG.exe'\" | Where-Object { \$_.CommandLine -like '*$mark*' } | ForEach-Object { \$_.ProcessId }" 2>/dev/null | tr -d '\r'
    else
        ps -eo pid,args 2>/dev/null | grep CaesuraAmeKAG | grep -F "$mark" | awk '{print $1}'
    fi
}
kill_own_engines() {
    local p
    for p in $(own_engine_pids); do stop_pid "$p"; done
    # t102: the ps-based scan is not trustworthy on the mac runner -- also
    # kill the engine recorded at launch (engine.pid), so cleanup cannot
    # orphan a live --editor on any platform.
    # t108: pragmatic identity guard -- kill -0 liveness first, then verify
    # via ps -p (comm/args contains CaesuraAmeKAG) when ps can answer; when the
    # platform ps cannot inspect the pid (empty result / git-bash limited
    # fields) degrade to the liveness check with the recorded pid (it is OUR
    # launch; CI runner semantics unchanged).
    for p in $(cat "$WORK/engine.pid" 2>/dev/null); do
        [ -n "$p" ] && [ "$p" -gt 0 ] 2>/dev/null || continue
        if ! kill -0 "$p" 2>/dev/null; then continue; fi
        if command -v ps >/dev/null 2>&1; then
            PSLINE="$(ps -p "$p" -o comm= 2>/dev/null)"
            if [ -z "$PSLINE" ]; then PSLINE="$(ps -p "$p" -o args= 2>/dev/null)"; fi
            if [ -z "$PSLINE" ] || printf '%s' "$PSLINE" | grep -qi 'CaesuraAmeKAG'; then
                stop_pid "$p"
            fi
        else
            stop_pid "$p"
        fi
    done
}
cleanup() {
    for p in $(own_engine_pids); do stop_pid "$p"; done
    if [ "$KEEP" = "1" ]; then printf '\n(kept %s)\n' "$WORK"; else rm -rf "$WORK"; fi
}
list_engine_pids() {
    if command -v powershell >/dev/null 2>&1; then
        powershell -NoProfile -Command \
            "(Get-Process CaesuraAmeKAG -ErrorAction SilentlyContinue).Id" 2>/dev/null | tr -d '\r'
    else
        pgrep -x CaesuraAmeKAG 2>/dev/null || true
    fi
}
stop_pid() {
    if command -v taskkill >/dev/null 2>&1; then taskkill //F //PID "$1" >/dev/null 2>&1 || true
    else kill -9 "$1" >/dev/null 2>&1 || true; fi
}
trap cleanup EXIT

# Port hygiene: the editor binds 127.0.0.1:9876, but Windows SO_REUSEADDR lets a
# STALE engine (leftover from another session) dual-bind and keep answering --
# its deleted webRoot yields 404 "index.html not found", its token yields 401 on
# /api/ping, while OUR engine stays alive next to it (observed in Sprint 4: a
# run failed 5 checks whose root cause was a leftover engine, not the package).
# A precondition this script cannot fix must FAIL LOUDLY with the reason, never
# slither into per-check verdicts. Checks both stacks: the server binds
# 127.0.0.1, yet a twin can hold ::1:9876.
#
# N7 contract: STALE_ENGINES is a WHOLE-MACHINE check (every CaesuraAmeKAG by
# name). Verification must therefore run SERIALLY on a host -- one editor-engine
# verification at a time -- and all engine cleanup must be by PID, never by
# image name: a harness doing "taskkill //IM CaesuraAmeKAG.exe" to "clean up"
# kills a parallel session's running engine too. Other sessions (editor e2e,
# dev debugging) must be finished or quiesced before this script runs.
#
# POSIX matches by EXACT process name (pgrep -x), NEVER pgrep -f: this script's
# own wrapper chain (xvfb-run / bash) carries the archive path -- which contains
# "CaesuraAmeKAG" -- in its command line, so -f self-matches and aborts a clean
# run (first bitten: CI run 33189827175, Linux · Package).
if command -v powershell >/dev/null 2>&1; then
    STALE_ENGINES="$(powershell -NoProfile -Command "(Get-Process CaesuraAmeKAG -ErrorAction SilentlyContinue).Id" 2>/dev/null | tr -d '\r')"
else
    STALE_ENGINES="$(pgrep -x CaesuraAmeKAG 2>/dev/null || true)"
fi
if [ -n "$STALE_ENGINES" ]; then
    printf 'FAIL: engine processes already running -- port 9876 is not clean:\n'
    printf '      %s\n' "$STALE_ENGINES" | sed 's/^/        /'
    printf '      A leftover engine can dual-bind 9876 and answer this run with its OWN\n'
    printf '      (deleted) webRoot and token, faking 404/401 failures.\n'
    printf '      Kill it first (kill by PID, never by image name):\n'
    if command -v powershell >/dev/null 2>&1; then
        printf '        powershell "Get-Process CaesuraAmeKAG -ErrorAction SilentlyContinue | Stop-Process -Force"\n'
    else
        printf '        pkill -x CaesuraAmeKAG   (or: kill <pid-from-ps>)\n'
    fi
    exit 2
fi
# Extract by container format: ZIP (Windows CPack) and tar.gz (Linux/Sprint6-L1)
# both land as a single top-level directory holding the engine executable.
case "$ZIP" in
    *.tar.gz|*.tgz)
        tar -xzf "$ZIP" -C "$WORK" || { printf 'FAIL: tar -xzf failed for %s\n' "$ZIP"; exit 2; } ;;
    *)
        unzip -q "$ZIP" -d "$WORK" || { printf 'FAIL: unzip failed\n'; exit 2; } ;;
esac
# Prefer the top-level directory that actually holds the engine executable.
# Blindly taking the first directory made a wrong-archive run (the web bundle,
# whose first top dir is assets/) report six content failures about a package
# that was never a desktop package -- the failures were true but useless.
ROOT=""
for d in "$WORK"/*/; do
    d="${d%/}"
    if [ -f "$d/CaesuraAmeKAG.exe" ] || [ -f "$d/CaesuraAmeKAG" ]; then ROOT="$d"; break; fi
done
if [ -z "$ROOT" ]; then
    if [ -f "$WORK/CaesuraAmeKAG.exe" ] || [ -f "$WORK/CaesuraAmeKAG" ]; then
        ROOT="$WORK"
    else
        ROOT="$(find "$WORK" -mindepth 1 -maxdepth 1 -type d | head -1)"
        [ -n "$ROOT" ] || ROOT="$WORK"
        # No executable anywhere: name the likely cause instead of letting the
        # content checks below blame the desktop install() rules.
        printf 'note: no CaesuraAmeKAG executable under any top-level directory --\n'
        printf '      this may not be a desktop package. top-level entries:\n'
        (cd "$WORK" && ls -1 | sed 's/^/        /' | head -10)
    fi
fi
printf 'extracted root: %s\n' "$ROOT"

# ------------------------------------------------------------ 2. contents
head_ "2. archive contents (a stranger has nothing but this folder)"
EXE=""
for cand in "$ROOT/CaesuraAmeKAG.exe" "$ROOT/CaesuraAmeKAG"; do
    [ -f "$cand" ] && EXE="$cand" && break
done
if [ -n "$EXE" ]; then ok "engine executable: $(basename "$EXE")"
else bad "engine executable" "neither CaesuraAmeKAG.exe nor CaesuraAmeKAG at the archive root"; fi

if [ -f "$ROOT/web-editor/dist/index.html" ]; then ok "web-editor/dist/index.html"
else bad "web-editor/dist/index.html" "web-editor/dist/index.html is NOT in the package (CMake install() must ship it; --editor serves this single-file debug panel -- the React IDE in editor/ is a source-tree component and is NOT part of release packages)"; fi

for d in scripts assets; do
    if [ -d "$ROOT/$d" ]; then ok "$d/ present"; else bad "$d/ present" "missing from the archive"; fi
done

# Sprint 5: the stranger-creation path needs the project templates (create)
# and a standalone Lua interpreter (ks_check/precompile) IN the package.
# python stays a documented prerequisite (doctor: Required); lua is not -- the
# package must be its own lua host via external/lua/lua.exe.
TEMPLATES="$ROOT/tools/project_templates"
if [ -d "$TEMPLATES" ]; then ok "tools/project_templates/ present"
else bad "tools/project_templates/" "missing from the archive (CMake install() must ship tools/project_templates)"; fi
for tpl in basic blank kag3 live2d showcase; do
    if [ -d "$TEMPLATES/$tpl" ]; then ok "project template: $tpl"
    else bad "project template: $tpl" "missing under tools/project_templates/"; fi
done
LUAEXE=""
for cand in "$ROOT/external/lua/lua.exe" "$ROOT/external/lua/lua"; do
    [ -f "$cand" ] && LUAEXE="$cand" && break
done
if [ -n "$LUAEXE" ]; then ok "external/lua/lua(.exe) bundled"
else bad "external/lua/lua(.exe)" "bundled Lua interpreter missing (install(TARGETS lua_cli ...) must ship it)"; fi
# Sprint 5 (N6): ProjectContext.looksLikeEngineRoot requires scripts + demo;
# without demo/ the editor Project Manager sourceRoot resolution silently
# degrades. Non-empty is anchored on a real stable file -- demo/cjk_smoke.ks
# is tracked in repo and shipped via install(DIRECTORY demo/ ...); an empty
# demo/ directory would pass dir-existence but still break creation.
DEMO_ANCHOR="$ROOT/demo/cjk_smoke.ks"
DEMO_ENTRIES=""
[ -d "$ROOT/demo" ] && DEMO_ENTRIES="$(find "$ROOT/demo" -mindepth 1 -maxdepth 1 | wc -l | tr -d ' ')"
if [ -d "$ROOT/demo" ] && [ -f "$DEMO_ANCHOR" ] && [ "$DEMO_ENTRIES" -gt 0 ] 2>/dev/null; then
    ok "demo/ present and non-empty (${DEMO_ENTRIES} top-level entr$( { [ "$DEMO_ENTRIES" = "1" ] && echo y || echo ies; } ); anchored demo/cjk_smoke.ks)"
else
    bad "demo/ present and non-empty" "demo/ missing, empty or without $DEMO_ANCHOR (ProjectContext.looksLikeEngineRoot needs scripts+demo)"
fi

# ------------------------------------------- 2b. packaged SDL3 dylib relocatability
# macOS hard gate (t36 plan item d, landed 2026-08-29): the Darwin TGZ must run
# on ANY machine, not be pinned to the build host. Two related regressions:
#   (1) a brew-built SDL3 dylib shipped as-is carries an ABSOLUTE LC_ID_DYLIB
#       (/opt/homebrew/opt/sdl3/lib/libSDL3.dylib) -- the package only runs on
#       the builder (E6 evidence in docs/plans/audit/macos-packaging-lane-plan.md);
#   (2) the engine binary's SDL3 LOAD path is absolute -- same pin, other side.
# Assert BOTH when otool exists AND a libSDL3*.dylib is packaged. The glob
# carries the version on purpose: SDL3's real SONAME file is libSDL3.0.dylib,
# not libSDL3.dylib (a match only on the unversioned name would silently take
# the note path and leave the gate empty). No otool (Windows/Linux) or no
# packaged dylib (statically linked SDL3) -> ok WITH a scope note, the same
# note-pass precedent as the POSIX stripped-PATH probe, so the check count
# stays platform-uniform (30/30 on every lane) and Windows/Linux runs keep
# every existing verdict byte-identical.
head_ "2b. packaged SDL3 dylib is relocatable"
if ! command -v otool >/dev/null 2>&1; then
    ok "packaged SDL3 dylib is relocatable -- otool absent (Windows/Linux): dylib relocation is a macOS-only property, nothing to assert here [note]"
else
    SDL_DYLIB="$(compgen -G "$ROOT/libSDL3*.dylib" 2>/dev/null | head -1)"
    if [ -z "$SDL_DYLIB" ]; then
        ok "packaged SDL3 dylib is relocatable -- no libSDL3*.dylib in package (statically linked SDL3; nothing to relocate) [note]"
    elif [ -z "$EXE" ]; then
        bad "packaged SDL3 dylib is relocatable" "dylib present ($SDL_DYLIB) but no engine executable to inspect (otool -L)"
    else
        # (1) LC_ID_DYLIB of the packaged dylib: @rpath/@loader_path/@executable_path
        #     (i.e. relative-ish) -- NEVER an absolute path.
        DYLIB_BAD=""
        DYLIB_ID="$(otool -D "$SDL_DYLIB" 2>/dev/null | tail -1 | tr -d ' ')"
        case "$DYLIB_ID" in
            @rpath/*|@loader_path/*|@executable_path/*) ;;
            *) DYLIB_BAD="LC_ID_DYLIB=$DYLIB_ID (absolute install_name pins the package to the build host)" ;;
        esac
        # (2) every SDL3 load item of the engine binary must be @rpath/@loader_path-
        #     relative; an absolute /opt/homebrew/... load path is the same pin.
        EXEBAD=""
        EXE_SDL_N=0
        while IFS= read -r _sdl_line; do
            EXE_SDL_N=$((EXE_SDL_N + 1))
            case "$_sdl_line" in
                @rpath/*|@loader_path/*|@executable_path/*) ;;
                *) EXEBAD="$EXEBAD; $_sdl_line" ;;
            esac
        done <<< "$(otool -L "$EXE" 2>/dev/null | grep -i sdl3 | sed 's/^[[:space:]]*//' | sed 's/[[:space:]].*//')"
        if [ "$EXE_SDL_N" = "0" ]; then
            EXEBAD="no SDL3 load item in otool -L output although $SDL_DYLIB is packaged"
        fi
        EXEBAD="${EXEBAD#; }"
        if [ -z "$DYLIB_BAD" ] && [ -z "$EXEBAD" ]; then
            ok "packaged SDL3 dylib is relocatable (id=$DYLIB_ID; engine SDL3 load item is @rpath/@loader_path)"
        elif [ -n "$DYLIB_BAD" ]; then
            bad "packaged SDL3 dylib is relocatable" "$DYLIB_BAD (absolute path pins the package to the build host)"
        else
            bad "packaged SDL3 dylib is relocatable" "$EXEBAD (absolute path pins the package to the build host)"
        fi
    fi
fi

# --------------------------------------------------------------- 3. serve
head_ "3. launch --editor from the extracted folder and fetch the editor"
if [ -z "$EXE" ]; then
    bad "editor serves HTML" "no executable to launch"
else
    TOKEN="relverify-$$-$(date +%s)"
    OUT="$WORK/editor.out"; ERR="$WORK/editor.err"
    T3_START="$(date +%s)"
    # t81/t93/t102: record the engine's own exit code AND its direct pid.
    # The launcher subshell backgrounds the engine, records the real engine
    # pid (NOT the subshell's) in engine.pid, waits for it and writes the
    # exit code to editor.rc only when the engine really exits.
    rm -f "$WORK/editor.rc" "$WORK/engine.pid"
    ( cd "$ROOT" && { CAESURA_EDITOR_TOKEN="$TOKEN" "$EXE" --editor >"$OUT" 2>"$ERR" & EPID=$!; echo "$EPID" >"$WORK/engine.pid"; wait "$EPID"; echo "$?" >"$WORK/editor.rc"; } ) &
    CODE="000"
    ALIVE=1
    for _ in $(seq 1 45); do
        sleep 1
        CODE="$(curl -s -o "$WORK/root.html" -m 3 -w '%{http_code}' \
                 -H "Authorization: Bearer $TOKEN" "http://127.0.0.1:$PORT/" 2>/dev/null)"
        [ -n "$CODE" ] || CODE="000"
        # t102: readiness-poll -- keep asking until the server answers (up to
        # 45s; slow software-rendered runners take multiple seconds). Already-
        # passing checks must not regress: the first non-000 response is the
        # truth, so the GET / below uses the READY code.
        [ "$CODE" != "000" ] && break
        # t102: death = the launcher recorded the exit (editor.rc exists). The
        # loop NEVER declares death from a process-name scan: on the round-7
        # mac runner the ps -args probe returned empty while the engine was
        # alive and serving (the same section then got browser-nav/api-ping
        # 200) -- a live process was judged dead and rc read as "(not
        # recorded)". A genuinely dead engine still fails fast: its launcher
        # writes editor.rc ~1s after the exit and the next iteration
        # ends the wait.
        if [ -f "$WORK/editor.rc" ]; then ALIVE=0; break; fi
    done

    printf '        [diag] editor readiness poll took %ss (45s cap)\n' "$(( $(date +%s) - T3_START ))"
    if [ "$ALIVE" = "1" ]; then
        printf '        [diag] editor ready after %ss\n' "$(( $(date +%s) - T3_START ))"
        ok "engine process stayed alive in the extracted folder"
    else
        bad "engine process stayed alive" "it exited early; stderr tail: $(tail -2 "$ERR" 2>/dev/null | tr '\n' ' | ')"
        # t81 forensics: an early exit leaves the exit code and stdout as the
        # only evidence -- the FAIL line above keeps two stderr lines (e.g. the
        # round-3 macOS run showed only [ShaderCache] compileVariant errors).
        # Print what the process actually logged, capped.
        # t93: poll up to 3s for the launcher to record rc (POSIX crash
        # handling can delay reaping beyond the old 1s grace).
        RCR=0
        while [ ! -s "$WORK/editor.rc" ] && [ "$RCR" -lt 6 ]; do RCR=$((RCR + 1)); sleep 0.5; done
        printf '        [diag] engine exit code: %s\n' "$(cat "$WORK/editor.rc" 2>/dev/null || echo '(not recorded)')"
        printf '        [diag] stdout tail (15 lines):\n'
        tail -n 15 "$OUT" 2>/dev/null | sed 's/^/          /'
        printf '        [diag] [RENDER]/FATAL/ERROR across stdout+stderr:\n'
        grep -hE '\[RENDER\]|FATAL|\[ERROR\]' "$OUT" "$ERR" 2>/dev/null | head -12 | sed 's/^/          /'

        # t93: macOS crash-report capture -- a crashing --editor (Metal drawable
        # hypothesis, H2) lands as CaesuraAmeKAG-*.ips in
        # ~/Library/Logs/DiagnosticReports; print the newest report head
        # (exception type + crashed-thread frames -- watch for MTLDrawable/
        # CAMetalLayer/nextDrawable frames as H2 evidence). The block is silent
        # on non-mac (no DiagnosticReports dir); Linux best-effort via
        # coredumpctl when it finds caesura core dumps.
        MAC_CRASH_DIR="$HOME/Library/Logs/DiagnosticReports"
        if [ -d "$MAC_CRASH_DIR" ]; then
            printf '        [diag] macOS DiagnosticReports (newest CaesuraAmeKAG):\n'
            ls -t "$MAC_CRASH_DIR" 2>/dev/null | grep -i 'caesura' | head -5 | sed 's/^/          /'
            LATEST_IPS="$(ls -t "$MAC_CRASH_DIR"/CaesuraAmeKAG*.ips 2>/dev/null | head -1)"
            if [ -n "$LATEST_IPS" ]; then
                printf '        [diag] newest report %s (head 80):\n' "$(basename "$LATEST_IPS")"
                head -80 "$LATEST_IPS" 2>/dev/null | sed 's/^/          /'
            else
                printf '        [diag] no CaesuraAmeKAG*.ips report (crash may not produce one)\n'
            fi
        elif command -v coredumpctl >/dev/null 2>&1; then
            CRASH_LINES="$(coredumpctl list --no-pager 2>/dev/null | grep -i caesura | tail -5 || true)"
            if [ -n "$CRASH_LINES" ]; then
                printf '        [diag] Linux coredumpctl (caesura core dumps):\n'
                printf '%s\n' "$CRASH_LINES" | sed 's/^/          /'
            fi
        fi

        # t93: pure-diagnostic demo probe -- NOT a check ([PASS]/[FAIL] and the
        # 30 count are untouched). H1 (scene content) vs H2 (hidden window /
        # editor path) discriminator: a visible-window 60-frame DEMO run that
        # exits 0 while --editor died is hard evidence for an editor-path
        # (e.g. Metal drawable) crash.
        DEMO_OUT="$WORK/demo-probe.log"
        ( cd "$ROOT" && "$EXE" --frames 60 ) >"$DEMO_OUT" 2>&1
        DEMO_RC=$?
        printf '        [diag] demo probe: "%s" --frames 60 -> rc=%s\n' "$(basename "$EXE")" "$DEMO_RC"
        printf '        [diag] demo probe log tail (8 lines):\n'
        tail -8 "$DEMO_OUT" 2>/dev/null | sed 's/^/          /'
    fi

    if [ "$CODE" = "200" ]; then ok "GET / with token -> 200"
    else bad "GET / with token" "status=$CODE (404 = webRoot not resolved from this folder)"; fi

    if grep -qi 'Caesura Web Editor' "$WORK/root.html" 2>/dev/null; then
        ok "response body is the editor HTML"
    else
        bad "response body is the editor HTML" "first bytes: $(head -c 80 "$WORK/root.html" 2>/dev/null | tr '\n' ' ')"
    fi

    # THE check that matches the documented stranger path: a BROWSER cannot
    # attach an Authorization header when you type a URL, so the editor page
    # itself must be reachable unauthenticated (static, non-secret HTML -- the
    # token still guards every /api/* call the page makes afterwards).
    # Two designs satisfy this (the script does not prejudge which):
    #   (a) the static editor route is exempt from the token gate, or
    #   (b) the static route accepts the token in the query string, so the
    #       startup banner can print a clickable http://…/?token=… URL.
    BCODE="$(curl -s -o "$WORK/browser.html" -m 3 -w '%{http_code}' "http://127.0.0.1:$PORT/" 2>/dev/null)"
    [ -n "$BCODE" ] || BCODE="000"
    QCODE="$(curl -s -o "$WORK/browserq.html" -m 3 -w '%{http_code}' "http://127.0.0.1:$PORT/?token=$TOKEN" 2>/dev/null)"
    [ -n "$QCODE" ] || QCODE="000"
    BROWSER_OK=0
    grep -qi 'Caesura Web Editor' "$WORK/browser.html" 2>/dev/null && [ "$BCODE" = "200" ] && BROWSER_OK=1
    grep -qi 'Caesura Web Editor' "$WORK/browserq.html" 2>/dev/null && [ "$QCODE" = "200" ] && BROWSER_OK=1
    if [ "$BROWSER_OK" = "1" ]; then
        ok "browser navigation reaches the editor HTML (plain=$BCODE, ?token=$QCODE)"
    else
        bad "browser navigation reaches the editor HTML" \
            "plain=$BCODE ?token=$QCODE -- a browser cannot send an Authorization header when you type a URL, so the static editor route must either be exempt from the /api/* gate or accept ?token="
    fi

    # Absence of the warning only means something when the engine actually
    # ran: a process that died before start() logs nothing, and treating that
    # silence as success is exactly how a green-but-broken run happens.
    if [ "$ALIVE" != "1" ]; then
        bad "engine resolved its webRoot" "not verifiable: the process never reached editor startup"
    elif grep -q 'web-editor/dist not found' "$ERR" "$OUT" 2>/dev/null; then
        bad "engine resolved its webRoot" "engine logged: web-editor/dist not found; serving API only"
    else
        ok "engine reported no missing webRoot"
    fi

    PING="$(curl -s -m 3 -H "Authorization: Bearer $TOKEN" "http://127.0.0.1:$PORT/api/ping" || true)"
    case "$PING" in
        *'"status":"ok"'*) ok "/api/ping with token -> ok" ;;
        *)                bad "/api/ping with token" "body: ${PING:-<empty>}" ;;
    esac

    # The gate must stay closed where it matters: /api/* runs arbitrary Lua and
    # writes files. Serving a static page is not a reason to open it, and this
    # script must never turn a red run green by relaxing authentication.
    UNAUTH="$(curl -s -m 3 -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT/api/ping" 2>/dev/null)"
    [ -n "$UNAUTH" ] || UNAUTH="000"
    if [ "$UNAUTH" = "401" ]; then ok "unauthenticated /api/ping -> 401 (auth gate intact)"
    else bad "unauthenticated /api/ping -> 401" "status=$UNAUTH (the API gate must stay closed)"; fi

    kill_own_engines
    sleep 1
fi

# --------------------------------------------- 4. stranger obtains a token
head_ "4. stranger path: no CAESURA_EDITOR_TOKEN configured"
if [ -z "$EXE" ]; then
    bad "token discoverable" "no executable to launch"
else
    OUT2="$WORK/editor2.out"; ERR2="$WORK/editor2.err"
    rm -f "$ROOT/.caesura-editor-token" "$WORK/engine.pid" "$WORK/editor2.rc"
    # t102: record the engine pid so kill_own_engines() at the end of this
    # section cannot orphan it on the mac runner (ps scan unreliable there).
    # t108: symmetric launcher -- editor2.rc confirms the exit, so the token
    # loop below judges death WITHOUT a process-name scan (the round-7 §3
    # false-dead root: mac ps -args returns empty for a live process).
    ( cd "$ROOT" && { env -u CAESURA_EDITOR_TOKEN "$EXE" --editor >"$OUT2" 2>"$ERR2" & EPID=$!; echo "$EPID" >"$WORK/engine.pid"; wait "$EPID"; echo "$?" >"$WORK/editor2.rc"; } ) &
    GEN=""
    T4_START="$(date +%s)"
    for _ in $(seq 1 45); do
        sleep 1
        [ -s "$ROOT/.caesura-editor-token" ] && GEN="$(tr -d '\r\n' < "$ROOT/.caesura-editor-token")" && break
        # t108: death = the launcher recorded the exit (editor2.rc exists). A
        # live but slow engine keeps the whole 45x1s window open for the token.
        if [ -s "$WORK/editor2.rc" ]; then break; fi
    done
    if [ -n "$GEN" ]; then
        printf '        [diag] token appeared after %ss\n' "$(( $(date +%s) - T4_START ))"
    fi
    if [ -n "$GEN" ]; then ok ".caesura-editor-token written next to the executable"
    else bad ".caesura-editor-token written" "no token file appeared in $ROOT"; fi

    if grep -q 'Generated editor token' "$ERR2" "$OUT2" 2>/dev/null; then
        ok "token also printed on stderr"
    else
        bad "token printed on stderr" "startup log did not mention a generated token"
    fi

    if [ -n "$GEN" ]; then
        C2="$(curl -s -o "$WORK/root2.html" -m 3 -w '%{http_code}' \
              -H "Authorization: Bearer $GEN" "http://127.0.0.1:$PORT/" 2>/dev/null)"
        [ -n "$C2" ] || C2="000"
        if [ "$C2" = "200" ] && grep -qi 'Caesura Web Editor' "$WORK/root2.html"; then
            ok "editor reachable with the token from the file"
        else
            bad "editor reachable with the file token" "status=$C2"
        fi
    fi
    kill_own_engines
fi

# ------------------------------- 5. stranger creates + builds + runs a game
head_ "5. stranger path: create a project, build a game and run it (in-package only)"
if [ -z "$EXE" ]; then
    bad "create+build+run" "no executable to launch"
else
    # The package must be self-sufficient: create needs tools/project_templates,
    # build needs a Lua interpreter (python is a documented prerequisite, lua is
    # NOT any more -- find_lua() probes <pkg>/external/lua/lua.exe first). Strip
    # every lua-ish PATH entry: if the packaged interpreter were missing or
    # broken, build must FAIL, not quietly borrow a system lua. The log lines
    # below are the evidence that the package lua ran ks_check + precompile.
    NO_LUA_PATH="$(printf '%s' "$PATH" | tr ':' '
' | grep -vi 'lua' | paste -sd: -)"
    SYS_LUA="$(PATH="$NO_LUA_PATH" command -v lua 2>/dev/null || true)"
    [ -n "$SYS_LUA" ] || SYS_LUA="$(PATH="$NO_LUA_PATH" command -v lua5.4 2>/dev/null || true)"
    if [ -z "$SYS_LUA" ]; then
        ok "no lua on stripped PATH (package lua is the only interpreter)"
    elif ! command -v powershell >/dev/null 2>&1; then
        # POSIX: the distro lua lives in /usr/bin, which CANNOT be stripped
        # without killing bash/coreutils (the strip only drops lua-named dirs).
        # The self-sufficiency property is still asserted strictly below by the
        # "ran under the PACKAGED lua" log checks -- find_lua() must prefer
        # <pkg>/external/lua ahead of any PATH lua. First seen: CI run
        # 33192030337 (Linux · Package), where /usr/bin/lua made the strict
        # form unsatisfiable on every Linux host.
        ok "stripped PATH keeps distro lua ($SYS_LUA) -- POSIX bin dirs unstrippable; packaged-lua usage asserted below"
    else
        bad "no lua on stripped PATH" "command -v lua still resolves -- PATH strip failed"
    fi

    CL="$WORK/create.log"; BL="$WORK/build.log"; RL="$WORK/run.log"
    ( cd "$ROOT" && env PATH="$NO_LUA_PATH" python scripts/caesura.py create ProbeGame --template basic ) >"$CL" 2>&1
    CR=$?
    if [ "$CR" = "0" ]; then
        ok "package create ProbeGame --template basic -> exit 0"
        # Path separators differ between MSYS (ROOT) and python (Windows); match
        # the tools/project_templates/basic segment and REQUIRE the source is NOT
        # the repo checkout (its path contains the repo name) -- i.e. a package
        # copying templates from the developer tree would fail this check.
        TFSRC="$(grep 'template from' "$CL" | head -1)"
        if printf '%s' "$TFSRC" | grep -q 'project_templates.*basic' \
           && ! printf '%s' "$TFSRC" | grep -q '文件存放处'; then
            ok "template sourced from the package"
        else
            bad "template sourced from the package" "create says: ${TFSRC:-<no template from line>}"
        fi
    else
        bad "package create ProbeGame" "exit=$CR; tail: $(tail -4 "$CL" | tr '
' ' | ')"
    fi

    if [ "$CR" = "0" ]; then
        ( cd "$ROOT" && env PATH="$NO_LUA_PATH" python scripts/caesura.py build ProbeGame --engine . ) >"$BL" 2>&1
        BR=$?
        if [ "$BR" = "0" ]; then
            ok "package build ProbeGame --engine . -> exit 0"
        else
            bad "package build ProbeGame" "exit=$BR; tail: $(tail -5 "$BL" | tr '
' ' | ')"
        fi
        if grep -q '\[build\] ks_check: [0-9][0-9]* scene(s) pass contracts' "$BL"; then
            ok "ks_check ran under the PACKAGED lua"
        else
            bad "ks_check ran under the packaged lua" "log: $(grep -i 'ks_check' "$BL" | head -2 | tr '
' ' | ')"
        fi
        if grep -q '\[build\] precompile: [0-9][0-9]*/[0-9][0-9]* scene(s) cached into cache/ksc' "$BL"; then
            ok "precompile ran under the PACKAGED lua"
        else
            bad "precompile under the packaged lua" "log: $(grep -i 'precompile' "$BL" | head -2 | tr '
' ' | ')"
        fi
    else
        bad "package build ProbeGame" "skipped: create failed, nothing to build"
    fi

    GAME="$ROOT/dist/ProbeGame-game"
    if [ -f "$GAME/CaesuraAmeKAG.exe" ] || [ -f "$GAME/CaesuraAmeKAG" ]; then
        GE=""
        for cand in "$GAME/CaesuraAmeKAG.exe" "$GAME/CaesuraAmeKAG"; do
            [ -f "$cand" ] && GE="$cand" && break
        done
        ( cd "$GAME" && "$GE" --frames 60 ) >"$RL" 2>&1
        RR=$?
        # "FATAL" as a bare substring is NOT the criterion: the basic template's
        # entry.lua probes repo-relative paths (demo/template/story.ks), fails in
        # a game-only package and prints a misleading "[Template] FATAL: cannot
        # find story.ks" BEFORE gracefully returning; the generated boot shim
        # then starts the packaged scene itself (by design). The real failure
        # modes are: non-zero exit, the boot shim's own not-start line, or the
        # KAG runner never starting. Distinguish, don't guess.
        BOOTFATAL=$(grep -c '\[caesura\] FATAL' "$RL" || true)
        KAGRUN=$(grep -c 'KAG Runner] Started' "$RL" || true)
        # The shader guard degrades loudly instead of crashing: a broken shader
        # set now exits 0 with rendering disabled (BGFX_DEBUG_IFH). Exit codes
        # alone can no longer prove the renderer works -- the guard marker must
        # be ABSENT, so a silently degraded run fails this same check.
        RENDERDISABLED=$(grep -c 'rendering disabled (BGFX_DEBUG_IFH)' "$RL" || true)
        if [ "$RR" = "0" ] && [ "$BOOTFATAL" = "0" ] && [ "$KAGRUN" -ge 1 ] && [ "$RENDERDISABLED" = "0" ]; then
            ok "game exe --frames 60 -> exit 0, KAG runner started, renderer alive, no boot fatal"
        else
            bad "game exe --frames 60" "exit=$RR bootfatal=$BOOTFATAL kagrunner=$KAGRUN renderdisabled=$RENDERDISABLED; tail: $(tail -5 "$RL" | tr '
' ' | ')"
            # t81 forensics: the per-program renderer failures live INSIDE $RL
            # (e.g. the round-3 Linux GL/mesa run) -- the FAIL line above keeps
            # only a 5-line tail. Print every [RENDER] line (capped) plus the
            # backend/renderer identification line so CI logs carry the cause.
            printf '        [diag] $RL [RENDER] lines (max 25):\n'
            grep '\[RENDER\]' "$RL" 2>/dev/null | head -25 | sed 's/^/          /'
            printf '        [diag] backend/renderer identification:\n'
            grep -iE 'render.*(backend|created|initialized|selected|using|device)|backend.*render' "$RL" 2>/dev/null | head -6 | sed 's/^/          /'
        fi
    else
        bad "game exe --frames 60" "no dist game at $GAME"
    fi
fi

# --------------------------------------------------------------- verdict
printf '\n== verdict\n'
printf 'pass=%d fail=%d\n' "$PASS" "$FAIL"
if [ "$FAIL" -gt 0 ]; then
    printf 'failed checks:\n'
    for f in "${FAILURES[@]}"; do printf '  - %s\n' "$f"; done
    printf 'RELEASE PACKAGE VERIFICATION FAILED\n'
    exit 1
fi
printf 'RELEASE PACKAGE VERIFICATION PASSED\n'
exit 0
