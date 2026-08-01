# Headless HTTP editor smoke test: exercises the real engine's HTTP editor
# transport end-to-end (ping / eval / getState / breakpoint lifecycle /
# continue / inspect reachability) plus the Live2D model-load route.
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request

exe = sys.argv[1] if len(sys.argv) > 1 else "CaesuraAmeKAG.exe"
cwd = os.path.dirname(os.path.abspath(exe)) or "."

port = 9876
proc = subprocess.Popen(
    [exe, "--editor"],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
    cwd=cwd,
)

results = []
BASE = "http://127.0.0.1:%d" % port


def check(name, ok, detail=""):
    results.append((name, ok))
    if not ok:
        print("FAIL", name, detail)


def request(path, data=None, timeout=30):
    req = urllib.request.Request(
        BASE + path, data=data,
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            body = r.read().decode("utf-8", errors="replace")
            try:
                return r.status, json.loads(body)
            except Exception:
                return r.status, {"raw": body[:200]}
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", errors="replace")
        try:
            return e.code, json.loads(body)
        except Exception:
            return e.code, {"raw": body[:200]}
    except Exception as e:
        return -1, {"error": str(e)}


def main():
    # Wait for the HTTP server to come up.
    ready = False
    for _ in range(80):
        if proc.poll() is not None:
            break
        try:
            with urllib.request.urlopen(BASE + "/api/ping", timeout=1) as r:
                if r.status == 200:
                    ready = True
                    break
        except Exception:
            time.sleep(0.5)
    check("server-ready", ready)
    if not ready:
        finish(1)

    st, resp = request("/api/ping")
    check("ping", st == 200 and resp.get("status") == "ok", "%s %s" % (st, resp))

    st, resp = request("/api/status")
    check("status", st == 200 and resp.get("status") == "ok", "%s %s" % (st, resp))

    st, resp = request("/api/eval", data=b"return 6 * 7")
    check("eval", st == 200 and resp.get("result") == "42", "%s %s" % (st, resp))

    st, resp = request("/api/eval", data=b"error('boom')")
    check("eval-error-surfaced", st == 500 and "boom" in str(resp), "%s %s" % (st, resp))

    st, resp = request("/api/debug/getState")
    check("getState", st == 200 and resp.get("status") == "ok", "%s %s" % (st, resp))

    st, resp = request("/api/debug/setBreakpoint",
                       data=json.dumps({"source": "demo_story.ks", "line": 10}).encode())
    check("setBreakpoint", st == 200 and resp.get("status") == "ok", "%s %s" % (st, resp))

    st, resp = request("/api/debug/removeBreakpoint",
                       data=json.dumps({"source": "demo_story.ks", "line": 10}).encode())
    check("removeBreakpoint", st == 200 and resp.get("status") == "ok", "%s %s" % (st, resp))

    st, resp = request("/api/debug/clearBreakpoints", data=b"")
    check("clearBreakpoints", st == 200 and resp.get("status") == "ok", "%s %s" % (st, resp))

    # continue/inspect without an active pause: must produce a semantic JSON
    # error (not a crash / empty body).
    st, resp = request("/api/debug/continue", data=b"{}")
    check("continue-reachable", st == 400 and "stale_pause" in str(resp), "%s %s" % (st, resp))

    st, resp = request("/api/debug/inspect?name=x&global=1")
    check("inspect-reachable", st == 400 and "inspection_unavailable" in str(resp),
          "%s %s" % (st, resp))

    st, resp = request("/api/debug/getFrame?w=320&h=240")
    check("getFrame-reachable", st == 500 and "capture_failed" in str(resp),
          "%s %s" % (st, resp))

    # Live2D model-load route must respond (Haru lives under live2d_test in
    # the build output; load may fail if the file is absent, but the route
    # must answer with a JSON error, never hang or crash).
    st, resp = request("/api/live2d/load",
                       data=json.dumps({"modelPath": "live2d_test/Haru/Haru.model3.json",
                                        "x": 0, "y": 0, "scale": 1, "show": True}).encode(),
                       timeout=40)
    check("live2d-load-route", st in (200, 500) and "raw" not in resp, "%s %s" % (st, resp))

    time.sleep(1)
    check("engine-alive", proc.poll() is None)

    finish(0 if all(ok for _, ok in results) else 1)


def finish(rc):
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except Exception:
            proc.kill()
    passed = sum(1 for _, ok in results if ok)
    total = len(results)
    print("HTTP smoke: %d/%d passed" % (passed, total))
    if rc == 0:
        print("ALL HEADLESS HTTP SMOKE TESTS PASSED")
    sys.exit(rc)


if __name__ == "__main__":
    main()
