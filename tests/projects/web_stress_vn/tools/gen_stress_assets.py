#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate Web stress-test assets for Caesura (AmeKAG) Track W4 (web_stress_vn).

Usage (paths are anchored to this script's location, CWD does not matter):

    cd tests/projects/web_stress_vn
    python tools/gen_stress_assets.py

Outputs (idempotent: re-running overwrites with byte-identical files):

    assets/stress/bg/bg001.png .. bg120.png
        256x256 solid-colour 8-bit RGB PNGs, one distinct colour each,
        every file <= 4 KB. bg001=red, bg002=green, bg003=blue; the rest are
        unique bright colours from a fixed-seed PRNG.
    assets/stress/bgm/tone440.wav / tone550.wav
        30 s sine tones (440 Hz / 550 Hz), 44100 Hz mono 16-bit PCM,
        ~2.6 MB each.

Implementation notes:
* Standard library only -- no PIL/numpy.
* PNG is hand-encoded with zlib+struct:
    signature b'\x89PNG\r\n\x1a\n'
    IHDR      width/height/8-bit/colour-type 2 (RGB)/no interlace
    IDAT      zlib-compressed raw scanlines, each prefixed with filter byte 0
    IEND      empty; every chunk framed as len(BE32)+type+payload+crc32
* WAV uses the stdlib wave module; samples are pure math.sin, so output is
  deterministic on a given platform.
* Determinism: colour RNG seeded with COLOR_SEED (fixed), so the palette and
  all file bytes are stable across runs.
"""

from __future__ import annotations

import math
import random
import struct
import wave
import zlib
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent          # tests/projects/web_stress_vn/
BG_DIR = BASE_DIR / "assets/stress/bg"
BGM_DIR = BASE_DIR / "assets/stress/bgm"

WIDTH = HEIGHT = 256
BG_COUNT = 120
MAX_PNG_BYTES = 4096                                       # keep each PNG <= 4 KB
COLOR_SEED = 20260823                                      # fixed => reproducible palette
FIRST_COLORS = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]     # red / green / blue

SAMPLE_RATE = 44100                                        # Hz
DURATION_S = 30
AMPLITUDE = 0.8                                            # fraction of int16 full scale
TONES_HZ = {"tone440.wav": 440.0, "tone550.wav": 550.0}


def png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    """Frame one PNG chunk: length(BE32) + type + payload + crc32(type+payload)."""
    return (
        struct.pack(">I", len(payload))
        + chunk_type
        + payload
        + struct.pack(">I", zlib.crc32(chunk_type + payload) & 0xFFFFFFFF)
    )


def encode_solid_png(width: int, height: int, rgb: tuple) -> bytes:
    """Minimal 8-bit truecolour PNG of a single solid colour."""
    # Each scanline: filter byte 0 followed by width * 3 RGB bytes.
    scanline = b"\x00" + bytes(rgb) * width
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", zlib.compress(scanline * height, 9))
        + png_chunk(b"IEND", b"")
    )


def build_palette(count: int) -> list:
    """Deterministic palette: red/green/blue first, then unique bright colours."""
    rng = random.Random(COLOR_SEED)
    palette = list(FIRST_COLORS)
    seen = set(palette)
    while len(palette) < count:
        c = (rng.randint(0, 255), rng.randint(0, 255), rng.randint(0, 255))
        if max(c) < 192:                # brightness floor: at least one hot channel
            continue
        if max(c) - min(c) < 64:        # skip dull near-grey colours
            continue
        if c in seen:                   # enforce "every image a different colour"
            continue
        seen.add(c)
        palette.append(c)
    return palette


def write_backgrounds() -> list:
    """Write bg001.png..bgNNN.png; returns list of file sizes in bytes."""
    BG_DIR.mkdir(parents=True, exist_ok=True)
    sizes = []
    for i, rgb in enumerate(build_palette(BG_COUNT), start=1):
        out = BG_DIR / ("bg%03d.png" % i)
        blob = encode_solid_png(WIDTH, HEIGHT, rgb)
        if len(blob) > MAX_PNG_BYTES:
            raise SystemExit("%s: %d bytes exceeds %d-byte limit"
                             % (out.as_posix(), len(blob), MAX_PNG_BYTES))
        out.write_bytes(blob)
        sizes.append(len(blob))
    return sizes


def write_tone_wav(path: Path, freq_hz: float) -> int:
    """Write one 30 s mono 16-bit PCM sine WAV via stdlib wave+math."""
    n_frames = SAMPLE_RATE * DURATION_S
    step = 2.0 * math.pi * freq_hz / SAMPLE_RATE
    amp = AMPLITUDE * 32767.0
    pack = struct.Struct("<h").pack
    frames = bytearray()
    for i in range(n_frames):
        frames += pack(int(amp * math.sin(step * i)))
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)               # 16-bit PCM
        w.setframerate(SAMPLE_RATE)
        w.writeframes(bytes(frames))
    return path.stat().st_size


def write_bgms() -> dict:
    """Write both tone WAVs; returns {filename: size_in_bytes}."""
    BGM_DIR.mkdir(parents=True, exist_ok=True)
    return {name: write_tone_wav(BGM_DIR / name, hz) for name, hz in TONES_HZ.items()}


def main() -> None:
    png_sizes = write_backgrounds()
    wav_sizes = write_bgms()
    print("[gen] backgrounds : %d PNGs (min %d B / max %d B / avg %d B)"
          % (len(png_sizes), min(png_sizes), max(png_sizes), sum(png_sizes) // len(png_sizes)))
    for name in sorted(wav_sizes):
        print("[gen] %-12s : %d bytes" % (name, wav_sizes[name]))
    total = sum(png_sizes) + sum(wav_sizes.values())
    print("[gen] total       : %d bytes across %d assets"
          % (total, len(png_sizes) + len(wav_sizes)))


if __name__ == "__main__":
    main()
