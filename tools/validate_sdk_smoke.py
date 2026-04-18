#!/usr/bin/env python3
"""
Validate Acrobat SDK smoke evidence JSON for Windows/macOS release gates.

Usage:
  python3 tools/validate_sdk_smoke.py --evidence docs/sdk_smoke_evidence.json
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


REQUIRED_ROWS = (
    ("Windows 11 23H2", "x64"),
    ("macOS 14 Sonoma", "arm64"),
    ("macOS 14 Sonoma", "x64 (Rosetta/Intel)"),
)

REQUIRED_CHECKS = (
    "plugin_load",
    "menu_actions",
    "preset_validate",
    "preset_preview",
    "preset_run_bundle",
    "runtime_quality_gate",
    "imposed_output_open",
    "panel_quick_actions",
)


def _normalize(value: str) -> str:
    return value.strip().lower()


def _is_pass(value: str) -> bool:
    return _normalize(value) in {"pass", "ok", "true", "1"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", required=True, help="Path to smoke evidence JSON.")
    args = parser.parse_args()

    evidence_path = Path(args.evidence)
    if not evidence_path.exists():
        print(f"ERROR: evidence file not found: {evidence_path}", file=sys.stderr)
        return 2

    data = json.loads(evidence_path.read_text(encoding="utf-8"))
    rows = data.get("rows", [])
    if not isinstance(rows, list):
        print("ERROR: JSON must contain a list field 'rows'.", file=sys.stderr)
        return 2

    failed = []
    for os_name, cpu in REQUIRED_ROWS:
        row = next(
            (
                item for item in rows
                if _normalize(str(item.get("os", ""))) == _normalize(os_name)
                and _normalize(str(item.get("cpu", ""))) == _normalize(cpu)
            ),
            None,
        )
        if row is None:
            failed.append(f"Missing matrix row: {os_name} / {cpu}")
            continue

        for check in REQUIRED_CHECKS:
            value = str(row.get(check, ""))
            if not _is_pass(value):
                failed.append(f"{os_name} / {cpu}: {check}={value!r} (expected pass)")

        for artifact_key in ("bundle_path", "proof_pdf_path", "imposed_output_path"):
            artifact_value = str(row.get(artifact_key, "")).strip()
            if artifact_value == "":
                failed.append(f"{os_name} / {cpu}: missing evidence field {artifact_key}")

    if failed:
        print("SDK smoke release gate: FAILED")
        for issue in failed:
            print(f"- {issue}")
        return 1

    print("SDK smoke release gate: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
