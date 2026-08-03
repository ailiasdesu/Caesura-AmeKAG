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