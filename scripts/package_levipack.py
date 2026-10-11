#!/usr/bin/env python3
"""Package OffhandQoL as a LeviLaunchroid .levipack using BedrockTools conventions."""

from __future__ import annotations

import argparse
import json
import sys
import zipfile
from pathlib import Path


def create_manifest(name: str, author: str, description: str, version: str, entry: str) -> dict[str, object]:
    return {
        "type": "preload-native",
        "name": name,
        "author": author,
        "description": description,
        "version": version,
        "entry": entry,
        "icon": "icon.png",
        "overwrite_files": ["icon.png", "resources/minecraft.ttf"],
        "overwrite_folders": ["resources/minecraft_resource_packs"],
    }


def package(library: Path, icon: Path, font: Path, resource_pack: Path,
            name: str, author: str, description: str, version: str, output: Path) -> None:
    for required in (library, icon, font):
        if not required.is_file():
            raise FileNotFoundError(f"Required package input is missing: {required}")
    if not resource_pack.is_dir():
        raise FileNotFoundError(f"Resource pack directory is missing: {resource_pack}")

    pack_manifest_path = resource_pack / "manifest.json"
    ui_path = resource_pack / "ui" / "hud_screen.json"
    for required in (pack_manifest_path, ui_path):
        if not required.is_file():
            raise FileNotFoundError(f"Required resource-pack file is missing: {required}")
    json.loads(pack_manifest_path.read_text(encoding="utf-8"))
    json.loads(ui_path.read_text(encoding="utf-8"))

    manifest = create_manifest(name, author, description, version, library.name)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)

    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        archive.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
        archive.write(library, library.name)
        archive.write(icon, "icon.png")
        archive.write(font, "resources/minecraft.ttf")
        pack_prefix = "resources/minecraft_resource_packs/offhand_android"
        for item in sorted(resource_pack.rglob("*")):
            if item.is_file():
                rel = item.relative_to(resource_pack).as_posix()
                archive.write(item, f"{pack_prefix}/{rel}")

    verify_package(output)


def verify_package(path: Path) -> None:
    with zipfile.ZipFile(path, "r") as archive:
        bad_member = archive.testzip()
        if bad_member:
            raise RuntimeError(f"Corrupt zip member: {bad_member}")
        names = set(archive.namelist())
        manifest = json.loads(archive.read("manifest.json"))
        entry = manifest.get("entry")
        required = {
            "manifest.json", "icon.png", "resources/minecraft.ttf",
            "resources/minecraft_resource_packs/offhand_android/manifest.json",
            "resources/minecraft_resource_packs/offhand_android/ui/hud_screen.json",
        }
        if not required.issubset(names):
            raise RuntimeError(f"Package is missing required entries: {sorted(required - names)}")
        if not isinstance(entry, str) or entry not in names:
            raise RuntimeError(f"Manifest entry does not exist in archive: {entry!r}")
        if manifest.get("type") != "preload-native":
            raise RuntimeError("Invalid native-mod manifest type")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--icon", required=True, type=Path)
    parser.add_argument("--font", required=True, type=Path)
    parser.add_argument("--resource-pack", required=True, type=Path)
    parser.add_argument("--name", required=True)
    parser.add_argument("--author", required=True)
    parser.add_argument("--description", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    try:
        package(
            args.library.resolve(), args.icon.resolve(), args.font.resolve(),
            args.resource_pack.resolve(), args.name, args.author, args.description,
            args.version, args.output.resolve(),
        )
    except Exception as error:  # report one concise failure line to CI
        print(f"levipack packaging failed: {error}", file=sys.stderr)
        return 1
    print(f"packaged and verified: {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())