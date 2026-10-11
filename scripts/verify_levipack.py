#!/usr/bin/env python3
"""Basic structural/manifest verification for a built .levipack."""

from __future__ import annotations

import json
import sys
import zipfile
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_levipack.py <package.levipack>", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    if not path.is_file():
        print(f"package not found: {path}", file=sys.stderr)
        return 1
    try:
        with zipfile.ZipFile(path) as archive:
            bad = archive.testzip()
            if bad:
                raise ValueError(f"corrupt zip entry: {bad}")
            names = set(archive.namelist())
            manifest = json.loads(archive.read("manifest.json"))
            entry = manifest["entry"]
            required = {
                entry,
                "manifest.json",
                "icon.png",
                "resources/minecraft.ttf",
                "resources/minecraft_resource_packs/offhand_android/manifest.json",
                "resources/minecraft_resource_packs/offhand_android/ui/hud_screen.json",
            }
            missing = required - names
            if missing:
                raise ValueError(f"missing required entries: {sorted(missing)}")
            if manifest.get("type") != "preload-native":
                raise ValueError("manifest type is not preload-native")
            if "offhand.android.place" in "\n".join(names):
                raise ValueError("legacy interact/place button unexpectedly present in filenames")
            for name in names:
                if not name.endswith((".json", ".md", ".txt")):
                    continue
                payload = archive.read(name)
                if b"offhand.android.place" in payload or b"offhand.android.interact" in payload:
                    raise ValueError(f"legacy interact/place button reference found in {name}")
            if not isinstance(manifest.get("overwrite_folders"), list) or "resources/minecraft_resource_packs" not in manifest["overwrite_folders"]:
                raise ValueError("manifest does not install the resource pack folder")
    except Exception as error:
        print(f"verification failed: {error}", file=sys.stderr)
        return 1
    print(f"verified {path}: {len(names)} entries, entry={entry}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())