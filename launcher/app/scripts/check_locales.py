#!/usr/bin/env python3
"""Checks every locale in src/i18n/locales against en.json: missing and extra keys, and
interpolations ({{name}}) a translation lost. Plural variants (_one, _few, ...) are free."""
import json, re, sys
from pathlib import Path

PLURAL = re.compile(r"_(zero|one|two|few|many|other)$")

def flat(d, prefix=""):
    out = {}
    for k, v in d.items():
        key = f"{prefix}{k}"
        out.update(flat(v, key + ".") if isinstance(v, dict) else {key: v})
    return out

root = Path(__file__).resolve().parent.parent / "src/i18n/locales"
ref = flat(json.loads((root / "en.json").read_text()))
base = {PLURAL.sub("", k) for k in ref}
failed = False
for path in sorted(root.glob("*.json")):
    if path.name == "en.json":
        continue
    loc = flat(json.loads(path.read_text()))
    missing = sorted(base - {PLURAL.sub("", k) for k in loc})
    extra = sorted(k for k in loc if PLURAL.sub("", k) not in base)
    lost = sorted(k for k, v in loc.items()
                  if set(re.findall(r"{{\w+}}", ref.get(PLURAL.sub("", k), ref.get(k, "")))) - set(re.findall(r"{{\w+}}", v)))
    status = "ok" if not (missing or extra or lost) else "PROBLEMS"
    print(f"{path.name:12} {len(loc):4} keys  {status}")
    for label, items in (("missing", missing), ("extra", extra), ("lost interpolation", lost)):
        if items:
            failed = True
            print(f"   {label}: {', '.join(items[:12])}{' …' if len(items) > 12 else ''}")
sys.exit(1 if failed else 0)
