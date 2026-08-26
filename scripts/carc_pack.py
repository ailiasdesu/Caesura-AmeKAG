#!/usr/bin/env python3
"""
Caesura (AmeKAG) — CARC & Delta CARC Python Validator and Tooling
Provides binary header verification, integrity checking, and command forwarding.
"""

import sys
import os
import struct
import subprocess
import argparse

CARC_MAGIC = 0x43524143  # 'CARC' in little-endian
CARC_VERSION = 1
DELTA_MAGIC = 0x4341524B  # 'CARK' in little-endian
DELTA_VERSION = 2

HEADER_SIZE_CARC = 64
TRAILER_SIZE_CARC = 96  # 64-byte signature + 32-byte public key
ENTRY_SIZE_CARC = 116

HEADER_SIZE_DELTA = 80
MIN_DELTA_SIZE = 140  # 80 header + 32 key + 12 nonce + 16 tag


def find_carc_pack_binary():
    candidates = [
        os.path.join("bin", "Debug", "carc_pack.exe"),
        os.path.join("bin", "Release", "carc_pack.exe"),
        os.path.join("bin", "carc_pack.exe"),
        os.path.join("bin", "carc_pack"),
        os.path.join("build", "tools", "carc_pack", "Debug", "carc_pack.exe"),
        os.path.join("build", "tools", "carc_pack", "Release", "carc_pack.exe"),
        os.path.join("build", "bin", "Debug", "carc_pack.exe"),
        os.path.join("build", "bin", "carc_pack"),
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return None


def verify_carc(file_path):
    """
    Verify CARC archive structure, header magic, version, bounds, and file index.
    Returns (is_valid: bool, message: str).
    """
    if not os.path.exists(file_path):
        return False, f"File not found: {file_path}"

    file_size = os.path.getsize(file_path)
    if file_size < HEADER_SIZE_CARC:
        return False, f"File too small for CARC header: {file_size} bytes (min {HEADER_SIZE_CARC})"

    with open(file_path, "rb") as f:
        header_bytes = f.read(HEADER_SIZE_CARC)
        if len(header_bytes) < HEADER_SIZE_CARC:
            return False, "Failed to read 64-byte CARC header"

        magic, version, content_offset, content_size, index_offset, index_size, num_files = struct.unpack(
            "<IIQQQQI", header_bytes[:44]
        )

        if magic != CARC_MAGIC:
            return False, f"Invalid CARC magic: 0x{magic:08X} (expected 0x{CARC_MAGIC:08X})"

        if version != CARC_VERSION:
            return False, f"Unsupported CARC version: {version} (expected {CARC_VERSION})"

        if content_offset < HEADER_SIZE_CARC:
            return False, f"Invalid content offset: {content_offset} < header size"

        if index_offset < content_offset:
            return False, f"Invalid index offset: {index_offset} < content offset {content_offset}"

        min_signed_size = index_offset + index_size
        if file_size < min_signed_size:
            return False, f"File truncated: {file_size} < index end {min_signed_size}"

        expected_min_index = num_files * ENTRY_SIZE_CARC
        if index_size < expected_min_index:
            return False, f"Index size {index_size} insufficient for {num_files} entries ({expected_min_index} required)"

    return True, f"OK: Valid CARC v{version} archive ({num_files} files, {file_size} bytes)"


def verify_delta_carc(file_path):
    """
    Verify Delta CARC archive structure, header magic, version, and bounds.
    Returns (is_valid: bool, message: str).
    """
    if not os.path.exists(file_path):
        return False, f"File not found: {file_path}"

    file_size = os.path.getsize(file_path)
    if file_size < MIN_DELTA_SIZE:
        return False, f"Delta file too small: {file_size} bytes (min {MIN_DELTA_SIZE})"

    with open(file_path, "rb") as f:
        header_bytes = f.read(HEADER_SIZE_DELTA)
        if len(header_bytes) < HEADER_SIZE_DELTA:
            return False, "Failed to read 80-byte Delta header"

        magic, version = struct.unpack("<II", header_bytes[:8])
        source_sha = header_bytes[8:40].hex()
        target_sha = header_bytes[40:72].hex()
        entry_count = struct.unpack("<I", header_bytes[72:76])[0]

        if magic != DELTA_MAGIC:
            return False, f"Invalid Delta magic: 0x{magic:08X} (expected 0x{DELTA_MAGIC:08X})"

        if version != DELTA_VERSION:
            return False, f"Unsupported Delta version: {version} (expected {DELTA_VERSION})"

    return True, (
        f"OK: Valid Delta CARC v{version} patch ({entry_count} entries, "
        f"source SHA: {source_sha[:8]}..., target SHA: {target_sha[:8]}...)"
    )


def info_carc(file_path):
    """Extract metadata information from a CARC or Delta CARC file."""
    if not os.path.exists(file_path):
        return None

    with open(file_path, "rb") as f:
        magic = struct.unpack("<I", f.read(4))[0]

    if magic == CARC_MAGIC:
        valid, msg = verify_carc(file_path)
        with open(file_path, "rb") as f:
            header = f.read(HEADER_SIZE_CARC)
            _, ver, coff, csize, ioff, isize, nfiles = struct.unpack("<IIQQQQI", header[:44])
        return {
            "type": "CARC",
            "valid": valid,
            "version": ver,
            "num_files": nfiles,
            "content_size": csize,
            "index_size": isize,
            "file_size": os.path.getsize(file_path),
            "message": msg,
        }
    elif magic == DELTA_MAGIC:
        valid, msg = verify_delta_carc(file_path)
        with open(file_path, "rb") as f:
            header = f.read(HEADER_SIZE_DELTA)
            _, ver = struct.unpack("<II", header[:8])
            ssha = header[8:40].hex()
            tsha = header[40:72].hex()
            entries = struct.unpack("<I", header[72:76])[0]
        return {
            "type": "DeltaCARC",
            "valid": valid,
            "version": ver,
            "entries": entries,
            "source_sha": ssha,
            "target_sha": tsha,
            "file_size": os.path.getsize(file_path),
            "message": msg,
        }
    else:
        return {"type": "Unknown", "valid": False, "magic": hex(magic)}


def main():
    parser = argparse.ArgumentParser(description="Caesura CARC and Delta CARC Tool & Validator")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # verify
    p_ver = subparsers.add_parser("verify", help="Verify a CARC archive")
    p_ver.add_argument("path", help="Path to .carc file")

    # verify-delta
    p_verd = subparsers.add_parser("verify-delta", help="Verify a Delta CARC patch")
    p_verd.add_argument("path", help="Path to delta .carc file")

    # info
    p_info = subparsers.add_parser("info", help="Inspect CARC / Delta CARC metadata")
    p_info.add_argument("path", help="Path to archive file")

    # delta
    p_delta = subparsers.add_parser("delta", help="Generate differential patch")
    p_delta.add_argument("old_carc", help="Path to base .carc")
    p_delta.add_argument("new_carc", help="Path to new .carc")
    p_delta.add_argument("delta_carc", help="Path to output delta .carc")

    # apply
    p_apply = subparsers.add_parser("apply", help="Apply differential patch")
    p_apply.add_argument("base_carc", help="Path to base .carc")
    p_apply.add_argument("delta_carc", help="Path to delta .carc")
    p_apply.add_argument("output_carc", help="Path to output .carc")

    # pack
    p_pack = subparsers.add_parser("pack", help="Pack a directory into CARC")
    p_pack.add_argument("input_dir", help="Directory to pack")
    p_pack.add_argument("output_carc", help="Output .carc path")
    p_pack.add_argument("pub_key", nargs="?", default="")
    p_pack.add_argument("priv_key", nargs="?", default="")

    # list
    p_list = subparsers.add_parser("list", help="List files in CARC")
    p_list.add_argument("carc_path", help="Path to .carc file")
    p_list.add_argument("pub_key", nargs="?", default="")

    # extract
    p_extract = subparsers.add_parser("extract", help="Extract files from CARC")
    p_extract.add_argument("carc_path", help="Path to .carc file")
    p_extract.add_argument("out_dir", help="Output directory")
    p_extract.add_argument("pub_key", nargs="?", default="")
    p_extract.add_argument("--path", dest="only_path", default="")

    args = parser.parse_args()

    if args.command == "verify":
        valid, msg = verify_carc(args.path)
        print(msg)
        sys.exit(0 if valid else 1)

    elif args.command == "verify-delta":
        valid, msg = verify_delta_carc(args.path)
        print(msg)
        sys.exit(0 if valid else 1)

    elif args.command == "info":
        res = info_carc(args.path)
        if not res:
            print(f"Error: cannot read {args.path}", file=sys.stderr)
            sys.exit(1)
        print(f"Archive Type : {res.get('type')}")
        print(f"Valid        : {res.get('valid')}")
        for k, v in res.items():
            if k not in ("type", "valid", "message"):
                print(f"{k:<12} : {v}")
        print(f"Status       : {res.get('message')}")
        sys.exit(0 if res.get("valid") else 1)

    else:
        # Forward command to native carc_pack executable
        bin_path = find_carc_pack_binary()
        if not bin_path:
            print("Error: carc_pack executable not found. Please build the project first.", file=sys.stderr)
            sys.exit(1)

        cmd = [bin_path, args.command]
        if args.command == "delta":
            cmd.extend([args.old_carc, args.new_carc, args.delta_carc])
        elif args.command == "apply":
            cmd.extend([args.base_carc, args.delta_carc, args.output_carc])
        elif args.command == "pack":
            cmd.extend([args.input_dir, args.output_carc])
            if args.pub_key:
                cmd.append(args.pub_key)
            if args.priv_key:
                cmd.append(args.priv_key)
        elif args.command == "list":
            cmd.append(args.carc_path)
            if args.pub_key:
                cmd.append(args.pub_key)
        elif args.command == "extract":
            cmd.extend([args.carc_path, args.out_dir])
            if args.pub_key:
                cmd.append(args.pub_key)
            if args.only_path:
                cmd.extend(["--path", args.only_path])

        res = subprocess.run(cmd)
        sys.exit(res.returncode)


if __name__ == "__main__":
    main()
