#!/usr/bin/env python3
"""Bulk-apply host runtime pass results for all platform rows from a single JSON input."""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

PASS_KEYS = (
    "plugin_load",
    "menu_actions",
    "preset_validate",
    "preset_preview",
    "preset_run_bundle",
    "runtime_quality_gate",
    "imposed_output_open",
    "panel_quick_actions",
)

REQUIRED_FIELDS = (
    "os",
    "cpu",
    "acrobat_version",
    "sdk_version",
    "bundle_path",
    "proof_pdf_path",
    "imposed_output_path",
    "preflight_json_path",
    "sdk_ops_path",
    "control_surface_path",
)


def _load_json(path: Path) -> dict:
    if not path.exists():
        raise SystemExit(f"ERROR: file not found: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def _find_row(rows: list[dict], os_name: str, cpu: str) -> dict | None:
    for row in rows:
        if str(row.get("os", "")) == os_name and str(row.get("cpu", "")) == cpu:
            return row
    return None


def _ensure_fields(entry: dict) -> None:
    missing = [field for field in REQUIRED_FIELDS if str(entry.get(field, "")).strip() == ""]
    if missing:
        raise SystemExit(f"ERROR: input row missing required fields: {', '.join(missing)}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", default="docs/sdk_smoke_evidence.json")
    parser.add_argument("--input", required=True, help="JSON file with rows[] entries to mark as pass")
    parser.add_argument("--verify-files", action="store_true", help="Require all artifact paths to exist on disk")
    args = parser.parse_args()

    evidence_path = Path(args.evidence)
    evidence = _load_json(evidence_path)
    evidence_rows = evidence.get("rows", [])
    if not isinstance(evidence_rows, list):
        raise SystemExit("ERROR: evidence JSON must contain list field 'rows'")

    fill_data = _load_json(Path(args.input))
    fill_rows = fill_data.get("rows", [])
    if not isinstance(fill_rows, list) or not fill_rows:
        raise SystemExit("ERROR: input JSON must contain non-empty list field 'rows'")

    timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    for entry in fill_rows:
        _ensure_fields(entry)
        target = _find_row(evidence_rows, str(entry["os"]), str(entry["cpu"]))
        if target is None:
            raise SystemExit(f"ERROR: target row not found in evidence for {entry['os']} / {entry['cpu']}")

        if args.verify_files:
            for field in (
                "bundle_path",
                "proof_pdf_path",
                "imposed_output_path",
                "preflight_json_path",
                "sdk_ops_path",
                "control_surface_path",
            ):
                if not Path(str(entry[field])).exists():
                    raise SystemExit(f"ERROR: artifact path does not exist ({field}): {entry[field]}")

        target["execution_mode"] = "host-runtime"
        target["acrobat_version"] = str(entry["acrobat_version"])
        target["sdk_version"] = str(entry["sdk_version"])
        target["run_timestamp_utc"] = timestamp

        for key in PASS_KEYS:
            target[key] = "pass"

        target["bundle_path"] = str(entry["bundle_path"])
        target["proof_pdf_path"] = str(entry["proof_pdf_path"])
        target["imposed_output_path"] = str(entry["imposed_output_path"])
        target["preflight_json_path"] = str(entry["preflight_json_path"])
        target["sdk_ops_path"] = str(entry["sdk_ops_path"])
        target["control_surface_path"] = str(entry["control_surface_path"])
        if str(entry.get("notes", "")).strip() != "":
            target["notes"] = str(entry["notes"])

    evidence_path.write_text(json.dumps(evidence, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Updated evidence with {len(fill_rows)} row(s): {evidence_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
