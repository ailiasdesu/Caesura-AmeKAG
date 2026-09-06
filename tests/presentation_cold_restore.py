"""Compare real Engine D3D11 pages and SoLoud PCM across exited/new processes.

The consumer gets immutable project resources and the producer's encrypted disk
slot. Reference pixels, PCM, and observations are read only by this parent.
NULLDRIVER output proves software mixing, not a physical sound-card output.
"""
from __future__ import annotations

import argparse
from array import array
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import wave

ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT, CHANNELS, RATE = 640, 360, 2, 48000
TONE_FRAMES, PREFIX_FRAMES, SEGMENT_FRAMES = 32768, 8192, 65536
PCM_TOLERANCE = 1e-5
SLOT = "save_38.json"
MARKER = b"u11-cold-presentation-must-be-encrypted"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_bitmap(path: Path, width: int, height: int, variant: int) -> None:
    stride = (width * 3 + 3) & ~3
    pixels = bytearray()
    for y in range(height - 1, -1, -1):
        row = bytearray()
        for x in range(width):
            if variant == 0:
                color = (22 + x * 2, 45 + y * 3, 95 + ((x // 8 + y // 6) % 2) * 65)
            elif variant == 1:
                color = (30 + (x % 8) * 20, 155 + y % 80, 70 + (y % 8) * 20)
            else:
                color = (190 + x % 50, 12 + y % 45, 20 + ((x + y) % 25))
            row.extend((color[2], color[1], color[0]))
        pixels.extend(row + bytes(stride - len(row)))
    header = struct.pack("<2sIHHI", b"BM", 54 + len(pixels), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, len(pixels), 0, 0, 0, 0)
    path.write_bytes(header + info + pixels)


def make_wave(path: Path, seed: int) -> None:
    samples = bytearray()
    state = seed
    # Independent successive LCG samples make offsets and channel swaps visible.
    # Amplitude is low enough that fixed mixer gains do not clip the signal.
    for _ in range(TONE_FRAMES):
        for _channel in range(CHANNELS):
            state = (1664525 * state + 1013904223) & 0xFFFFFFFF
            sample = (((state >> 16) & 0xFFFF) - 32768) // 5
            samples.extend(struct.pack("<h", sample))
    with wave.open(str(path), "wb") as output:
        output.setparams((CHANNELS, 2, RATE, TONE_FRAMES, "NONE", "not compressed"))
        output.writeframes(samples)


def seed_resources(destination: Path) -> dict[str, str]:
    shutil.copytree(ROOT / "scripts", destination / "scripts",
                    ignore=shutil.ignore_patterns("game_logic.lua", "__pycache__", "*.pyc"))
    shutil.copytree(ROOT / "assets/lang", destination / "assets/lang")
    font = destination / "assets/fonts/NotoSansCJKsc-Regular.otf"
    font.parent.mkdir(parents=True)
    shutil.copyfile(ROOT / "assets/fonts" / font.name, font)
    project = destination / "tests/projects/u11_restore"
    project.mkdir(parents=True)
    for filename in ("base.ks", "presentation_fixture.lua"):
        shutil.copyfile(ROOT / "tests/projects/u11_restore" / filename, project / filename)
    images = destination / "assets/u11"
    images.mkdir()
    make_bitmap(images / "background.bmp", 64, 36, 0)
    make_bitmap(images / "foreground.bmp", 64, 64, 1)
    make_bitmap(images / "changed.bmp", 64, 36, 2)
    make_wave(images / "tone.wav", 0x125ABC98)
    make_wave(images / "changed.wav", 0x9D18A036)
    (destination / "settings").mkdir()
    (destination / "settings/window.lua").write_text(
        "return {window_width=640,window_height=360,fullscreen=false}\n", encoding="utf-8")
    (destination / "saves").mkdir()
    return {str(path.relative_to(destination)).replace("\\", "/"): digest(path)
            for path in destination.rglob("*") if path.is_file()}


def run_probe(executable: Path, resource_root: Path, output: Path, role: str) -> dict:
    command = [str(executable), "--role", role, "--resource-root", str(resource_root),
               "--output-dir", str(output)]
    started = time.monotonic_ns()
    started_utc = datetime.now(timezone.utc).isoformat()
    process = subprocess.Popen(command, cwd=resource_root, stdin=subprocess.DEVNULL,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    timed_out = False
    try:
        stdout, stderr = process.communicate(timeout=120)
    except subprocess.TimeoutExpired:
        timed_out = True
        process.kill()  # Only this owned child, followed by a mandatory wait.
        stdout, stderr = process.communicate(timeout=10)
    finished = time.monotonic_ns()
    (output / "stdout.log").write_bytes(stdout)
    (output / "stderr.log").write_bytes(stderr)
    observation_path = output / "result.json"
    observation = json.loads(observation_path.read_text(encoding="utf-8")) if observation_path.exists() else None
    return {"command": command, "cwd": str(resource_root), "pid": process.pid,
            "started_utc": started_utc, "started_ns": started, "finished_ns": finished,
            "exit_code": process.returncode, "timed_out": timed_out, "observation": observation}


def require_success(child: dict) -> None:
    observation = child["observation"]
    if child["timed_out"] or child["exit_code"] != 0 or not observation:
        raise RuntimeError(f"{child['command'][2]} probe failed; see its stdout/stderr and result.json")
    if observation.get("status") != "PASS" or observation.get("pid") != child["pid"]:
        raise RuntimeError("Probe did not report success from its actual OS process")


def read_page(directory: Path, name: str) -> bytes:
    png = directory / f"{name}.png"
    if not png.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"):
        raise RuntimeError(f"Missing real PNG observation: {name}")
    value = (directory / f"{name}.rgba").read_bytes()
    if len(value) != WIDTH * HEIGHT * 4:
        raise RuntimeError(f"Wrong full-page RGBA size: {name}")
    return value


def pixel_difference(left: bytes, right: bytes, region: tuple[int, int, int, int] | None = None) -> dict:
    x, y, width, height = region or (0, 0, WIDTH, HEIGHT)
    changed, maximum = 0, 0
    for row in range(y, y + height):
        for column in range(x, x + width):
            offset = (row * WIDTH + column) * 4
            a, b = left[offset:offset + 4], right[offset:offset + 4]
            if a != b:
                changed += 1
                maximum = max(maximum, max(abs(c - d) for c, d in zip(a, b)))
    return {"changed_pixels": changed, "max_channel_difference": maximum}


def read_pcm(directory: Path, name: str) -> array:
    raw = (directory / f"{name}.f32").read_bytes()
    if len(raw) != SEGMENT_FRAMES * CHANNELS * 4:
        raise RuntimeError(f"Wrong PCM length: {name}")
    values = array("f")
    values.frombytes(raw)
    if sys.byteorder != "little":
        values.byteswap()
    if not all(math.isfinite(value) for value in values):
        raise RuntimeError(f"Non-finite PCM observation: {name}")
    return values


def pcm_energy(values: array) -> dict:
    rms = math.sqrt(sum(value * value for value in values) / len(values))
    tail = values[-2048 * CHANNELS:]
    channel_delta = max(abs(values[i] - values[i + 1]) for i in range(0, len(values), CHANNELS))
    return {"rms": rms, "peak": max(abs(value) for value in values),
            "tail_rms": math.sqrt(sum(value * value for value in tail) / len(tail)),
            "channel_max_difference": channel_delta}


def pcm_difference(left: array, right: array) -> dict:
    maximum, changed, first = 0.0, 0, None
    for index, (a, b) in enumerate(zip(left, right)):
        difference = abs(a - b)
        maximum = max(maximum, difference)
        if difference > PCM_TOLERANCE:
            changed += 1
            if first is None:
                first = {"sample_index": index, "reference": a, "observed": b}
    return {"max_absolute_error": maximum, "samples_over_tolerance": changed, "first_difference": first}


def compare(producer: dict, consumer: dict, first: Path, second: Path) -> dict:
    failures: list[str] = []
    pixels: dict = {}
    pcm: dict = {}

    def check(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    saved = read_page(first, "saved-a")
    for name, directory in (("saved-b", first), ("hot", first), ("cold", second)):
        pixels[name] = pixel_difference(saved, read_page(directory, name))
        check(pixels[name]["changed_pixels"] == 0, f"Full-page pixels differ: {name}")
    for name, directory, minimum in (("blank", first, 10000), ("changed", first, 5000),
                                      ("bootstrap", second, 5000)):
        pixels[name] = pixel_difference(saved, read_page(directory, name))
        check(pixels[name]["changed_pixels"] > minimum, f"Pixel nonempty/change control ineffective: {name}")
    pixels["text-hidden"] = pixel_difference(saved, read_page(first, "text-hidden"), (24, 228, 588, 105))
    check(pixels["text-hidden"]["changed_pixels"] > 80, "Text region did not actually render/change")

    reference = read_pcm(first, "continuous")
    reference_energy = pcm_energy(reference)
    pcm["continuous"] = {"energy": reference_energy}
    for name, directory in (("hot", first), ("cold", second)):
        values = read_pcm(directory, name)
        pcm[name] = {"difference": pcm_difference(reference, values), "energy": pcm_energy(values)}
        check(pcm[name]["difference"]["max_absolute_error"] <= PCM_TOLERANCE,
              f"PCM differs from continuous playback: {name}")
    for name in ("continuous", "hot", "cold"):
        energy = pcm[name]["energy"]
        check(energy["rms"] > 0.001 and energy["peak"] > 0.01, f"PCM is silent: {name}")
        check(energy["tail_rms"] > 0.001, f"No real loop output beyond EOF: {name}")
        check(energy["channel_max_difference"] > 0.01, f"Stereo control ineffective: {name}")
    for name, directory in (("wrong-position", first), ("wrong-gain", first),
                            ("changed", first), ("bootstrap", second)):
        pcm[name] = pcm_difference(reference, read_pcm(directory, name))
        check(pcm[name]["max_absolute_error"] > 0.001, f"PCM change control ineffective: {name}")

    before = producer["audio"]["continuous"]["before"]
    check(before == producer["saved_audio"], "Audio advanced between save observation and continuous reference")
    check(abs(before["position"] - PREFIX_FRAMES / RATE) <= 1 / RATE, "Saved cursor is not the measured nonzero prefix")
    check(before["path"] == "assets/u11/tone.wav" and before["gain"] == 0.625 and before["looping"] is True,
          "Producer did not save the required BGM path/gain/loop")
    for name, observation in (("hot", producer), ("cold", consumer)):
        segment = observation["audio"][name]
        restored = segment["before"]
        check(abs(restored["position"] - before["position"]) <= 1 / RATE, f"Wrong restored audio cursor: {name}")
        for field in ("path", "gain", "looping"):
            check(restored[field] == before[field], f"Wrong restored BGM {field}: {name}")
        check(segment["after"]["path"] == before["path"] and segment["after"]["looping"] is True,
              f"Looping BGM did not remain alive: {name}")
        check(abs(segment["after"]["position"] - producer["audio"]["continuous"]["after"]["position"]) <= 1 / RATE,
              f"Audio cursor differs after equal mixed frames: {name}")
        check(not segment["voice_playing"] and not segment["se_playing"], f"Old session audio remains: {name}")
    check(PREFIX_FRAMES + SEGMENT_FRAMES > TONE_FRAMES * 2, "Fixture must observe two EOF crossings")
    check(producer["host"] == consumer["host"], "Producer/consumer runtime configuration differs")
    return {"failures": failures, "pixels": pixels, "pcm": pcm,
            "pcm_tolerance": PCM_TOLERANCE, "loop_fixture_frames": TONE_FRAMES}


def checked_scratch(path: Path, parent: Path) -> None:
    resolved = path.resolve()
    if resolved.parent != parent or not resolved.name.startswith("caesura-u11-presentation-") or path.is_symlink():
        raise RuntimeError("Temporary path escaped the owned scratch directory")


def validate(executable: Path, evidence: Path) -> dict:
    scratch_parent = Path(tempfile.gettempdir()).resolve()
    scratch = Path(tempfile.mkdtemp(prefix="caesura-u11-presentation-", dir=scratch_parent))
    checked_scratch(scratch, scratch_parent)
    result: dict = {"status": "RUNNING", "scope": "real Engine cold D3D11 pixels and SoLoud offline PCM; no sound-card claim"}
    try:
        result["binary_sha256"] = digest(executable)
        seed = scratch / "resources"
        seed.mkdir()
        result["resource_sha256"] = seed_resources(seed)
        roots, outputs = {}, {}
        for role in ("producer", "consumer"):
            roots[role], outputs[role] = scratch / f"{role}-root", scratch / f"{role}-output"
            shutil.copytree(seed, roots[role])
            outputs[role].mkdir()
        first = run_probe(executable, roots["producer"], outputs["producer"], "producer")
        result["producer"] = first
        require_success(first)
        slot = roots["producer"] / "saves" / SLOT
        data = slot.read_bytes()
        if not data.startswith(b"CAES") or MARKER in data:
            raise RuntimeError("Producer did not write a real encrypted fixture slot")
        result["save_sha256"] = digest(slot)
        shutil.copyfile(slot, evidence / SLOT)
        consumer_slot = roots["consumer"] / "saves" / SLOT
        shutil.copyfile(slot, consumer_slot)
        # Producer has exited. Only this disk slot is copied, never observations.
        second = run_probe(executable, roots["consumer"], outputs["consumer"], "consumer")
        result["consumer"] = second
        require_success(second)
        # Each run_probe starts and waits for its own Popen child. A monotonic
        # clock may report equal adjacent ticks, and Windows may reuse a PID.
        if first["finished_ns"] > second["started_ns"]:
            raise RuntimeError("Producer and consumer process lifetimes overlap")
        if digest(slot) != result["save_sha256"] or digest(consumer_slot) != result["save_sha256"]:
            raise RuntimeError("Loading the fixture changed a disk slot")
        for role, root in roots.items():
            for relative, expected in result["resource_sha256"].items():
                if digest(root / relative) != expected:
                    raise RuntimeError(f"Immutable project input changed: {role}/{relative}")
        result["comparison"] = compare(first["observation"], second["observation"], outputs["producer"], outputs["consumer"])
        if result["comparison"]["failures"]:
            raise RuntimeError("; ".join(result["comparison"]["failures"]))
        result["status"] = "PASS"
    except Exception as error:
        result["status"] = "FAIL"
        result["error"] = str(error)
    finally:
        # Preserve failure observations before deleting only the verified root.
        try:
            for role in ("producer", "consumer"):
                output = scratch / f"{role}-output"
                if output.exists():
                    shutil.copytree(output, evidence / role)
            (evidence / "result.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        finally:
            checked_scratch(scratch, scratch_parent)
            shutil.rmtree(scratch)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--evidence-dir", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    if os.name != "nt":
        parser.error("This evidence lane requires Windows and real D3D11")
    evidence = args.evidence_dir or ROOT / "artifacts/validation/u11-restore" / (
        "presentation-cold-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S") + f"-{os.getpid()}")
    evidence = evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    if any(evidence.iterdir()):
        parser.error("Evidence directory must be empty to prevent stale observations")
    result = validate(executable, evidence)
    if result["status"] != "PASS":
        print(f"COLD PRESENTATION RESTORE FAILED: {result.get('error')}", file=sys.stderr)
        print(f"Evidence: {evidence}", file=sys.stderr)
        return 1
    print("ALL NATIVE COLD PRESENTATION TESTS PASSED")
    print(f"Evidence: {evidence}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
