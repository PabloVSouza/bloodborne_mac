#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""JSON answers for the macOS launcher (launcher/app), from the same code run.sh uses.
  launcher_api.py check GAME_DIR              {"problem": text or null}
  launcher_api.py mods MODS_DIR CONFIG        {"mods": [{"name", "enabled"}]} in load order
  launcher_api.py patches PATCHES_DIR CONFIG  {"patches": [{"key", "name", "file", "author",
                                               "note", "default", "enabled"}]}
CONFIG: mods.json ({"order", "disabled"}) or patches.json ({"enabled", "disabled"}), as the
GTK launcher writes them."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import game_check  # noqa: E402
from mods import discover  # noqa: E402
from patches import external_patches  # noqa: E402


def read_json(path):
    try:
        return json.loads(Path(path).read_text())
    except (OSError, ValueError):
        return {}


def check(game_dir):
    path = Path(game_dir).expanduser()
    if not (path / "eboot.bin").is_file():
        return {"problem": "No eboot.bin in this folder: choose the CUSA03173 folder of your dump."}
    return {"problem": game_check.problem(path)}


def mods(directory, config):
    profile = read_json(config)
    available = discover(Path(directory).expanduser())
    order = list(dict.fromkeys(n for n in [*profile.get("order", []), *available] if n in available))
    disabled = set(profile.get("disabled", []))
    return {"mods": [{"name": n, "enabled": n not in disabled} for n in order]}


def patches(directory, config):
    profile = read_json(config)
    enabled, disabled = set(profile.get("enabled", [])), set(profile.get("disabled", []))
    rows = []
    for key, path, meta in external_patches(Path(directory).expanduser()):
        default = meta.get("isEnabled", "false").lower() == "true"
        rows.append({
            "key": key,
            "name": meta.get("Name") or key,
            "file": key.split("/", 1)[0],
            "author": meta.get("Author") or "",
            "note": (meta.get("Note") or "").replace("\\n", "\n"),
            "default": default,
            "enabled": key in enabled or (default and key not in disabled),
        })
    return {"patches": rows}


def main():
    command, args = sys.argv[1], sys.argv[2:]
    result = {"check": check, "mods": mods, "patches": patches}[command](*args)
    json.dump(result, sys.stdout, ensure_ascii=False)


if __name__ == "__main__":
    main()
