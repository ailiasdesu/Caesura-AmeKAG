"""End-to-end verification: whitepaper example scene through the REAL engine
(headless + RPC eval). The scene uses set/inc/for/macro/iscript + %f.i%
interpolation; asserts the scheduler produced the expected variable state."""
import json
import os
import queue
import subprocess
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(ROOT, "build", "Debug", "CaesuraAmeKAG.exe")

SCENE = """*start
[set f.gold 100]
[inc f.gold 50]
[for var="i" start="1" end="3"]
  [ch name="Hero" text="count %f.gold% i=%f.i%"]
  [p]
[endfor]
[macro show_status args="hp,mp"]
  [ch name="Hero" text="HP %hp% / MP %mp%"]
[endmacro]
[show_status hp=100 mp=50]
[iscript]
  f.bonus = f.gold > 200
[/endscript]
*end
[ending]
"""

LUA = (
    "(function() "
    "local tokenizer = require('tokenizer');"
    "local scheduler = require('scheduler');"
    "local toks = tokenizer.parse([[" + SCENE + "]]);"
    "local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},"
    " tokens = toks, token_index = 1, current_scene = 'wp_verify.ks',"
    " label_index = {}, characters = {} };"
    "local co = coroutine.create(function() scheduler.run(ctx, toks, 1) end);"
    "local err;"
    "while coroutine.status(co) ~= 'dead' do"
    " local ok, e = coroutine.resume(co);"
    " if not ok then err = e break end "
    "end;"
    "if err then return 'SCENE_FAIL: ' .. tostring(err) end;"
    "return string.format('gold=%s bonus=%s', tostring(ctx.f.gold), tostring(ctx.f.bonus))"
    "end)()"
)

proc = subprocess.Popen([EXE, "--headless"], stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                        cwd=os.path.dirname(EXE), bufsize=1, text=True,
                        encoding="utf-8", errors="replace")
deadline = time.monotonic() + 60
ready = False
while time.monotonic() < deadline:
    line = proc.stdout.readline()
    if "Backends ready" in line:
        ready = True
        break
print("ready:", ready)
assert ready

q = queue.Queue()

def reader():
    while True:
        line = proc.stdout.readline()
        if not line:
            q.put(None)
            return
        q.put(line)

threading.Thread(target=reader, daemon=True).start()
req = {"jsonrpc": "2.0", "id": 1, "method": "eval", "params": {"code": "return " + LUA}}
proc.stdin.write(json.dumps(req) + "\n")
proc.stdin.flush()
result = None
dl = time.monotonic() + 30
while time.monotonic() < dl:
    line = q.get(timeout=30)
    if line is None:
        break
    try:
        m = json.loads(line)
        if m.get("id") == 1:
            result = m
            break
    except json.JSONDecodeError:
        continue
print("ENGINE RESULT:", result)
proc.stdin.write(json.dumps({"jsonrpc": "2.0", "id": 2, "method": "stop"}) + "\n")
proc.stdin.flush()
try:
    proc.wait(timeout=5)
except Exception:
    proc.kill()

ok = result and result.get("status") == "ok" and "gold=150" in result.get("result", "")
print("WHITEPAPER_SCENE_ENGINE_VERIFIED" if ok else "VERIFICATION_FAILED")
raise SystemExit(0 if ok else 1)
