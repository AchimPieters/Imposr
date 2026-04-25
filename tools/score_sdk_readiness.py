#!/usr/bin/env python3
"""Compute an objective readiness percentage for SDK host smoke evidence."""

from __future__ import annotations

import argparse
import json
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

REQUIRED_METADATA = (
    "acrobat_version",
    "sdk_version",
    "run_timestamp_utc",
)

REQUIRED_ARTIFACTS = (
    "bundle_path",
    "proof_pdf_path",
    "imposed_output_path",
    "preflight_json_path",
    "sdk_ops_path",
    "control_surface_path",
)


def _normalize(value: str) -> str:
    return value.strip().lower()


def _is_pass(value: str) -> bool:
    return _normalize(value) in {"pass", "ok", "true", "1"}


def _is_nonempty_and_not_tbd(value: str) -> bool:
    normalized = value.strip().lower()
    return normalized not in {"", "tbd", "vul-hier-versie-in", "vul-hier-sdk-in", "fill-me", "placeholder"}


def _row_looks_mock(row: dict) -> bool:
    for key in ("acrobat_version", "sdk_version", "execution_mode", "notes"):
        value = str(row.get(key, "")).strip().lower()
        if "mock" in value or value.startswith("simulated"):
            return True
    notes = str(row.get("notes", "")).strip().lower()
    if "mock" in notes:
        return True
    for key in REQUIRED_ARTIFACTS:
        value = str(row.get(key, "")).strip().lower()
        if "mock" in value:
            return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--require-100", action="store_true")
    parser.add_argument("--forbid-mock", action="store_true")
    args = parser.parse_args()

    evidence_path = Path(args.evidence)
    if not evidence_path.exists():
        raise SystemExit(f"ERROR: evidence file not found: {evidence_path}")

    data = json.loads(evidence_path.read_text(encoding="utf-8"))
    rows = data.get("rows", [])
    if not isinstance(rows, list):
        raise SystemExit("ERROR: evidence JSON must contain list field 'rows'")

    total_items = 0
    passed_items = 0

    print("SDK readiness score details:")
    for os_name, cpu in REQUIRED_ROWS:
        row = next(
            (
                item
                for item in rows
                if _normalize(str(item.get("os", ""))) == _normalize(os_name)
                and _normalize(str(item.get("cpu", ""))) == _normalize(cpu)
            ),
            None,
        )

        row_total = 1 + len(REQUIRED_CHECKS) + len(REQUIRED_METADATA) + len(REQUIRED_ARTIFACTS)
        row_passed = 0

        if row is not None and _normalize(str(row.get("execution_mode", ""))) == "host-runtime":
            row_passed += 1

        if row is not None:
            if args.forbid_mock and _row_looks_mock(row):
                # Mock rows contribute 0 when strict production scoring is requested.
                total_items += row_total
                passed_items += 0
                print(f"- {os_name} / {cpu}: 0/{row_total} (0.0%) [mock evidence blocked]")
                continue
            for key in REQUIRED_CHECKS:
                if _is_pass(str(row.get(key, ""))):
                    row_passed += 1
            for key in REQUIRED_METADATA:
                if _is_nonempty_and_not_tbd(str(row.get(key, ""))):
                    row_passed += 1
            for key in REQUIRED_ARTIFACTS:
                if str(row.get(key, "")).strip() != "":
                    row_passed += 1

        total_items += row_total
        passed_items += row_passed

        row_pct = (100.0 * row_passed / row_total) if row_total else 0.0
        print(f"- {os_name} / {cpu}: {row_passed}/{row_total} ({row_pct:.1f}%)")

    score = (100.0 * passed_items / total_items) if total_items else 0.0
    print(f"Overall readiness score: {passed_items}/{total_items} ({score:.1f}%)")

    if args.require_100 and passed_items != total_items:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
