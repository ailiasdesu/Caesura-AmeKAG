"""Real encrypted save/restore across two owned engine processes.

This lane proves variable/control state and execution continuation. It does not
claim GPU pixels or real audio playback from the headless backend.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

COMMON = r'''
local runner = require('kag_runner')
local saves = require('kag.commands.save')
local state = require('kag.save_state')
local compiler = require('kag.compiler')
local text_scene = require('kag.text_scene')
local function observe()
    local c = assert(runner.get_ctx())
    local draws = {}
    text_scene.render(c, {
        render_text=function(...) draws[#draws+1]={'text',...} end,
        render_ruby=function(...) draws[#draws+1]={'ruby',...} end,
    })
    local frames = {}
    for i, frame in ipairs(c.call_stack or {}) do
        frames[i] = {scene=frame.scene,index=frame.index,lf=frame.lf,mp=frame.mp,
                     control=frame.control}
    end
    return compiler.encode_lua_literal({f=c.f,sf=c.sf,lf=c.lf,mp=c.mp,
        scene=c.current_scene,index=c._executing_index or c.token_index,
        frames=frames,control=state.capture_control(c),
        text=text_scene.capture(c),draws=draws})
end
local function finish()
    for turn=1,80 do
        local _, why = runner.update(60)
        if why == 'ended' then
            local c = assert(runner.get_ctx())
            assert(c.f.finished and c.f.visits==2 and c.f.caller_owner=='caller')
            assert(c.f.callee_owner=='callee' and c.f.future_only==nil)
            return observe()
        end
    end
    error('restore fixture did not finish')
end
local function inline_commit()
    local c = assert(runner.get_ctx())
    local saved = assert(KAG.load_game(38))
    assert(c._resume_index==saved.token_index and saved.token_index==3,
        'load entry changed the saved continuation')
    assert(c.f.before==1 and c.f.replayed==nil and c.f.future_only==nil)
    return observe()
end
local function inline_finish()
    for turn=1,80 do
        runner.update(0.016)
        local c = assert(runner.get_ctx())
        if c.waiting_input then runner.on_click() end
        if c.f.finished then
            assert(c.f.replayed==1 and c.f.branch=='restored' and c.f.future_only==nil)
            local marker=false
            for _, entry in ipairs(c.backlog or {}) do
                if entry.text=='RESTORED-MARKER' then marker=true end
            end
            assert(marker,'the saved visible continuation was skipped')
            return observe()
        end
    end
    error('inline continuation did not finish')
end
'''

PRODUCER = COMMON + r'''
assert(KAG.set_encryption_key(string.rep('K',32)))
assert(runner.start('tests/projects/u11_restore/call.ks'))
for turn=1,24 do runner.update(0) end
local c = assert(runner.get_ctx())
assert(c._executing_command=='wait' and c.f.i==1 and c.f.visits==0)
c.f.secret='must-stay-encrypted-u11'
c.f.nested={enabled=true,sequence={2,4,8},zero='L'..string.char(0)..'R'}
c.text_speed=50
c.reveal={total=6,elapsed=100,last_shown=2}
text_scene.add_text(c,'AB中文CD',40,80,{240,120,60,255},'u11',1.25,true,false,false,false)
c.text_state.opacity=180
c.text_state.reveal_chars=2
local saved = observe()
saves.save(c,{slot=36,thumbnail='u11-cold-restore'})
assert(c.tf.save_result=='ok',c.tf.save_error)
c.f.future_only=true
c.f.nested.sequence[1]=99
c.lf.owner='future'
c.tf.future_only=true
assert(saves.load(c,{slot=36}))
runner.update(0)
assert(runner.get_ctx().tf.future_only==nil)
local restored = observe()
assert(saved==restored,'native hot restore changed the saved value/control state')
local ending = finish()
assert(runner.start('tests/projects/u11_restore/inline.ks'))
local pending=false
for turn=1,80 do
    runner.update(0.016)
    local current=assert(runner.get_ctx())
    if current.waiting_input then runner.on_click() end
    if current._pendingRestore then pending=true; break end
end
assert(pending,'producer never executed its inline load')
assert(runner.update(0))
local boundary=inline_commit()
local replayed=inline_finish()
return saved..'\nU11_ENDING\n'..ending..'\nU11_INLINE_COMMIT\n'..boundary
    ..'\nU11_INLINE_DONE\n'..replayed
'''

CONSUMER = COMMON + r'''
assert(runner.start('tests/projects/u11_restore/base.ks'))
for turn=1,4 do runner.update(0) end
local bootstrap = assert(runner.get_ctx())
bootstrap.f.bootstrap_marker='old-session'
local old_co = bootstrap.co
assert(KAG.set_encryption_key(string.rep('X',32)))
assert(saves.load(bootstrap,{slot=36})==false)
assert(runner.get_ctx()==bootstrap and bootstrap.co==old_co)
assert(bootstrap.f.bootstrap_marker=='old-session' and not bootstrap.stop_flag)
assert(KAG.set_encryption_key(string.rep('K',32)))
assert(saves.load(bootstrap,{slot=36}))
runner.update(0)
local c = assert(runner.get_ctx())
assert(c.f.bootstrap_marker==nil and c.f.future_only==nil)
local restored = observe()
local ending = finish()
KAG.clear_encryption_key()
assert(saves.load(runner.get_ctx(),{slot=37}))
local legacy = assert(runner.get_ctx())
assert(legacy.f.route=='forest' and legacy.f.score==7)
assert(legacy.f.legacy_nested.sequence[3]==8)
assert(legacy.sf.profile_marker=='release-v1.0.1' and legacy.f.secret==nil)
assert(KAG.set_encryption_key(string.rep('K',32)))
assert(saves.load(legacy,{slot=38}))
local boundary=inline_commit()
local replayed=inline_finish()
return restored..'\nU11_ENDING\n'..ending..'\nU11_INLINE_COMMIT\n'..boundary
    ..'\nU11_INLINE_DONE\n'..replayed
'''


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_engine(executable: Path, resource_root: Path, code: str, name: str,
               evidence: Path | None) -> dict:
    requests = [{"id": 1, "method": "ping"},
                {"id": 2, "method": "eval", "code": code},
                {"id": 3, "method": "stop"}]
    payload = "".join(json.dumps(item, ensure_ascii=False) + "\n" for item in requests)
    process = subprocess.Popen(
        [str(executable), "--headless", "--resource-root", str(resource_root)],
        cwd=resource_root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    try:
        stdout, stderr = process.communicate(payload.encode("utf-8"), timeout=40)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate(timeout=10)
        if evidence:
            (evidence / f"{name}.stdout.log").write_bytes(stdout)
            (evidence / f"{name}.stderr.log").write_bytes(stderr)
        raise RuntimeError(f"{name} engine timed out") from None
    if evidence:
        (evidence / f"{name}.stdout.log").write_bytes(stdout)
        (evidence / f"{name}.stderr.log").write_bytes(stderr)
    responses = {}
    for line in stdout.splitlines():
        if not line.startswith(b"{"):
            continue
        response = json.loads(line)
        if isinstance(response, dict) and "id" in response:
            responses[response["id"]] = response
    if process.returncode != 0:
        raise RuntimeError(f"{name} engine exited {process.returncode}")
    if responses.get(2, {}).get("status") != "ok":
        raise RuntimeError(f"{name} restore failed: {responses.get(2)}")
    if responses.get(3, {}).get("result") != "ok":
        raise RuntimeError(f"{name} did not acknowledge stop")
    result = responses[2].get("result")
    if not isinstance(result, str) or "U11_ENDING" not in result:
        raise RuntimeError(f"{name} returned no state/continuation observation")
    return {"pid": process.pid, "exit_code": process.returncode, "observation": result}


def validate(executable: Path, evidence: Path | None) -> dict:
    scratch_parent = Path(tempfile.gettempdir()).resolve()
    with tempfile.TemporaryDirectory(prefix="caesura-u11-restore-", dir=scratch_parent) as directory:
        resource_root = Path(directory).resolve()
        if resource_root.parent != scratch_parent or not resource_root.name.startswith("caesura-u11-restore-"):
            raise RuntimeError("unexpected temporary resource root")
        shutil.copytree(ROOT / "scripts", resource_root / "scripts",
                        ignore=shutil.ignore_patterns("game_logic.lua", "__pycache__", "*.pyc"))
        shutil.copytree(ROOT / "assets/lang", resource_root / "assets/lang")
        for name in ("u11_restore", "u11_legacy_release"):
            shutil.copytree(ROOT / "tests/projects" / name, resource_root / "tests/projects" / name)
        (resource_root / "saves").mkdir()
        shutil.copyfile(ROOT / "tests/golden_saves/release-v1.0.1-kag-save.json",
                        resource_root / "saves/save_37.json")
        first = run_engine(executable, resource_root, PRODUCER, "producer", evidence)
        # communicate() has waited for the first process's exit before this read
        # and before starting the second process. Only the real disk slot survives.
        save_file = resource_root / "saves/save_36.json"
        raw = save_file.read_bytes()
        if not raw.startswith(b"CAES") or b"must-stay-encrypted-u11" in raw:
            raise RuntimeError("native save was not encrypted")
        saved_digest = digest(save_file)
        inline_file = resource_root / "saves/save_38.json"
        if not inline_file.read_bytes().startswith(b"CAES"):
            raise RuntimeError("inline save was not encrypted")
        inline_digest = digest(inline_file)
        if evidence:
            shutil.copyfile(save_file, evidence / "save_36.json")
            shutil.copyfile(inline_file, evidence / "save_38.json")
        second = run_engine(executable, resource_root, CONSUMER, "consumer", evidence)
        if first["observation"] != second["observation"]:
            raise RuntimeError("cold restore differs from the saved/hot-restored continuation")
        if digest(save_file) != saved_digest or digest(inline_file) != inline_digest:
            raise RuntimeError("reading the slot changed its bytes")
        return {"result": "PASS", "scope": "native values/control/text submissions; no pixel or audio claim",
                "binary_sha256": digest(executable), "save_sha256": saved_digest,
                "inline_save_sha256": inline_digest,
                "producer": first, "consumer": second}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--evidence-dir", type=Path)
    args = parser.parse_args()
    if args.evidence_dir:
        args.evidence_dir.mkdir(parents=True, exist_ok=True)
    result = validate(args.executable.resolve(), args.evidence_dir)
    if args.evidence_dir:
        (args.evidence_dir / "result.json").write_text(
            json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print("ALL NATIVE COLD SAVE RESTORE TESTS PASSED")


if __name__ == "__main__":
    main()
