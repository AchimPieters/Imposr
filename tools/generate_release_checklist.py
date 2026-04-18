#!/usr/bin/env python3
"""
Generate a markdown release checklist from sdk smoke evidence JSON.

Usage:
  python3 tools/generate_release_checklist.py --evidence docs/sdk_smoke_evidence.json --out docs/RELEASE_CHECKLIST.md
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


CHECKS = [
    ("plugin_load", "Plugin load"),
    ("menu_actions", "Menu actions"),
    ("preset_validate", "Preset validate"),
    ("preset_preview", "Preset preview"),
    ("preset_run_bundle", "Preset run bundle"),
    ("runtime_quality_gate", "Runtime quality gate"),
]


def _is_pass(value: str) -> bool:
    return value.strip().lower() in {"pass", "ok", "true", "1"}


def _mark(value: str) -> str:
    return "✅" if _is_pass(value) else "⬜"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    evidence_path = Path(args.evidence)
    data = json.loads(evidence_path.read_text(encoding="utf-8"))
    rows = data.get("rows", [])

    out = []
    out.append("# SDK Release Checklist\n")
    out.append("\n")
    out.append("| OS | CPU | Status | Checks |\n")
    out.append("|---|---|---|---|\n")

    for row in rows:
        os_name = row.get("os", "")
        cpu = row.get("cpu", "")
        checks = []
        passed = True
        for key, label in CHECKS:
            value = str(row.get(key, ""))
            mark = _mark(value)
            checks.append(f"{mark} {label}")
            if mark != "✅":
                passed = False
        status = "PASS" if passed else "PENDING"
        out.append(f"| {os_name} | {cpu} | {status} | {'<br>'.join(checks)} |\n")

    out_path = Path(args.out)
    out_path.write_text("".join(out), encoding="utf-8")
    print(f"Wrote checklist: {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

