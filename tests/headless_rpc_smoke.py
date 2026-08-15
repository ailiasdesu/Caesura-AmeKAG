# Headless JSON-RPC integration test: exercises the real engine's stdio RPC
# transport end-to-end (ping / eval / managed-coroutine run / post-run state).
import json
import os
import subprocess
import queue
import threading
import sys
import time

exe = sys.argv[1] if len(sys.argv) > 1 else "CaesuraAmeKAG.exe"
cwd = os.path.dirname(os.path.abspath(exe)) or "."

proc = subprocess.Popen(
    [exe, "--headless"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL,
    cwd=cwd,
    bufsize=1,
    text=True,
    encoding="utf-8",
    errors="replace",
)

# The engine boot window (Lua runtime init, script scan) is NOT covered by
# the per-request timeout below -- on a loaded CI runner the first ping can
# arrive >20s after spawn and flake. Wait for the headless-ready banner with
# a generous startup deadline before issuing any request.
startup_deadline = time.monotonic() + 45.0
boot_line = None
while time.monotonic() < startup_deadline:
    line = proc.stdout.readline()
    if not line:
        break
    boot_line = line.strip()
    if "Backends ready" in line:
        break
if not boot_line or "Backends ready" not in boot_line:
    try:
        proc.kill()
    except Exception:
        pass  # child may already have exited
    raise RuntimeError("engine did not reach 'Backends ready' within 45s "
                       "(last line: %r)" % (boot_line or "<none>"))

results = []


# Singleton stdout reader: one daemon thread pumps readline() into a queue.
# A fresh reader thread per request would race for lines with the previous
# (still-blocked) reader and swallow responses -- the request timeout then
# fires even though the engine replied.
_line_queue = queue.Queue()

def _reader_loop():
    while True:
        line = proc.stdout.readline()
        if not line:
            _line_queue.put(None)  # EOF sentinel
            break
        _line_queue.put(line)

threading.Thread(target=_reader_loop, daemon=True).start()

def request(req, timeout=20):
    proc.stdin.write(json.dumps(req) + "\n")
    proc.stdin.flush()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError("timeout waiting for response to " + json.dumps(req))
        try:
            line = _line_queue.get(timeout=min(remaining, 5.0))
        except queue.Empty:
            continue  # no line yet; keep polling within the deadline
        if line is None:
            raise RuntimeError("engine closed stdout")
        try:
            parsed = json.loads(line)
        except ValueError:
            continue  # engine banner / plain log lines that are not JSON
        if isinstance(parsed, dict) and "event" in parsed:
            continue  # {"event":"log",...} push lines are not responses
        return parsed
    raise RuntimeError("timeout waiting for response to " + json.dumps(req))


def check(name, condition):
    results.append((name, bool(condition)))
    print(("PASS " if condition else "FAIL ") + name)


try:
    r = request({"id": 1, "method": "ping"})
    check("ping", r.get("result") == "ok")

    r = request({"id": 2, "method": "eval", "code": "return 1 + 1"})
    check("eval-arithmetic", r.get("status") == "ok" and r.get("result") == "2")

    r = request({"id": 3, "method": "eval", "code": "error('boom')"})
    check("eval-error-propagates", "error" in r)

    # The first managed run may take longer on slow CI runners (the engine
    # must reach its per-frame pump loop before the run is serviced); give
    # it the same window as the boot deadline instead of the default 20s.
    r = request({"id": 4, "method": "run",
                 "script": "print('managed-run-start'); "
                           "coroutine.yield(); "
                           "print('managed-run-resumed')"}, timeout=45)
    check("run-started", r.get("status") == "ok")

    # Give the managed coroutine time to yield and be resumed across frames.
    time.sleep(1.5)

    r = request({"id": 5, "method": "eval", "code": "return 2 * 3"})
    check("eval-after-run", r.get("status") == "ok" and r.get("result") == "6")

    r = request({"id": 6, "method": "eval", "code": "return type(_G.print)"})
    check("vm-alive-after-run", r.get("status") == "ok" and r.get("result") == "function")

    r = request({"id": 7, "method": "run", "script": "error('run-failure')"})
    check("run-with-error-accepted", r.get("status") == "ok")

    time.sleep(0.5)
    r = request({"id": 8, "method": "eval", "code": "return 7 + 7"})
    check("eval-after-failing-run", r.get("status") == "ok" and r.get("result") == "14")

    # Round 29: JSON \u escapes must decode BMP and astral-plane (surrogate
    # pair) code points into correct UTF-8 on the live readJsonString path.
    r = request({"id": 8, "method": "eval", "code": "return 'é中😀'"})
    check("eval-unicode-escapes",
          r.get("status") == "ok" and r.get("result") == "é中😀")

    # ---- KAG scene-level debugger (Neo-Genesis) ---------------------------
    r = request({"id": 9, "method": "kagSetBreakpoint",
                 "params": {"scene": "assets/script/main.ks", "cmd": "ch"}})
    check("kag-set-breakpoint", r.get("status") == "ok" and r.get("result") == "true")

    r = request({"id": 10, "method": "kagSetBreakpoint",
                 "params": {"scene": "assets/script/main.ks", "line": 42}})
    check("kag-set-breakpoint-line", r.get("status") == "ok")

    r = request({"id": 11, "method": "kagSetBreakpoint",
                 "params": {"scene": "assets/script/main.ks"}})
    check("kag-bad-breakpoint-rejected", r.get("error") is not None)

    r = request({"id": 12, "method": "kagInspectScopes", "params": {"scope": "f"}})
    check("kag-inspect-scope", r.get("status") == "ok")

    r = request({"id": 13, "method": "kagInspectScopes"})
    check("kag-inspect-all", r.get("status") == "ok")

    r = request({"id": 14, "method": "kagDebugStep"})
    check("kag-debug-step", r.get("status") == "ok")

    r = request({"id": 15, "method": "kagDebugContinue"})
    check("kag-debug-continue", r.get("status") == "ok")

    r = request({"id": 16, "method": "kagClearBreakpoints"})
    check("kag-clear-breakpoints", r.get("status") == "ok")

    # ---- scene hot reload (editor workflow) --------------------------------
    r = request({"id": 17, "method": "kagReloadScene",
                 "params": {"scene": "assets/script/__missing__.ks"}})
    check("kag-reload-scene-missing-graceful",
        r.get("status") == "ok" and "error" in (r.get("result") or ""))
    r = request({"id": 18, "method": "kagReloadScene"})
    check("kag-reload-scene-no-scene-graceful",
        r.get("status") == "ok")

    # ---- Round 57: LSP endpoints against the contract registry -----------
    # The editor's lspCall() bridges to kag.lsp.json() via /api/eval;
    # validate completion / hover / diagnostics for the round-51 contract
    # commands (schema-driven, same registry ks_check uses).
    _lsp_id = [19]

    def lsp_json(method, *args):
        code = "local lsp = require('kag.lsp'); return lsp.json('%s'%s)" % (
            method, "".join(", " + json.dumps(a) for a in args))
        _lsp_id[0] += 1
        return request({"id": _lsp_id[0], "method": "eval", "code": code})

    r = lsp_json("hover", "blur", "duration")
    _b = r.get("result") or ""
    check("lsp-hover-blur-contract",
        r.get("status") == "ok" and '"title"' in _b
        and "type=number" in _b and "duration" in _b)

    r = lsp_json("hover", "fade", "duration")
    _b = r.get("result") or ""
    check("lsp-hover-fade-contract",
        r.get("status") == "ok" and "type=number" in _b)

    r = lsp_json("hover", "unlock", "type")
    _b = r.get("result") or ""
    check("lsp-hover-unlock-param",
        r.get("status") == "ok" and "type=" in _b)

    r = lsp_json("hover", "saveload", "mode")
    _b = r.get("result") or ""
    check("lsp-hover-saveload-mode",
        r.get("status") == "ok" and "mode" in _b and "type=" in _b)

    r = lsp_json("hover", "select")
    _b = r.get("result") or ""
    check("lsp-hover-select-params", r.get("status") == "ok" and "params:" in _b)

    r = lsp_json("completion", "[sav")
    _b = r.get("result") or ""
    check("lsp-completion-saveload", r.get("status") == "ok" and "saveload" in _b)

    r = lsp_json("completion", "[bl")
    _b = r.get("result") or ""
    check("lsp-completion-blur", r.get("status") == "ok" and "blur" in _b)

    r = lsp_json("completion", "[ch ")
    _b = r.get("result") or ""
    check("lsp-completion-ch-params", r.get("status") == "ok" and "text" in _b)

    r = lsp_json("diagnostics", '[ch name="A" text="x"]\n[if exp="f.hp >"]')
    _b = r.get("result") or ""
    check("lsp-diag-bad-expr-compile",
        r.get("status") == "ok" and "does not compile" in _b)

    r = lsp_json("diagnostics", "[playbgm vol=1]")
    _b = r.get("result") or ""
    check("lsp-diag-missing-anyof",
        r.get("status") == "ok" and "requires one of" in _b)

    r = lsp_json("diagnostics", "[blurrr oops=1]")
    _b = r.get("result") or ""
    check("lsp-diag-unknown-command",
        r.get("status") == "ok" and len(_b) > 2)
finally:
    try:
        proc.stdin.close()
    except Exception:
        pass
    try:
        proc.wait(timeout=15)
    except Exception:
        try:
            proc.kill()
        except Exception:
            pass

failed = [name for name, ok in results if not ok]
if failed:
    print("FAILED: " + ", ".join(failed))
    sys.exit(1)
print("ALL HEADLESS RPC TESTS PASSED")