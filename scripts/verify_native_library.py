#!/usr/bin/env python3
"""Validate an Android ARM64 native Levi mod library before packaging."""
from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_native_library.py <mod.so>", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    if not path.is_file():
        print(f"library not found: {path}", file=sys.stderr)
        return 1

    data = path.read_bytes()[:64]
    if len(data) < 64 or data[:4] != b"\x7fELF":
        print("not an ELF shared library", file=sys.stderr)
        return 1
    if data[4] != 2 or data[5] != 1:
        print("expected ELF64 little-endian", file=sys.stderr)
        return 1
    e_type, e_machine = struct.unpack_from("<HH", data, 16)
    if e_type != 3 or e_machine != 183:
        print(f"expected AArch64 ET_DYN; got type={e_type} machine={e_machine}", file=sys.stderr)
        return 1

    try:
        symbols = subprocess.run(
            ["readelf", "--dyn-syms", "--wide", str(path)],
            check=True, capture_output=True, text=True,
        ).stdout
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"failed to inspect dynamic symbols: {exc}", file=sys.stderr)
        return 1

    found = False
    for line in symbols.splitlines():
        if "PLGetModRegistration" not in line:
            continue
        fields = line.split()
        if len(fields) >= 8 and fields[6] != "UND" and fields[4] in {"GLOBAL", "WEAK"}:
            found = True
            break
    if not found:
        print("missing exported PLGetModRegistration; preloader cannot activate this mod", file=sys.stderr)
        return 1

    print(f"verified Android arm64 ELF and PLGetModRegistration: {path} ({path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
