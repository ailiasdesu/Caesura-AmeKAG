# Headless AI integration smoke: drives the REAL engine binary through
# stdio JSON-RPC eval against a REAL local Ollama server (when present).
# Exercises the AI binding end-to-end: sync AI.query (with the binding's
# model auto-discovery) and async AI.query_async through pollMainThreadJobs.
# Without Ollama (CI) the script exits 77 -> ctest SKIP.
import json
import os
import queue
import subprocess
import sys
import threading
import time
import urllib.request

exe = sys.argv[1] if len(sys.argv) > 1 else "CaesuraAmeKAG.exe"
cwd = os.path.dirname(os.path.abspath(exe)) or "."

def ollama_reachable(timeout=2.0):
    try:
        with urllib.request.urlopen("http://127.0.0.1:11434/api/tags",
                                    timeout=timeout) as resp:
            return resp.status == 200, json.loads(resp.read())
    except Exception:
        return False, None

ok, tags = ollama_reachable()
if not ok:
    print("SKIPPED: Ollama unreachable (127.0.0.1:11434) - real-AI smoke not run")
    sys.exit(77)
models = (tags or {}).get("models") or []
if not models:
    print("SKIPPED: Ollama reachable but no models pulled")
    sys.exit(77)
model = models[0].get("name", "")

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
        pass
    raise RuntimeError("engine did not reach Backends ready within 45s "
                       "(last line: %r)" % (boot_line or "<none>"))

results = []
_line_queue = queue.Queue()

def _reader_loop():
    while True:
        line = proc.stdout.readline()
        if not line:
            _line_queue.put(None)
            break
        _line_queue.put(line)

threading.Thread(target=_reader_loop, daemon=True).start()

_req_id = [0]
def request(code, timeout=20):
    _req_id[0] += 1
    req = {"id": _req_id[0], "method": "eval", "code": code}
    proc.stdin.write(json.dumps(req) + "\n")
    proc.stdin.flush()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError("timeout waiting for eval: " + code[:80])
        try:
            line = _line_queue.get(timeout=min(remaining, 5.0))
        except queue.Empty:
            continue
        if line is None:
            raise RuntimeError("engine closed stdout")
        try:
            parsed = json.loads(line)
        except ValueError:
            continue
        if isinstance(parsed, dict) and "event" in parsed:
            continue
        if parsed.get("id") == _req_id[0]:
            return parsed
    raise RuntimeError("timeout waiting for eval: " + code[:80])

def check(name, condition):
    results.append((name, bool(condition)))
    print(("PASS " if condition else "FAIL ") + name)

try:
    # 1. Sync query with model auto-discovery (empty model -> binding asks
    # the server for its first available model).
    r = request(
        "return tostring(AI.query('Reply with the single word: hello', "
        "{ timeout_ms = 120000 }))",
        timeout=150)
    reply = r.get("result") or ""
    check("ai-sync-real-reply", r.get("status") == "ok"
          and len(reply) > 0 and reply != "nil")
    print("  reply: " + reply[:80].replace("\n", " "))

    # 2. Async query (callback from pollMainThreadJobs) with explicit model.
    # The eval sandbox blocks NEW globals; package.loaded is an existing
    # writable table (also proves the sandbox cooperates with AI.query_async).
    r = request(
        "package.loaded._ai_smoke = nil; "
        "local ok = AI.query_async('Reply with the single word: hello', "
        "{ model = '" + model + "', timeout_ms = 120000 }, "
        "function(r, e) package.loaded._ai_smoke = { text = r, err = e } end); "
        "return tostring(ok)",
        timeout=20)
    check("ai-async-accepted", r.get("status") == "ok"
          and r.get("result") == "true")

    deadline = time.monotonic() + 150
    done = False
    while time.monotonic() < deadline and not done:
        time.sleep(1.0)
        r = request("return package.loaded._ai_smoke ~= nil", timeout=20)
        done = r.get("result") == "true"
    check("ai-async-completed", done)
    r = request(
        "return tostring(package.loaded._ai_smoke and "
        "package.loaded._ai_smoke.text or '')",
        timeout=20)
    areply = r.get("result") or ""
    check("ai-async-real-reply", len(areply) > 0 and areply != "nil")
    print("  reply: " + areply[:80].replace("\n", " "))

finally:
    try:
        proc.stdin.close()
    except Exception:
        pass
    try:
        proc.terminate()
    except Exception:
        pass
    try:
        proc.wait(timeout=5)
    except Exception:
        proc.kill()

failed = [n for n, ok in results if not ok]
if failed:
    print("HEADLESS AI SMOKE FAILED: " + ", ".join(failed))
    sys.exit(1)
print("ALL HEADLESS AI SMOKE TESTS PASSED (model " + model + ")")
sys.exit(0)
