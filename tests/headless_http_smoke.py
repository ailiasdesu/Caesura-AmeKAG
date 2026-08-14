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


def request(path, data=None, timeout=30, headers=None):
    hdrs = {"Content-Type": "application/json"}
    if headers:
        hdrs.update(headers)
    req = urllib.request.Request(
        BASE + path, data=data, headers=hdrs,
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
        # The editor needs a GPU window; on headless CI runners (no display,
        # or SDL dummy video driver) bgfx init fails and the engine exits.
        # That is an environment limitation, not an HTTP-route regression --
        # the same routes are covered locally and by the stdio smoke. Skip
        # with ctest SKIP_RETURN_CODE (77) instead of failing the pipeline.
        # Skip only when the engine genuinely exited non-zero (GPU-less
        # runners: bgfx init fails, process exits). If the engine is still
        # alive but the server never became ready, that is a real route or
        # startup regression and must fail, not skip. A single poll() call
        # avoids a TOCTOU between two checks.
        rc = proc.poll()
        if rc is not None and rc != 0:
            print("HTTP smoke: skipped (editor needs GPU, engine exited rc=%d)" % rc)
            print("HTTP SMOKE SKIPPED: NO GPU")
            sys.exit(77)
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

    # Canonical /api/state (IDE preview panel): typed fields present.
    st, resp = request("/api/state")
    check("state-endpoint", st == 200 and resp.get("status") == "ok"
          and isinstance(resp.get("scene"), str)
          and isinstance(resp.get("token_index"), int)
          and isinstance(resp.get("language"), str)
          and isinstance(resp.get("backlog_count"), int)
          and isinstance(resp.get("layer_count"), int),
          "%s %s" % (st, resp))

    # /api/sma/validate (round 19): valid asset ok, broken asset lists
    # field-located violations, unsafe paths rejected.
    st, resp = request("/api/sma/validate?path=demo/assets/sma/hero.json")
    check("sma-validate-hero", st == 200 and resp.get("status") == "ok"
          and resp.get("ok") is True and resp.get("errors") == []
          and isinstance(resp.get("meta"), str) and "bones" in resp.get("meta", ""),
          "%s %s" % (st, resp))
    st, resp = request("/api/sma/validate?path=demo/assets/sma/_broken_example.json")
    check("sma-validate-broken", st == 200 and resp.get("status") == "ok"
          and resp.get("ok") is False
          and len(resp.get("errors", [])) >= 4
          and any("undefined bone" in e for e in resp.get("errors", [])),
          "%s %s" % (st, resp))
    st, resp = request("/api/sma/validate?path=../../outside.json")
    check("sma-validate-unsafe-path", st == 400, "%s %s" % (st, resp))
    st, resp = request("/api/sma/validate?path=demo/assets/sma/missing.json")
    check("sma-validate-missing-file", st == 200 and resp.get("ok") is False
          and resp.get("errors") and "cannot open" in resp.get("errors", [])[0],
          "%s %s" % (st, resp))

    # Rejected: line must be a positive int32. 4294967297 (2^32+1) previously
    # wrapped to a positive int (line 1) via unchecked get<int>(), silently
    # setting a wrong breakpoint.
    st, resp = request("/api/debug/setBreakpoint",
                       data=json.dumps({"source": "demo_story.ks", "line": 0}).encode())
    check("setBreakpoint-line0-rejected", st == 400, "%s %s" % (st, resp))
    st, resp = request("/api/debug/setBreakpoint",
                       data=json.dumps({"source": "demo_story.ks",
                                        "line": 4294967297}).encode())
    check("setBreakpoint-overflow-rejected", st == 400, "%s %s" % (st, resp))

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

    # --editor runs with GPU enabled, so a real frame capture may succeed
    # (200 + base64) or fail (500 + capture_failed); either is a valid answer.
    st, resp = request("/api/debug/getFrame?w=320&h=240")
    ok_frame = (st == 200 and isinstance(resp.get("base64"), str) and resp.get("base64"))
    ok_frame = ok_frame or (st == 500 and "capture_failed" in str(resp))
    check("getFrame-reachable", ok_frame, "%s %s" % (st, resp))

    # Live2D model-load route must respond (Haru lives under live2d_test in
    # the build output; load may fail if the file is absent, but the route
    # must answer with a JSON error, never hang or crash).
    st, resp = request("/api/live2d/load",
                       data=json.dumps({"modelPath": "live2d_test/Haru/Haru.model3.json",
                                        "x": 0, "y": 0, "scale": 1, "show": True}).encode(),
                       timeout=40)
    check("live2d-load-route", st in (200, 500) and "raw" not in resp, "%s %s" % (st, resp))

    # CORS: only localhost/127.0.0.1 origins are allowed.
    st, resp = request("/api/status", headers={"Origin": "http://evil.example.com"})
    check("cors-evil-origin-rejected", st == 403, "%s %s" % (st, resp))
    st, resp = request("/api/status", headers={"Origin": "http://localhost.evil.com"})
    check("cors-evil-subdomain-rejected", st == 403, "%s %s" % (st, resp))
    # Short / malformed Origin headers must not crash the editor.
    st, resp = request("/api/status", headers={"Origin": "http:/"})
    check("cors-short-origin-no-crash", st == 403, "%s %s" % (st, resp))
    st, resp = request("/api/status", headers={"Origin": "http://localhost:5173"})
    check("cors-localhost-allowed", st == 200, "%s %s" % (st, resp))
    # No token configured in the default editor launch: requests stay open.
    st, resp = request("/api/status")
    check("no-token-open", st == 200, "%s %s" % (st, resp))

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
