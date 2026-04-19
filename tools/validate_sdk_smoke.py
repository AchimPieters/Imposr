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

REQUIRED_METADATA = (
    "execution_mode",
    "acrobat_version",
    "sdk_version",
    "run_timestamp_utc",
)


def _normalize(value: str) -> str:
    return value.strip().lower()


def _is_pass(value: str) -> bool:
    return _normalize(value) in {"pass", "ok", "true", "1"}


def _is_valid_execution_mode(value: str) -> bool:
    normalized = _normalize(value)
    return normalized in {"host-runtime", "simulated-runtime"}


def _is_missing_metadata(value: str) -> bool:
    normalized = _normalize(value)
    return normalized in {"", "tbd", "vul-hier-versie-in", "vul-hier-sdk-in", "fill-me", "placeholder"}


def _row_looks_mock(row: dict) -> bool:
    notes = _normalize(str(row.get("notes", "")))
    if "mock" in notes:
        return True
    for key in (
        "bundle_path",
        "proof_pdf_path",
        "imposed_output_path",
        "preflight_json_path",
        "sdk_ops_path",
        "control_surface_path",
    ):
        value = _normalize(str(row.get(key, "")))
        if "mock" in value:
            return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", required=True, help="Path to smoke evidence JSON.")
    parser.add_argument(
        "--require-host-runtime",
        action="store_true",
        help="Fail validation unless every matrix row has execution_mode=host-runtime.",
    )
    parser.add_argument(
        "--forbid-mock",
        action="store_true",
        help="Fail validation when row notes/paths indicate mock evidence.",
    )
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

        for metadata_key in REQUIRED_METADATA:
            metadata_value = str(row.get(metadata_key, "")).strip()
            if _is_missing_metadata(metadata_value):
                failed.append(f"{os_name} / {cpu}: missing metadata field {metadata_key}")
        execution_mode = str(row.get("execution_mode", ""))
        if not _is_valid_execution_mode(execution_mode):
            failed.append(
                f"{os_name} / {cpu}: execution_mode={execution_mode!r} "
                "(expected host-runtime or simulated-runtime)"
            )
        elif args.require_host_runtime and _normalize(execution_mode) != "host-runtime":
            failed.append(
                f"{os_name} / {cpu}: execution_mode={execution_mode!r} "
                "(host-runtime required for production gate)"
            )
        if args.forbid_mock and _row_looks_mock(row):
            failed.append(f"{os_name} / {cpu}: mock evidence is not allowed for production gate")

        for artifact_key in (
            "bundle_path",
            "proof_pdf_path",
            "imposed_output_path",
            "preflight_json_path",
            "sdk_ops_path",
            "control_surface_path",
        ):
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
